# 规约变更记录 (SPEC CHANGELOG)

> 记录 `vibe-docs/spec` 正式条款的每次变更 (日期 / 条款 / 理由)，服务长期项目的规约演进回溯。
> 格式：`YYYY-MM-DD` 分组，逐条列 `[新增|修改|删除] 条款 —— 理由`。
> 变更走 `docs/spec-*` 分支 + MR (见 §5)。

---

## 2026-08-02

- [新增] §6.7.4 [FfmBinding.md](FfmBinding.md) —— 所有者裁决并提升 P28：固化
  host-image presentation additive Version 1 ABI。新增 presentation 中立 RGBA/BGRA UNORM
  format、96-byte `NativeHostImagePresentationRuntimeCreateInfoVersion1`、独立 create/detach/submit
  symbols，以及 borrowed host image ownership、exact extent、requested surface format 与
  non-critical downcall 规则；旧 Version 1 record/symbol 原样保留，禁止复用 reserved field，
  从而在不破坏已发布 ABI 的前提下支持 ADR-0006 D9 第 2 步。
- [修改] [ProposedExtensions.md](ProposedExtensions.md) —— P28 状态由 `PROPOSED` 提升为
  `ACCEPTED` 并迁入 §6.7.4。
- [修改] §3.6 [BuildSystem.md](BuildSystem.md) —— 将 shared-library export 范围从过时的
  “仅 §6.6”纠正为“§6 固化的 Versioned C ABI”，覆盖已批准的 §6.7 presentation symbols；
  不增加 symbol 或构建开关。
- [修改] [ADR-0006](../adr/ADR-0006-NativeOwnedMinecraftPresentation.md) D5.4 / D6 / D10.3
  —— 统一 host barrier 为 `MEMORY_WRITE`，并明确 resize HEAD 先 detach imported-host
  resources、保留 clear-capable Native runtime，使 generation replacement 期间仍逐帧输出。
- [修改] §6.7.4 [FfmBinding.md](FfmBinding.md) / [ADR-0006](../adr/ADR-0006-NativeOwnedMinecraftPresentation.md)
  D5.4 —— 依所有者补充裁决，为 P28 增加 idempotent non-critical host-resource detach symbol；
  固定 open-frame fence 排除、clear runtime 保留与 detach 失败时取消完整 host resize 的 ownership
  规则，补齐 D10.3 fallback 所需的跨边界操作。
- [修改] §4.4 [Testing.md](Testing.md) —— 所有者裁决并提升 P29：Native CI 改为分层矩阵，
  Linux Vulkan + lavapipe execution 覆盖 `THREADED_RECORDING` on/off，Windows backend-neutral
  build/test 覆盖同一开关；明确 Windows Vulkan 是未验证 compatibility gap，不再以
  backend-neutral cell 冒充 Vulkan 覆盖。
- [修改] [ProposedExtensions.md](ProposedExtensions.md) —— P29 状态由 `PROPOSED` 提升为
  `ACCEPTED` 并迁入 §4.4。

---

## 2026-07-26

- [新增] [ADR-0005](../adr/ADR-0005-SceneDataAccessAndParameterDsl.md) —— 固化 Minecraft
  场景数据接入与 TA 参数 DSL（所有者裁决 D1–D4）：数据源分级 MVP 只做 Tier 1
  （枚举 `ENVIRONMENT_ATTRIBUTE` / `ATTRIBUTE_TYPE` 注册表自动发现，含第三方模组属性）；
  TA 脚本采用自研受限 DSL 并在烘焙期经 ASM 编译为直线字节码（控制权优先，否决 Lua/
  GraalVM/Java 子集）；TA 逻辑时间域 MVP 只做 A 类参数计算，C 类逐 draw 逻辑永久禁止；
  参数块布局编译期固定。明确推迟 Tier 2 外部声明数据源与 B 类图结构决策（记录于 D5，
  非放弃）。
- [新增] [ProposedExtensions.md](ProposedExtensions.md) —— 登记 P25 能力清单产出物、
  P26 参数 DSL 与参数块 ABI；逐项批准前不得实现。
