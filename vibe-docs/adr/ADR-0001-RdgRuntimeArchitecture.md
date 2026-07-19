# ADR-0001: RDG 运行时架构 —— 阶段划分、命令流 IR、GPU-Driven

- **状态**: Accepted (2026-07-19)
- **决策者**: 项目所有者 (Chilliziehen)、Claude
- **关系**: 细化 [`../claude/project-brief.md`](../claude/project-brief.md) 的核心架构判断[4]
  (原文暗示"Java 逐条 `vkCmd*` 走 trivial downcall"，本 ADR 将 FFM 粒度改为按 lane)。
- **相关规约**: §6 [FfmBinding](../spec/FfmBinding.md)、§7、§8、T0[1]/[2]/[3]。

> ADR 记录一次性的架构决策及其理由。若后续推翻/修订，新增 ADR 并在此标注 Superseded-by。

---

## Context

project-brief 已确定：图 = 纯数据、JIT 把图编译为 Java 字节码驱动 C++ Vulkan 后端、
Java↔Native 走 Panama/FFM。但**留白**了一个关键问题：RDG 的"构建 / 指令录制 / 播放"
分别落在 Java 还是 C++，以及 FFM 的调用粒度。这直接影响 T0[1] (性能)、T0[3] (多后端
兼容)、T0[2] (可维护)。

## Decision

### 1. 按"编译期 vs 运行时"划分阶段 (而非按语言整体切分)

| 阶段 | 职责 | 热路径 | 落点 |
| ---- | ---- | ------ | ---- |
| 编译 | JSON 图 → SSA → 死节点消除 → barrier 插入 → 生命周期/别名 → lane 分区 | 否 (每图一次) | **Java** |
| 录制 | 将编译结果落为 `vkCmd*` | **是** | **拆分** (见 2) |
| 提交/同步 | queue submit、fence、semaphore、swapchain | 阻塞 | **C++** |
| Vk 对象创建 | pipeline/资源创建 | 否 (init) | **C++** |

编译留在 Java：这是本项目相对 UE RDG 的差异化点 (JIT 特化)，且非热路径。

### 2. 录制经"命令流 IR"边界，而非逐条 FFM downcall

- Java 编译产物的 `recordLaneN()` 通过 `VarHandle` 把一段**紧凑命令流**写入预分配的
  `MemorySegment`（**每条命令无 FFM 调用**，纯内存写，GC-free、可内联）。
- 每 lane/帧一次 `critical` downcall 把该段交给 C++；C++ 将扁平命令流**回放**为
  `VkCommandBuffer`（opcode → `vkCmd*`）。
- **FFM 跨界从 O(命令数) 降为 O(lane 数)。**

命令流 IR 是**版本化的 Java↔C++ ABI**（集中承载 §6）。它不是图解释器：命令流已被 Java
编译器扁平化、barrier 解算、lane 分区，C++ 只回放"纸带"——直线、分支可预测、无查表
(T0[1] 洁净)。opcode **后端中立**，允许后端专属 opcode 区段 → 多后端 (T0[3]) 优于把
`vkCmd*` 焊死进字节码。

### 3. GPU-Driven，性能优先 (所有者裁决)

- bindless + 间接绘制 (`vkCmdDrawIndexedIndirectCount`) + GPU 计算 culling 产出间接参数。
- 静态拓扑用**一次录制、可复用**的 command buffer；每帧热路径 = Java 把动态数据
  (矩阵/uniform/间接计数) 写入**持久映射**的 `MemorySegment`（零逐值 FFM）+ 少量 submit
  downcall。**命令流因此主要是 bake 期 IR**；仅真正动态的 pass 每帧重录。**每帧 FFM ≈ O(1)**。

**净角色**: Java = 编译器 + 每帧*数据*生产者（非命令生产者）；C++ = bindless/间接
Vulkan 运行时 + 同步。逐对象工作交给 GPU。

## Consequences

- **MC 26.2 强制 dynamic rendering** ⇒ 无 render pass/subpass ⇒ vk-mirage 的 Granite 式
  **subpass 合并** (`RGPhysicalPass`) 不可复用；barrier 插入/别名/lane 分区仍可作为
  Java 中端的参考。
- bindless 在 Vulkan 1.2 基线可行 (descriptor indexing 为 1.2 核心)。
- Native **不拥有窗口系统**（MC 拥有 surface）；Native **接收** Vulkan device/queue
  (依赖注入，借用所有权) —— 见 Integration。
- 新增需维护的产物：命令流 opcode 格式 (定义 + 版本化)。

## Integration with Minecraft (参考实现)

- **VulkanMod** (`ref/VulkanMod`，Java+LWJGL)：通过 Blaze3D mixin 以 Vulkan 取代 GL。
  其 `VRenderSystem` 是 **Blaze3D 状态/uniform 面**——矩阵 (modelView/proj/MVP/texture)、
  管线状态 (depth/cull/topology/colorMask)、fog、光方向、screen/texture size——全部存于
  **mapped buffer**。这正是我们的数据接口需暴露的每帧数据清单，且印证 mapped-`MemorySegment`
  方案。**Blaze3D** (MC 的 `GpuDevice`/`GpuBuffer`/`GpuTextureView` 抽象) 作为数据接口参考。
- **OPEN（device bring-up 前需调研的决策）**：Barri-Eww 是**共享 MC 26.2 现有 `VkDevice`**
  (经 Blaze3D `GpuDevice` interop) 还是**自建** (LWJGL/native)？倾向共享以避免第二个 device，
  但取决于 MC 是否暴露原始句柄。**Native 被设计为无论哪种都"接收" device/queue**，故该决策
  不阻塞 context 端口开发。

## Follow-ups

- 定义命令流 opcode 格式（后续 ADR）。
- Spike：调研 MC 26.2 是否可获取其 `VkDevice`/队列句柄（决定 device 所有权）。
