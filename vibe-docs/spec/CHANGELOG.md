# 规约变更记录 (SPEC CHANGELOG)

> 记录 `vibe-docs/spec` 正式条款的每次变更 (日期 / 条款 / 理由)，服务长期项目的规约演进回溯。
> 格式：`YYYY-MM-DD` 分组，逐条列 `[新增|修改|删除] 条款 —— 理由`。
> 变更走 `docs/spec-*` 分支 + MR (见 §5)。

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