- [新增] §9.17 [CommandStreamFormat.md](CommandStreamFormat.md) —— GraphOutputTable v0.1
  （所有者裁决 P27）：section type `0x0008`，固化图与 presentation 的**单一交接契约**——
  每帧槽一张由 RDG 分配并拥有的 output color image、图收尾布局、format/extent 一致性与
  usage 校验；每帧槽独立 output 为必需（相邻帧在飞时共用会读写相撞）。
- [修改] §9.4 / §9.16 [CommandStreamFormat.md](CommandStreamFormat.md) —— section 类型清单
  加入 `0x0008`；明确 swapchain image 与宿主 GUI/UI 纹理**不进入 BECS**，由 presentation 侧
  独立持有并发出其固定 barrier 集合，从而使同一张图可在 standalone、Minecraft 宿主与离屏
  测试中原样执行。
- [修改] [ADR-0006](../adr/ADR-0006-NativeOwnedMinecraftPresentation.md) —— 状态
  `Proposed` → **`Accepted`**（所有者逐项裁决 D1、D3–D6、D8–D10）。D8/D9/D10 裁决：
  D8 合成结构固定为一次全屏图形 pass、不对 TA 开放且**不承诺开放机制**，三者 extent 必须
  相等（MVP 不缩放）、不做色彩空间转换，**混合公式依赖 D7 故 D8 在其定案前不完备**；
  D9 验证路径由三步改为**四步**，新增「取消世界 + 纯色背景」一步使 D7 的 alpha 语义
  单独暴露（第 2 步中宿主纹理完全不透明，合成退化为拷贝，不触发 alpha 问题）；
  D10 **订正初稿**——初稿「不满足则保持原版路径」假定了不存在的运行期回退，实际上阻止
  宿主创建 swapchain 是 generation 级一次性决定，接管后原版无 swapchain 可用；现拆为两层：
  presentation 接管为 generation 级且接管后仅支持**延迟回退**（下个 generation 不再接管），
  世界替换为每帧级可自由回退，并保留「已接管即每帧必须有输出」不变式。
  D2（屏蔽手法）留待实现期、D7 留待实测，其余条款为已批准决策。
- [修改] [ADR-0006](../adr/ADR-0006-NativeOwnedMinecraftPresentation.md) D5.3 —— 补强
  保持 GENERAL 的理由：宿主对 main render target 的使用不止 blit，后处理链与截图
  （`GameRenderer.java:234/:431/:473`）同样假定 GENERAL，转走而不转回将使这些路径进入
  未定义行为，且症状与渲染主线索无关、极难定位。
- [修改] [ADR-0006](../adr/ADR-0006-NativeOwnedMinecraftPresentation.md) —— D5 裁决：
  D5.1 仅导入宿主 main render target 的 color texture，不导 depth；D5.2 **订正初稿**——
  宿主 GUI 纹理**不走 P22**（P22 是 BECS imported entry 机制，与 D3「宿主纹理不进 BECS」
  冲突），它只是 presentation 侧的借用句柄，故 presentation 路径**不再以 P22 为前置依赖**；
  D5.3 保持 `VK_IMAGE_LAYOUT_GENERAL` 只发内存 barrier，理由为回退安全（宿主 blit 恒按
  GENERAL 读，转走而不转回将使 D10 回退路径进入未定义行为），并禁止以 `UNDEFINED` 作
  `oldLayout`；D5.4 hook `RenderTarget.resize()` 换代 + `VkImage` 句柄比对兜底；
  D5.5 记录已知限制——取消世界渲染后宿主深度缺失致**手部恒在最前**，玩家嵌入方块时穿模，
  不阻塞 MVP，记录以免日后反复排查。
  Context 补入两项实测：宿主纹理恒为 GENERAL（`VulkanGpuTexture.java:47/62-63`、
  `VulkanGpuSurface.java:366`）、main target color texture 含 `SAMPLED` usage
  （`MainTarget.java:79` + `VulkanConst.textureUsageToVk`），故合成可直接采样。
