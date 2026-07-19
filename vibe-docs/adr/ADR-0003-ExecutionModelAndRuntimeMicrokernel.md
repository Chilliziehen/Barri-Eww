# ADR-0003: 执行模型 —— 编译分级与运行时微内核

- **状态**: Accepted (2026-07-20，所有者确认)
- **决策者**: 项目所有者 (Chilliziehen)、Claude
- **关系**: 细化 [ADR-0001](ADR-0001-RdgRuntimeArchitecture.md) (阶段划分) 与
  [ADR-0002](ADR-0002-CommandStreamFormat.md) (BECS 定位)；不推翻任何既有决策，
  但**收紧其中的措辞与残留自由度**。
- **来源**: 所有者设计意图澄清 (2026-07-20)。

---

## Context

常规 RDG 系统 (UE RDG、Granite、vk-mirage 原型) 的形态是：解析图 → 拓扑排序成
执行流 → **一个常驻运行时每帧读取并维护该执行流** (`for each pass: execute(pass)`)。
执行流是运行时的数据，"下一步做什么"由运行时读数据决定。

本项目的立场 (所有者原则)：**凡编译期可确定者，不得在执行期再确定。** 图是静态的，
因此图的一切派生物 (顺序、barrier、生命周期、lane 划分) 都应编译为**直线代码**，
执行期不存在图、不存在 pass 循环、不存在调度器：

```
[常规 Runtime]                      [本项目 Renderer (生成的直线字节码)]
for each pass p in stream:          executeGeometryPass();
    execute(p)                      executeLightingPass();
                                    executePostProcessPass();
                                    executeUserInterfacePass();
                                    // present 由微内核帧循环完成 (见 D3)
```

运行时不是被消灭，而是**微内核化**：只保留"只有运行时能做的事"。

## Decision

### D1. 编译分级表 (每个决定固定在其输入最早可用的阶段)

| 层级 | 决定内容 | 固定时机 | 运行期形态 |
| ---- | -------- | -------- | ---------- |
| 图 | 拓扑/顺序/barrier/别名/lane 划分 | **图编译期** | **不存在** (已蒸发为直线字节码；无 pass 循环、无调度器) |
| 命令序列 | 每 lane 的 `vkCmd*` 序列 | **bake→load 期** | **预录 `VkCommandBuffer`**；每帧成本 = submit |
| 每帧参数 | 矩阵/uniform/间接计数 | 每帧 (数值本身动态) | 直线字节码写**持久映射 MemorySegment** + GPU indirect；**序列固定，仅值变** |
| 拓扑变体 | permutation/条件分支 | **编译期全枚举** | 多份预烤产物，运行期**只做选择**，绝不构造 |

> 关键洞察：预录 `VkCommandBuffer` 才是 RDG 在 Vulkan 语境下真正的"原生代码"——
> 即使是 C++ 直线 `execute(...)` 序列，每帧仍要重发全部 `vkCmd*`；预录 buffer 连
> 这笔 CPU 成本也免掉，只剩 submit。本项目取后者，比示意图更进一步。

### D2. BECS 的定位收紧 (对 ADR-0002 的措辞修正)

- BECS 流是**编译器输出的跨语言运输格式**，消费只发生在**加载期**
  (校验 → 录制为 `VkCommandBuffer`)。**每帧路径不读任何 BECS 字节。**
- **命名约束**：消费侧组件用 **Recorder / Loader** 语汇 (如
  `CommandBufferRecorder`)，禁用 "Replayer / Interpreter" 语汇——名字会诱导后来者
  把它写成每帧解释器。(ADR-0002 中的"回放器"一词自本 ADR 起按此理解。)
- **动态 pass 三级策略** (对 ADR-0001 "动态 pass 每帧重录" 的收紧，按优先序)：
  1. **参数间接化**：序列不变，值走 mapped memory / indirect buffer (首选，覆盖绝大多数"动态")；
  2. **预烤变体选择**：序列有限变化 → 编译期枚举，运行期选段提交；
  3. **每帧重录** (最后手段，如 UI 这类几何数量每帧变化的 pass)：也只是
     "直线 Java 写 + 一次 native 录制调用"，录制路径依然无图逻辑、无查表。

### D3. Native 运行时 = 微内核 (职责白名单)

只有下列职责允许存在于运行时，**白名单之外的职责进入运行时须先过 ADR**：

1. **bindless descriptor heap 驻留管理**——heap *population* 是运行时职责
   (MC 流式加载区块/纹理，"索引 37 此刻是谁"只有运行时知道)；descriptor *layout*
   与槽位分配方案是编译期职责，运行时不做布局决策；
2. **帧循环**：swapchain acquire/present、fence/semaphore 轮转、frames-in-flight
   (image index 只有 acquire 之后才存在，天然运行时)；
3. **queue submit** (提交预录 buffer)；
4. **流式资源驻留**：区块顶点/索引池的分配与回收 (数据内容运行时到达)；
5. **装载**：BECS 校验 + 命令录制、§8 ClassLoader/产物装载配合 (每图一次，慢路径)。

**明确禁止进入运行时**：图遍历、pass 调度、barrier 决策、资源生命周期/别名决策、
descriptor 布局决策——这些的输入在编译期已齐备。

## Consequences

- swapchain 轮转与预录 buffer 的矛盾 → **按 swapchain image 预录 N 份**
  (又一次"编译期枚举 + 运行期选择")，或渲染至离屏 + 预录 blit。
- UI/文本类每帧变几何的 pass → mapped vertex pool + 参数化预录绘制优先；
  重录为最后手段 (D2 策略 3)。
- Profiling/调试钩子不得重新引入每 pass 的运行期分发 (编译期开关注入，§3)。
- 未来任何"在运行时读产物决定行为"的提案，先对照 D1 表找它的最早可固定阶段。

## Follow-ups

- 回放器骨架更名为 **CommandBufferRecorder** 增量 (D2 命名约束的第一个落点)。
- 微内核骨架 (帧循环 + submit) 为 Native 侧下一个大增量的边界依据。
