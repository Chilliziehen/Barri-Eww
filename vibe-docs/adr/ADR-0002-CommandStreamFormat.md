# ADR-0002: 命令流 IR 格式 (BECS — Barri-Eww Command Stream)

- **状态**: Accepted (2026-07-19，所有者批准 D1–D6 全部条款)
- **决策者**: 项目所有者 (Chilliziehen)、Claude
- **关系**: 落实 [ADR-0001](ADR-0001-RdgRuntimeArchitecture.md) 决策 2 (命令流 IR 边界)；
  Follow-up "定义命令流 opcode 格式" 的答卷。
- **相关规约**: §6 (FFM ABI 集中承载)、§8 (产物校验/受控扩展)、T0[1]/[3]。
- **语义基准**: **Vulkan 1.3 core** (所有者指定)。参考文档为本地
  `D:\Repositories\ComputerGraphics\documents\vkspec\Vulkan-Doc` (统一版 spec，
  checkout 为 1.4.347 / 2026-03-20，完整含 1.3 core 定义；引用章节以文件名标注)。

---

## Context

ADR-0001 确定：Java 编译器产出命令流写入 `MemorySegment`，C++ 回放为
`VkCommandBuffer`；FFM 跨界 O(lane)。本 ADR 定义该流的**物理编码、容器布局、
opcode 空间与校验/版本策略** —— 它是 Java↔C++ 的**版本化 ABI**，也是 §6 的集中落点。

与 MC 26.2 基线的关系：MC 要求 Vulkan 1.2 + dynamic rendering + push descriptors。
以 1.3 core 为语义基准恰与其对齐 —— 1.3 将 dynamic rendering
(`chapters/renderpass.adoc`) 与 synchronization2 (`chapters/synchronization.adoc`)
收编为 core；在严格 1.2+扩展 驱动上，回放器按 KHR 等价物 1:1 映射即可
(IR 层不感知，仅回放器实现细节)。push descriptors 在 1.3 仍为
`VK_KHR_push_descriptor` (1.4 才 core，`chapters/descriptorsets.adoc`)，MC 已强制
其存在，故直接入 Vulkan opcode 区段。

## Decision

### D1. 物理编码：小端、8 字节对齐、定长字段

- **固定小端 (little-endian)**。所有目标平台 (x86_64 / AArch64 的
  Windows/Linux/macOS) 均为 LE，两侧零字节序转换；Java 侧 `VarHandle` 显式指定
  `ByteOrder.LITTLE_ENDIAN`。
- **一切字段自然对齐；每条命令总长为 8 的倍数**。理由：Java aligned `VarHandle`
  访问要求自然对齐 (未对齐访问慢或非法)；C++ 侧可直接 `reinterpret` 读取。
- **无变长整数、无内嵌指针**。定长字段 + 编译期常量偏移 ⇒ JIT 生成的
  `recordLaneN()` 是纯常量偏移写入序列 (T0[1])。

### D2. 双层结构：模块容器 + lane 命令流

**BECS 模块 (bake 产物)** = 节区容器，一次交付给 C++ (慢路径)：

| 节区 | 内容 |
| ---- | ---- |
| `SectionDirectory` | 节区 id → offset/size 目录 |
| `HandleTables` | 分类型槽表：pipeline / buffer / image / imageView / sampler |
| `BarrierBatchTable` | 编译期 barrier pass 解算出的屏障批 (烘焙为 `VkDependencyInfo` 模板；sync2 语义) |
| `RenderingTemplateTable` | dynamic rendering 的 attachment/loadOp/storeOp/clear 模板 (烘焙为 `VkRenderingInfo`) |
| `PushDescriptorTemplateTable` | push descriptor 写入模板 |
| `LaneStream[N]` | 每 lane 一段命令流 (格式见 D3)，互不引用 ⇒ THREADED_RECORDING 天然无共享 |

**槽位间接 (slot indirection)：流内不出现任何原始句柄/指针**，只有 `u32` 槽位号，
指向类型化槽表。理由：
1. 流是纯数据，可完整校验/sandbox (§8 哲学延伸)；
2. 后端中立 (T0[3])：同一槽位在 Vulkan 解析为 `VkPipeline`，未来 DX12 解析为
   `ID3D12PipelineState`；