- [修改] [ADR-0004](../adr/ADR-0004-JavaVulkanPresentationRuntime.md) D4 —— 加入
  2026-07-26 修正记录：图工作由「secondary + `vkCmdExecuteCommands`」改为**多 primary
  同批提交**（ADR-0006 D4.1 裁决）。依据为 `CommandBufferRecorder` 落地后产出的即是
  primary（begin 路径不设 inheritance info），已有 61 个用例建立其上；改用 secondary 需
  改动 recorder 与全部测试夹具而无对应收益。原文保留，以修正记录形式追加，不改写决策轨迹。
- [修改] §9.16 [CommandStreamFormat.md](CommandStreamFormat.md) —— 明确 presentation 侧
  合成/呈现管线为手写实现、不经 BECS，因而**不受 §9.8 push-constants-only 约束**，可自由
  使用 descriptor set；为支持合成而向 BECS 增加 descriptor table 属不必要（ADR-0006 D4.5）。
- [修改] [ADR-0006](../adr/ADR-0006-NativeOwnedMinecraftPresentation.md) —— D4 裁决：
  D4.1 多 primary 同批提交（`submitAndPresentFrame` 参数改为 `std::span<const VkCommandBuffer>`）；
  D4.2 presentation primary 固定构成；D4.3 查询面为装载期一次性、普通 C++ 访问器 +
  `std::span`，依赖方向严格单向（模块不知晓 presentation）；D4.4 合成所需 sampled view
  由 presentation 自建；D4.5 合成必须为图形 pass（blit 无法 alpha 混合）。
- [修改] [ADR-0006](../adr/ADR-0006-NativeOwnedMinecraftPresentation.md) —— D6 裁决为
  **独立提交**：Barri-Eww 自行 `vkQueueSubmit`，完全不注入 Minecraft 的
  `VulkanCommandEncoder`，首版即终局。依据为 `Minecraft.java:1308` 的唯一 `submit()`
  紧邻 `:1310` 的 `present()`，故我方在 present 接管点提交时宿主 GUI 工作已提交完毕，
  仅凭 Vulkan submission-order 语义下的一次 barrier 即可同步，无需任何 semaphore 交互；
  接管点与现有 `beginFrame`/`submitAndPresentFrame` 一一对应，presentation runtime 零改动。
  **推翻初稿建议的方案 α**（注入宿主 encoder）：`VulkanCommandEncoder` 无 `signalFence`，
  走 α 将迫使 frame-slot fence 迁移为 timeline semaphore 而产生双同步路径（D3 已否决的
  双模式问题），并继承宿主 `awaitSubmitCompletion` 的双重节流。α 作为已评估否决方案留档。
  连带记录两项必须查证/处理项：宿主 GUI 纹理渲染后的 image layout（D5，禁止以
  `UNDEFINED` 代替）、`isAcquired()` 门控（D2，不处理将直接黑屏）。D4 标记为须依
  D3/D6 重写并已消除与 D3 的表述冲突。
- [修改] [ADR-0006](../adr/ADR-0006-NativeOwnedMinecraftPresentation.md) —— D3 裁决为单一
  交接点；**订正初稿论证**：原「barrier 必须编译期计算故须进 RDG 体系」不成立（presentation
  的 barrier 是封闭集合，硬编码同样是编译期确定），真实理由是图的可移植性。备选演进方向
  （presentation 图化）明确标注为非既定路径。
- [新增] [ADR-0006](../adr/ADR-0006-NativeOwnedMinecraftPresentation.md) `Proposed`
  —— Native-owned Minecraft presentation：D1 presentation 由 Native 完全接管已裁决
  （否决 Minecraft-hosted，理由为单一模型 + HDR/FG 空间，非"避免图像回传 Java"）；
  其余条款待逐项讨论，含四个接管点、RDG 与 presentation 的资源/帧循环边界、
  合成尾段预录矩阵、P22 导入契约、提交边界方案 α/β、GUI alpha 待实测项与三步验证路径。
  基于 2026-07-26 反编译 MC 26.2 的实测帧流。

