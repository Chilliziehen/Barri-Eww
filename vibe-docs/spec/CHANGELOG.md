# 规约变更记录 (SPEC CHANGELOG)

> 记录 `vibe-docs/spec` 正式条款的每次变更 (日期 / 条款 / 理由)，服务长期项目的规约演进回溯。
> 格式：`YYYY-MM-DD` 分组，逐条列 `[新增|修改|删除] 条款 —— 理由`。
> 变更走 `docs/spec-*` 分支 + MR (见 §5)。

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