3. 回放热路径 = 一次稠密数组索引 (加载期已把槽表解析为原生句柄数组，T0[1])。

### D3. Lane 流格式

**流头 (32 字节)：**

| 偏移 | 字段 | 类型 |
| ---- | ---- | ---- |
| 0  | magicBytes = "BECS" | `u8[4]` |
| 4  | versionMajor | `u16` |
| 6  | versionMinor | `u16` |
| 8  | laneIndex | `u32` |
| 12 | commandCount | `u32` |
| 16 | totalByteSize | `u64` |
| 24 | graphHash (§8 产物命名一致) | `u64` |

**命令头 (8 字节) + 定长载荷：**

| 偏移 | 字段 | 类型 | 说明 |
| ---- | ---- | ---- | ---- |
| 0 | opcode | `u16` | 见 D4 区段划分 |
| 2 | commandFlags | `u16` | 保留，当前恒 0 |
| 4 | byteSize | `u32` | 含命令头，8 的倍数 ⇒ 支持工具按步长遍历 |

载荷字段布局按 opcode 固定 (附录 A 给出 v0.1 全目录)。示例：

```
Draw (0x0010), byteSize = 24:
  +8  vertexCount   u32        DrawIndexedIndirectCount (0x0014), byteSize = 40:
  +12 instanceCount u32          +8  argumentBufferSlot u32
  +16 firstVertex   u32          +12 countBufferSlot    u32
  +20 firstInstance u32          +16 argumentOffset     u64
                                 +24 countOffset        u64
                                 +32 maxDrawCount       u32
                                 +36 strideBytes        u32
```

### D4. opcode 空间区段化 (T0[3])

| 区段 | 归属 |
| ---- | ---- |
| `0x0000–0x0FFF` | 核心中立 (所有后端必须实现) |
| `0x1000–0x1FFF` | Vulkan 专属 |
| `0x2000–0x2FFF` | DX12 专属 (占位) |
| `0x3000–0x3FFF` | Metal 专属 (占位) |
| `0xF000–0xFFFF` | 受控扩展 opaque 节点 callout (§8.3；DLSS 类用例) |

### D5. 校验与信任策略 (对齐 §8.4)

- **加载期 (慢路径，一次)**：全量结构校验 —— magic/版本、opcode 属已知区段、
  槽位号越界检查、byteSize 链式一致 (Σ = totalByteSize)。类比"classload 前过
  JVM verifier"。
- **回放期 (热路径)**：Release 构建**零检查**，信任已校验产物 (产物由我们的
  编译器生成 + 加载期已验，T0[1])。
- 排障用的回放期逐命令校验，留待编译期开关 **`BARRIEWW_STREAM_VALIDATION`**
  (默认 OFF)；**实现前须先登记 §3.3** (本 ADR 不预登记，遵守"先登记后使用")。

### D6. 版本策略

- `versionMajor` 破坏性变更；`versionMinor` 仅追加 opcode/节区。
- 回放器只接受**精确匹配的 major**；mismatch = 加载期错误 (慢路径异常，Java 侧转译)。
- 头部 `graphHash` 与 §8 生成类命名 (`…$<graphHash>`) 同源，产物三件套
  (字节码 / BECS 模块 / 槽表内容) 绑定同一哈希。

## 附录 A: v0.1 opcode 目录 (初始集)

核心区段 (载荷字段全部语义完整命名；对应 Vulkan 1.3 入口按 `chapters/*.adoc` 标注)：