---

## 2026-07-23

- [修改] [ProposedExtensions.md](ProposedExtensions.md) / §3.3 / §6.7 / §9.11 / §9.15–§9.16
  —— 所有者裁决并提升 P21–P24：presentation execution FFM、imported image binding、
  completed-frame metrics/GPU compile-time switch、Pure Compute CopyBufferToImage 可视传输。
- [新增] [ADR-0004](../adr/ADR-0004-JavaVulkanPresentationRuntime.md) —— 固化 Java-owned
  LWJGL/GLFW Vulkan bootstrap 与 Native-owned presentation runtime：MAILBOX→FIFO、分族
  CONCURRENT、预录 secondary/primary、recreation、Pure Compute 先行与 completed-frame metrics。
- [新增] [ProposedExtensions.md](ProposedExtensions.md) —— 登记 P21 execution FFM、P22 imported
  image binding、P23 frame metrics/compile-time switch、P24 Pure Compute transfer；逐项批准前不得实现。
- [修改] §3.5 [BuildSystem.md](BuildSystem.md) —— 固化根 `build.sh`/`build.bat` 同构 CLI
  （所有者裁决 P20）：module/configuration/backend/threaded/tests/coverage 参数、默认值、固定模块
  顺序、变体透传、缺 manifest/非法参数 fail-fast、路径与退出码传播；根目录仍禁止模块 build system。
- [修改] [ProposedExtensions.md](ProposedExtensions.md) —— P20 已裁决并迁入 §3.5。

---

## 2026-07-22 (第三批)

- [新增] §6.6 [FfmBinding.md](FfmBinding.md) —— 固化首个真实 Java→Native FFM
  module-validation Version 1 C ABI（所有者裁决 P19）：`BarriEwwNativeFfm` shared target、
  versioned symbol、24-byte status layout、同步 Java-owned segment 借用、稳定错误码/offset
  转译；明确只验证 BECM container 与内嵌 BECS lane，不宣称 Vulkan 完整装载。
- [修改] §6.1/§6.2 [FfmBinding.md](FfmBinding.md) —— FFM 归属与总纲统一为 `Core/`；
  保留 generated hot-path `condy` 规则，增加 handwritten load/init binding 的 instance-lifetime
  一次解析规则；将不存在的 `Linker.Option.isTrivial` 更正为 JDK 25
  `Linker.Option.critical(boolean)`，P19 validation 固定 non-critical。
- [新增] §3.6 [BuildSystem.md](BuildSystem.md) —— 固化 internal static target 与
  Java-loadable shared target、hidden-by-default export、`ffm/` artifact 目录和 Core absolute
  library path property。
- [修改] §4.1/§4.2.1/§4.3/§4.5 [Testing.md](Testing.md) —— Java 测试/覆盖率归属修正为
  `Core/` 与 `Mod/`；P19 用同一 Java-produced native segment 经真实 FFM 完成 staged gate
  第一阶段替换；保留未来 IR→ASM→ClassLoader→Native execution 完整门禁；CI target 修正为
  `dev` / `master`。
- [修改] §7.1.2 [ErrorHandlingAndLogging.md](ErrorHandlingAndLogging.md) 与 §8
  [GeneratedArtifacts.md](GeneratedArtifacts.md) —— FFM 错误转译及 codegen/ClassLoader 归属
  修正为 Core；P19 不创建不存在的 `CompiledRenderPipeline`/ASM/ClassLoader 占位 API。
- [修改] [ProposedExtensions.md](ProposedExtensions.md) —— P19 已裁决并迁入 §3/§4/§6/§7/§8；
  P18 保留给后续 indexed-draw ABI 提案。

---

## 2026-07-22 (第二批)

