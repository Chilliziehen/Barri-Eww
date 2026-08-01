# 规约扩充建议 (台账)

> 本文件收录 AI/协作者提出的**规约扩充建议**及其裁决。`ACCEPTED` 者已提升为正式条款
> 并迁入对应细分文档，保留在此仅作演进记录 (另见 [CHANGELOG.md](CHANGELOG.md))。
> `PROPOSED` 者仍待所有者裁决，不构成对已固化规约的约束。

---

## 已裁决 (ACCEPTED — 已迁入正式文档)

| 编号 | 内容                                              | 落地位置                                   |
| ---- | ------------------------------------------------- | ------------------------------------------ |
| P1   | C++ 标准锁定 **C++23**                            | spec.md 模块表 / BuildSystem §3.1          |
| P2   | 编译期开关前缀统一 **`BARRIEWW_`**                | BuildSystem §3.3                           |
| P3   | 代码注释**全英文** (文档中文)                     | CommentConvention §2.1                      |
| P4   | Panama/FFM 绑定层专项规约                          | §6 FfmBinding.md                           |
| P5   | 错误处理与日志规约 (+ 编译期日志开关)             | §7 ErrorHandlingAndLogging.md              |
| P6   | 分支类别扩充 (docs/test/refactor/hotfix)          | VersionControl §5.2                         |
| P7a  | 测试框架 Catch2 / JUnit 5 / Vitest                | Testing §4.1                               |
| P7b  | CI 矩阵 OS×后端×开关 + 软件光栅集成测试           | Testing §4.4                               |
| P8   | 生成产物命名与 ClassLoader / 受控扩展约定         | §8 GeneratedArtifacts.md                   |
| P9   | 规约变更记录 (CHANGELOG)                          | CHANGELOG.md                               |
| P10  | Editor **直接采用 TypeScript**                    | spec.md / §1T CodingConventionTs.md        |
| P11  | 覆盖率门槛 C++/Java ≥ 90%、TS ≥ 70%               | Testing §4.5                               |
| P12  | §1.1.2 豁免 Vulkan 惯用语 `Info`/`CreateInfo` (限镜像 API 概念) | CodingConvention §1.1.2       |
| P13  | 简单访问器精简注释 (类级总括 + 单行说明)          | CommentConvention §2.6                     |
| P14  | ImageViewHandleTable v0.1 ABI + BECS staged integration gate | CommandStreamFormat §9.2 / Testing §4.2.1 |
| P15  | 全量 BECS ABI 规范固化 (流头/命令头/模块容器/全部 handle·barrier·template 表/命令 opcode 载荷/中立编码枚举) | CommandStreamFormat §9.3–§9.13 |
| P16  | GraphicsPipelineTable v0.1 (section type 0x0007, 80B 定长记录, 动态渲染管线; BindGraphicsPipeline/SetViewport/SetScissor 载荷; 拓扑/比较/剔除/绕向中立编码) | CommandStreamFormat §9.14 / §9.11 / §9.12 |
| P17  | DrawIndirect 载荷固化 (GPU-driven 绘制; v0.1 限 drawCount==1/stride==16, 多重间接绘制留待 additive 增量) | CommandStreamFormat §9.11 |
| P27  | GraphOutputTable v0.1 (section type 0x0008)：图与 presentation 的单一交接契约——每帧槽一张 RDG 拥有的 output image、final layout、format/extent 一致性；swapchain image 与宿主 GUI 纹理不进 BECS | CommandStreamFormat §9.17 / §9.16 / ADR-0006 D3 |
| P19  | 首个 FFM 校验边界（Core 归属、JDK 25 critical API、Version 1 C ABI、真实 Java→Native 校验门禁） | BuildSystem §3.6 / Testing §4.2.1 / §6 / §7.1.2 / §8 |
| P20  | 根构建入口 CLI（跨平台同构参数、变体透传、模块顺序、fail-fast 语义） | BuildSystem §3.5 |
| P21  | Java-owned Vulkan bootstrap / Native-owned presentation execution FFM Version 1：opaque runtime/module owners、frame status、lifetime | FfmBinding §6.7 / ADR-0004 |
| P22  | ImportedImageBinding v0.1：按 importIdentifier 绑定 borrowed image、metadata compatibility 与 generation replacement | CommandStreamFormat §9.15 / ADR-0004 |
| P23  | FrameMetricsVersion 1 与 `BARRIEWW_GPU_FRAME_METRICS`：completed-frame CPU/GPU 分段指标 | BuildSystem §3.3 / FfmBinding §6.7.3 / ADR-0004 |
| P24  | Pure Compute visible CopyBufferToImage：64-byte payload、tight-packed range 与 swapchain TRANSFER_DST 规则 | CommandStreamFormat §9.11 / §9.16 / ADR-0004 |
| —    | `Editor/` 技术栈 Node.js/Electron                 | spec.md / §1T                              |
| —    | 两级合并流 `功能 → dev → master` + 全 CI          | VersionControl §5.4                         |
| —    | 新增开关 `BARRIEWW_CRITICAL_HOTPATH_LOG`          | BuildSystem §3.3 / §7.2.1 (所有者补充)      |

T0 三指标仲裁顺序 `性能 > 兼容性 > 可维护性` 已确认 (spec.md T0)。

