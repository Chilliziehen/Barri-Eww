# Barri-Eww 工程规约 (SPEC)

> 本文件是工程规约的**索引与总纲**。它固化项目关键指标 (T0) 与总体约束，
> 各细分领域的详细规约拆分到 `vibe-docs/spec/` 下的独立文档，便于长期维护。
>
> 项目设计意图 / 架构判断见 [`claude/project-brief.md`](claude/project-brief.md)。
> 本文件只关心**如何做 (how)**，不重复**做什么 / 为什么 (what / why)**。

---

## 规约状态与修改流程

- 本规约是长期 vibe coding 项目的契约。**已固化条款 (T0 及 §1–§5) 的任何修改
  必须先与项目所有者讨论并以其意见为准。**
- AI/协作者**有权直接提出规约扩充建议**，扩充建议统一进入
  [`spec/ProposedExtensions.md`](spec/ProposedExtensions.md)，标记为 `PROPOSED`，
  经所有者确认后方可提升为正式条款并迁入对应细分文档。
- 规约变更走正常的 git 流程 (见 §5)，分支/commit summary 使用 `docs/spec-*`。

---

## T0. 项目关键指标 (最高优先级，一切设计决策的仲裁标准)

> 当任意设计取舍与下列指标冲突时，以下列指标为准；三条指标之间冲突时，
> 按 **`[1] 性能 > [3] 兼容性 > [2] 可维护性`** 的顺序仲裁 (已确认)。

### T0[1] 性能敏感 —— 编译期优先原则

**凡是可以在编译期完成的工作，必须在编译期完成，不得推迟到运行时。**

- C++ 侧：优先 `constexpr` / `consteval` / 模板特化 / `if constexpr`，而非运行时分支；
  平台与功能差异走编译期宏注入 (见 §3)，不留运行时 dead branch。
- Java 侧：图拓扑在编译期 (JIT codegen) 解算完成；Panama downcall handle 在图编译期
  解析并作为常量嵌入生成字节码；运行时热路径是直线代码，不做图遍历 / 查表 / 反射。
- 运行时热路径 (command 录制) 不允许出现：堆分配、装箱、虚分派 (除非已证明可被内联)、
  锁竞争、异常控制流。

### T0[2] 长期可维护 —— 严格规范原则

**所有代码必须严格遵守 §1 (代码规范) 与 §2 (注释规范)，无例外。**

- 语义完整优先于书写便利：**禁止任何缩写与语义缩减** (见 §1)。
- "一个文件一个类"：类/结构体的定义与实现按此拆分 (见 §1)。
- 关键算法必须在注释中标注原理并给出语义完整的伪代码 (见 §2)。

### T0[3] 高兼容性 —— 多变体构建原则

**必须提供多种构建变体，以适配不同平台、不同功能配置的需要。**

- 平台维度与功能开关维度的组合，通过构建系统的编译期开关表达 (见 §3)，
  而非运行时探测 + 分支。
- 所有编译期开关集中登记，禁止散落式新增 (见 §3 的开关注册表)。

---

## 模块拓扑

项目分为三个顶层模块，各自持有独立构建系统，**根目录不放置任何构建系统** (见 §3)：

| 模块       | 语言              | 构建系统  | 职责                                                                 |
| ---------- | ----------------- | --------- | -------------------------------------------------------------------- |
| `Native/`  | C++23             | CMake 4.4 | 原生渲染后端 (MVP: Vulkan)，接收 Java 侧 native 调用，录制 command   |
| `Mod/`     | Java (SE 25)      | Gradle    | Fabric mod + RDG 编译器 (IR → ASM 字节码) + Panama/FFM 绑定层        |
| `Editor/`  | TypeScript (Node.js/Electron) | npm + electron-builder | 可视化 RenderGraph 编辑器 (产出图 JSON)；非 MVP 范围 |

根目录仅提供 [`build.sh`](build.sh) / [`build.bat`](build.bat) 作为跨模块构建入口 (见 §3)。

---

## §1–§5 细分规约索引

| 编号 | 领域         | 文档                                                          |
| ---- | ------------ | ------------------------------------------------------------- |
| §1   | 代码规范 (C++/Java) | [`spec/CodingConvention.md`](spec/CodingConvention.md)  |
| §1T  | 代码规范 (TypeScript/Electron) | [`spec/CodingConventionTs.md`](spec/CodingConventionTs.md) |
| §2   | 注释规范     | [`spec/CommentConvention.md`](spec/CommentConvention.md)     |
| §3   | 构建系统     | [`spec/BuildSystem.md`](spec/BuildSystem.md)                 |
| §4   | 软件测试     | [`spec/Testing.md`](spec/Testing.md)                         |
| §5   | 版本管理     | [`spec/VersionControl.md`](spec/VersionControl.md)           |
| §6   | Panama/FFM 绑定层 | [`spec/FfmBinding.md`](spec/FfmBinding.md)              |
| §7   | 错误处理与日志 | [`spec/ErrorHandlingAndLogging.md`](spec/ErrorHandlingAndLogging.md) |
| §8   | 生成产物与 ClassLoader | [`spec/GeneratedArtifacts.md`](spec/GeneratedArtifacts.md) |
| —    | 扩充建议     | [`spec/ProposedExtensions.md`](spec/ProposedExtensions.md)   |
| —    | 规约变更记录 | [`spec/CHANGELOG.md`](spec/CHANGELOG.md)                     |