| opcode | 名称 | 载荷摘要 | Vulkan 1.3 对应 |
| ------ | ---- | -------- | --------------- |
| 0x0001 | BindGraphicsPipeline | pipelineSlot | `vkCmdBindPipeline` (pipelines.adoc) |
| 0x0002 | BindComputePipeline | pipelineSlot | 同上 (COMPUTE) |
| 0x0003 | BindVertexBuffers | firstBinding, bindingCount, N×{bufferSlot, bindOffset} | `vkCmdBindVertexBuffers` (fxvertex.adoc) |
| 0x0004 | BindIndexBuffer | bufferSlot, indexType, bufferOffset | `vkCmdBindIndexBuffer` (fxvertex.adoc) |
| 0x0005 | PushConstants | shaderStageMask, byteOffset, byteCount, 内联数据 | `vkCmdPushConstants` (descriptorsets.adoc) |
| 0x0006 | SetViewport / 0x0007 SetScissor | 视口/裁剪矩形内联 | `vkCmdSetViewport/Scissor` (fragops.adoc) |
| 0x0010 | Draw | 见 D3 示例 | `vkCmdDraw` (drawing.adoc) |
| 0x0011 | DrawIndexed | indexCount, instanceCount, firstIndex, vertexOffset, firstInstance | `vkCmdDrawIndexed` |
| 0x0012/13 | DrawIndirect / DrawIndexedIndirect | bufferSlot, bufferOffset, drawCount, strideBytes | `vkCmdDraw*Indirect` |
| 0x0014 | DrawIndexedIndirectCount | 见 D3 示例 (**GPU-driven 基石**) | `vkCmdDrawIndexedIndirectCount` (drawing.adoc, 1.2+ core) |
| 0x0020 | Dispatch | groupCountX/Y/Z | `vkCmdDispatch` (dispatch.adoc) |
| 0x0021 | DispatchIndirect | bufferSlot, bufferOffset | `vkCmdDispatchIndirect` |
| 0x0030 | BeginRendering | renderingTemplateSlot | `vkCmdBeginRendering` (renderpass.adoc, 1.3 core) |
| 0x0031 | EndRendering | — | `vkCmdEndRendering` |
| 0x0032 | ExecuteBarrierBatch | barrierBatchSlot | `vkCmdPipelineBarrier2` (synchronization.adoc, sync2 语义) |
| 0x0040 | CopyBuffer | srcBufferSlot, dstBufferSlot, srcOffset, dstOffset, byteCount | `vkCmdCopyBuffer` (copies.adoc) |
| 0x0041/42 | CopyImage / BlitImage | src/dst imageSlot + 单 region 内联 (多 region = 重复命令) | `vkCmdCopyImage` / `vkCmdBlitImage` |
| 0x0043/44 | ClearColorImage / ClearDepthStencilImage | imageSlot, clearValue, subresourceRange 内联 | `vkCmdClear*Image` (clears.adoc) |
| 0x0045 | ResolveImage | src/dst imageSlot + region | `vkCmdResolveImage` (copies.adoc) |
| 0x0046/47 | CopyBufferToImage / CopyImageToBuffer | bufferSlot, imageSlot, region | `vkCmdCopy*` |

Vulkan 区段：

| opcode | 名称 | 说明 |
| ------ | ---- | ---- |
| 0x1000 | PushDescriptorSet | pushDescriptorTemplateSlot；`vkCmdPushDescriptorSetKHR` (MC 26.2 强制扩展；1.4 core) |
| 0x1001/02 | DebugLabelBegin / DebugLabelEnd | 内联 UTF-8；仅 debug bake 产出 (Java bake 期决定，Release 流中零存在) |

受控扩展区段：

| opcode | 名称 | 说明 |
| ------ | ---- | ---- |
| 0xF000 | InvokeOpaqueExtensionNode | extensionNodeTypeSlot + 定长参数块；信任边界见 §8.3 |

## Consequences

- **测试性大幅提升 (§4)**：C++ 回放器可用手工构造的 BECS 流独立测试 (Catch2，
  无需 JVM)；Java 写侧对 golden 字节转储断言 (JUnit)；同一份 golden 文件做跨语言
  一致性测试。集成测试的"跨语言链路"因此可分层定位故障。
- 新增维护物：opcode 目录与版本策略 (追加走 minor，纪律成本低)。
- 反汇编器 (BECS → 可读文本) 是廉价的排障工具，列为后续项。
- 若未来 pass 需要运行时分支 (permutation)，在流层面表达为"多段流 + Java 选段
  提交"，不在流内加控制流 opcode —— 流保持直线 (T0[1] 与 ADR-0001 一致)。

## Follow-ups

- 实现启动时：将本格式提升为规范性文档 (spec §9 候选)，走 ProposedExtensions。
- 实现回放期校验前：登记 `BARRIEWW_STREAM_VALIDATION` 入 §3.3。
- BECS 反汇编工具 (test/tooling 分支)。
- ADR-0003 候选：device 所有权 (待 MC 26.2 Blaze3D 句柄可得性 spike，见 ADR-0001 OPEN)。
