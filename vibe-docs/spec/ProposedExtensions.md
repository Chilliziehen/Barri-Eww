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
| —    | `Editor/` 技术栈 Node.js/Electron                 | spec.md / §1T                              |
| —    | 两级合并流 `功能 → dev → master` + 全 CI          | VersionControl §5.4                         |
| —    | 新增开关 `BARRIEWW_CRITICAL_HOTPATH_LOG`          | BuildSystem §3.3 / §7.2.1 (所有者补充)      |

T0 三指标仲裁顺序 `性能 > 兼容性 > 可维护性` 已确认 (spec.md T0)。

---

## 待裁决 (PROPOSED)

_当前无待裁决项。_

新的扩充建议在此登记，标 `PROPOSED`，经所有者确认后迁入对应正式文档并在 CHANGELOG 记录。