- [修改] §9.11 [CommandStreamFormat.md](CommandStreamFormat.md) —— 固化 DrawIndirect
  载荷 (byteSize 32: bufferSlot/drawCount/bufferOffset/strideByteCount) 与录制期规则
  (所有者裁决 P17)：继承 Draw 全部前置条件,另需 Indirect usage、4 对齐偏移、参数
  区间在缓冲内;v0.1 仅接受 drawCount==1 且 stride==16,多重间接绘制留待 additive
  增量(设备能力门控)。GPU-driven 形态:参数由同流 compute 阶段产出,CPU 预录时
  不知晓工作量 (ADR-0001)。
- [修改] [ProposedExtensions.md](ProposedExtensions.md) —— P17 已裁决并迁入 §9.11。

---

## 2026-07-22

- [新增] §9.14 [CommandStreamFormat.md](CommandStreamFormat.md) —— GraphicsPipelineTable
  v0.1（所有者裁决 P16）：section type `0x0007`，80-byte 定长记录（双着色器槽/推送常量/
  拓扑/深度状态/剔除绕向/内联 8 槽颜色附件格式），面向动态渲染
  (`VkPipelineRenderingCreateInfo`) 材质化。图形管线不复用 §9.8 PipelineHandleTable——
  改 16-byte entry 属 §9.13 breaking change，新增 section type 为 additive minor。
  v0.1 固定态：无 vertex input（BDA 顶点拉取）、无混合、单采样、viewport/scissor 恒动态。
- [修改] §9.11 [CommandStreamFormat.md](CommandStreamFormat.md) —— 固化
  BindGraphicsPipeline/SetViewport/SetScissor 载荷；补录制期规则（Draw 需开放渲染作用域 +
  已绑图形管线 + 已设 viewport/scissor；绑管线时校验附件格式与作用域模板匹配，ADR-0002 D5）。
- [修改] §9.12 [CommandStreamFormat.md](CommandStreamFormat.md) —— 新增
  PrimitiveTopology/CompareOperation/CullMode/FrontFace 四组中立编码。
- [修改] [ProposedExtensions.md](ProposedExtensions.md) —— P16 已裁决并迁入 §9.14/§9.11/§9.12。

---

## 2026-07-21

- [新增] §9 [CommandStreamFormat.md](CommandStreamFormat.md) —— 将 BECS typed handle-table ABI
  提升为规范性格式文档；定义 `ImageViewHandleTable` v0.1 的 8-byte header、32-byte entry、
  typed slot indirection、load-time materialization/lifetime 与 v0.1 exclusions（所有者裁决 P14）。
- [修改] §4.2 [Testing.md](Testing.md) —— 在 Java IR/ASM/classload/Panama load path 尚未落地
  前，为仅扩展既有 BECS resource table 的 feature 增加 shared Java/C++ golden + native
  Vulkan materialization 的 staged integration gate；FFM load path 落地后必须替换为完整
  end-to-end integration（所有者裁决 P14）。
- [修改] [ProposedExtensions.md](ProposedExtensions.md) —— P14 已裁决并迁入 §9 / §4.2.1。
- [修改] §9 [CommandStreamFormat.md](CommandStreamFormat.md) —— 扩充为完整 BECS ABI 规范
  （所有者裁决 P15）：新增 §9.3 lane stream 与命令头、§9.4 模块容器 (BECM)、
  §9.5 BufferHandleTable、§9.6 ImageHandleTable、§9.7 ShaderModuleTable、
  §9.8 PipelineHandleTable、§9.9 BarrierBatchTable (global/buffer/image 记录)、
  §9.10 RenderingTemplateTable、§9.11 命令 opcode 载荷、§9.12 中立编码枚举值、
  §9.13 版本策略。各布局逐字段对齐代码 offsetof 断言；先前只在代码/ADR 中的设计决策
  自此固化为规范性 v0.1 ABI。
- [新增] RenderingTemplateTable v0.1 + AttachmentLoadOp/StoreOp 中立编码（随 §9.10/§9.12
  固化，双侧写读 + golden RenderingTemplateTable.becs）。

---

## 2026-07-19

初始规约建立，及首轮所有者裁决落地。

- [新增] `spec.md` —— 工程规约总纲：T0 关键指标 (仲裁顺序 `性能 > 兼容性 > 可维护性`)、
  模块拓扑、规约修改流程。