---

## 待裁决 (PROPOSED)

| 编号 | 建议                                                                                                     | 目标条款                          |
| ---- | -------------------------------------------------------------------------------------------------------- | --------------------------------- |
| P25  | **能力清单 (Capability Manifest) 产出物** `PROPOSED`：装载期枚举 MC `ENVIRONMENT_ATTRIBUTE` / `ATTRIBUTE_TYPE` 注册表导出的机器可读数据源清单（属性 id、值类型、默认值、range、是否空间插值/同步 + 我方内建源），供 Editor 作数据调色板。需定义命名、序列化格式与版本策略 | GeneratedArtifacts §8 / ADR-0005 D1 |
| P26  | **参数 DSL 与 frame-slot 参数块 ABI** `PROPOSED`：受限 DSL 文法与内建函数集、值类型到 MC `AttributeType` 的显式映射（含 RGB_COLOR 等打包类型）、参数块 schema 与编译期偏移分配规则 | CodingConvention §1 / CommandStreamFormat §9 / ADR-0005 D2·D4 |
| P28  | **Host-image presentation additive Version 1 ABI** `PROPOSED`：新增 presentation 中立 UNORM format、96-byte host-image create record、独立 create/submit symbols；保留既有 Version 1 record 与 symbol 的二进制兼容，固定 borrowed host image ownership、exact extent、requested surface format 与 non-critical downcall 语义 | FfmBinding §6.7 / ADR-0006 D5·D6·D8·D9.2 |

> ADR-0005 D5 另记录两项**明确推迟、非放弃**的方向，落地时须先在此登记并裁决：
> Tier 2 外部声明数据源 (D5.1)、B 类图结构决策 (D5.2)。

新的扩充建议在此登记，标 `PROPOSED`，经所有者确认后迁入对应正式文档并在 CHANGELOG 记录。

### P28 提案明细：Host-image presentation additive Version 1 ABI

`PresentationImageFormat` 使用稳定值 `R8G8B8A8Unorm = 1`、`B8G8R8A8Unorm = 2`。
该枚举只属于 presentation interoperability；即使与 BECS 中立 format 数值一致，也不使
宿主 image 进入 BECS 或 P22。

新增 `NativeHostImagePresentationRuntimeCreateInfoVersion1`，布局固定为：

| offset | 字段 | 类型 |
| ------ | ---- | ---- |
| 0 | `instanceHandle` | `uint64` |
| 8 | `physicalDeviceHandle` | `uint64` |
| 16 | `logicalDeviceHandle` | `uint64` |
| 24 | `surfaceHandle` | `uint64` |
| 32 | `graphicsQueueHandle` | `uint64` |
| 40 | `presentQueueHandle` | `uint64` |
| 48 | `graphicsQueueFamilyIndex` | `uint32` |
| 52 | `presentQueueFamilyIndex` | `uint32` |
| 56 | `framebufferWidth` | `uint32` |
| 60 | `framebufferHeight` | `uint32` |
| 64 | `framesInFlightCount` | `uint32` |
| 68 | `reservedFlags`（必须为 0） | `uint32` |
| 72 | `hostImageHandle` | `uint64` |
| 80 | `hostImageFormatValue` | `uint32` |
| 84 | `requestedSurfaceFormatValue` | `uint32` |
| 88 | `hostImageWidth` | `uint32` |
| 92 | `hostImageHeight` | `uint32` |

该 record 固定 `sizeof == 96`、`alignof == 8`，以双侧 layout assertions 钉死。

新增两个完整命名、additive、non-critical 的 Version 1 C symbols：

```cpp
NativePresentationRuntimeOperationResult
barriEwwCreateHostImagePresentationRuntimeVersion1(
    const NativeHostImagePresentationRuntimeCreateInfoVersion1* createInfo,
    NativePresentationRuntimeCreateResultVersion1* createResult) noexcept;

NativePresentationRuntimeOperationResult
barriEwwSubmitAndPresentHostImageFrameVersion1(
    std::uint64_t runtimeAddress,
    NativePresentationSubmitFrameResultVersion1* submitResult) noexcept;
```

固定语义：

1. 旧 `NativePresentationRuntimeCreateInfoVersion1`、所有旧 symbol 与 status record 原样保留；
   禁止复用旧 `reservedFlags` 传递 host-image 数据。
2. Native 借用 host `VkImage`，不拥有 image 或 memory；Native 只拥有为 sampled presentation
   创建的 view、descriptor、pipeline 与 command buffers，且必须在宿主销毁 image 前退休。
3. `hostImageHandle` 非零；host、framebuffer 与实际 swapchain extent 必须 exact match；format
   必须为上述中立 UNORM 值。
4. `requestedSurfaceFormatValue` 必须由 surface 以 `SRGB_NONLINEAR` color space 实际支持；
   不允许静默替换为 SRGB format，也不在 shader 内做色彩空间抵消。
5. create/submit 涉及分配、driver synchronization 与 queue operation，FFM downcall 固定
   non-critical；binding 构造期解析一次 handle，frame path 不做 symbol lookup。
6. submit 要求已有 open frame，并以该 frame 的固定 slot/image index 选择预录 command；
   operation/result/exception containment 与既有 presentation Version 1 规则一致。