- [新增] §1 [CodingConvention.md](CodingConvention.md) —— C++/Java 命名法、禁缩写、
  一文件一类、关键算法注释要求。
- [新增] §1T [CodingConventionTs.md](CodingConventionTs.md) —— Editor 采用
  **TypeScript** (所有者裁决 P10)，`strict` 全开 + ESLint/Prettier，同构 C++/Java 命名与注释。
- [新增] §2 [CommentConvention.md](CommentConvention.md) —— doxygen-javadoc 子集，
  `@brief` 语言差异，`@note ThreadSafety` (首行) 与 `@warning MemoryOwnership`；
  **注释全英文** (裁决 P3)。
- [新增] §3 [BuildSystem.md](BuildSystem.md) —— CMake 4.4 (**C++23**, 裁决 P1) / Gradle /
  npm+Vite+electron-builder；编译期开关注册表，前缀 **`BARRIEWW_`** (裁决 P2)；
  根目录无构建系统，仅 `build.sh`/`build.bat`。
- [新增] §4 [Testing.md](Testing.md) —— 单元/回归/集成/CI；框架 Catch2 / JUnit 5 / Vitest
  (裁决 P7a)；CI 矩阵 (P7b)；**覆盖率门槛 C++/Java ≥ 90%，TS ≥ 70%** (裁决 P11)。
- [新增] §5 [VersionControl.md](VersionControl.md) —— 分支类别扩充
  (feature/fix/hotfix/chore/docs/test/refactor, 裁决 P6)；commit 六字段；
  **两级合并流 `功能 → dev → master`，两级均需 MR + 完整 CI** (所有者补充)。
- [新增] §6 [FfmBinding.md](FfmBinding.md) —— Panama/FFM 绑定层：downcall handle 编译期
  常量嵌入、trivial/critical IR 标注、内存所有权矩阵、异常不穿越边界 (裁决 P4)。
- [新增] §7 [ErrorHandlingAndLogging.md](ErrorHandlingAndLogging.md) —— 错误处理分层；
  日志系统编译期开关 `BARRIEWW_LOGGING`，新增 `BARRIEWW_CRITICAL_HOTPATH_LOG` 允许热路径
  日志 I/O (裁决 P5 + 所有者补充)。
- [新增] §8 [GeneratedArtifacts.md](GeneratedArtifacts.md) —— 生成类命名、ClassLoader
  隔离、受控扩展机制信任边界与 resolve 契约 (裁决 P8)。
- [新增] [ProposedExtensions.md](ProposedExtensions.md) —— 扩充建议台账 (含已裁决记录)。
- [新增] [CHANGELOG.md](CHANGELOG.md) —— 本文件 (裁决 P9)。
- [修改] §3.3 [BuildSystem.md](BuildSystem.md) —— 新增"构建"维度开关
  `BARRIEWW_BUILD_TESTS` / `BARRIEWW_ENABLE_COVERAGE` 并登记入注册表 (所有者裁决:
  所有 `BARRIEWW_` 私有开关一律登记)；明确"构建"维度不注入为产品 `#if` 宏。
- [修改] spec.md 模块拓扑 + §3.1/§3.4 [BuildSystem.md](BuildSystem.md) —— 新增 `Core/`
  模块 (Java 大脑：RDG 编译器 + BECS 写侧 + FFM)；`Mod/` 重定义为薄二进制加载器，
  只做游戏侧装载 (所有者裁决 2026-07-19)。
- [修改] §1.1.2 [CodingConvention.md](CodingConvention.md) —— 豁免清单新增
  `Info`/`CreateInfo` (仅限镜像 Vulkan `Vk*Info`/`Vk*CreateInfo` 惯用语；裁决 P12)。
- [新增] §2.6 [CommentConvention.md](CommentConvention.md) —— 简单访问器精简注释：
  类级总括 + 单行说明，条件三项 (单表达式/不跨 FFM/类级已总括)；跨 FFM 边界函数
  不受影响 (裁决 P13)。
