# §5 版本管理

> 服务 T0[2] (可维护) 与协作可追溯性。

---

## §5.1 版本控制系统

- 使用 **git** 进行版本管理。

## §5.2 分支命名规范

- 格式：`<type>/<specific-work>`。
- `<type>` 取值 (已确认扩充)：

  | type       | 用途                                   |
  | ---------- | -------------------------------------- |
  | `feature`  | 新功能开发                             |
  | `fix`      | 缺陷修复 (非紧急)                      |
  | `hotfix`   | 紧急修复 (针对已发布/`master`)         |
  | `chore`    | 杂务 (依赖升级、脚手架、配置)          |
  | `docs`     | 文档 (含本 spec)                       |
  | `test`     | 测试专项 (补测试、测试基建)            |
  | `refactor` | 重构 (不改变对外行为)                  |

- `<specific-work>` 用小写 kebab-case 描述具体工作，语义完整、不缩写。
- 示例：`feature/barrier-insertion-pass`、`docs/spec-authoring`、
  `fix/downcall-handle-lifetime`、`refactor/command-recorder-lanes`。

## §5.3 Commit Message 规范

**概要行**同分支：`<type>/<specific-work>` (`type` 取值同 §5.2)。

**正文 (※重要※，必填以下全部字段)：**

```
<type>/<specific-work>

改动点:
  - <本次具体改了什么>
影响范围:
  - <涉及哪些模块/子系统/构建变体>
相关文件:
  - <path/to/file> — <该文件的改动摘要>
改动思路:
  - <为什么这样改，采用的方案与取舍>
功能性变化:
  - <对外可观察行为的变化；无则写 无>
非功能性变化:
  - <性能/可维护性/构建/兼容性等变化；无则写 无>
```

- 缺任一字段的 commit 不合规。字段值同样遵守"语义完整、不缩写"。
- 一次 commit 聚焦单一逻辑改动，避免混杂无关变更。

## §5.4 合并流与分支保护 (※强制※)

采用 **`功能分支 → dev → master`** 的两级合并流：

```
feature/… ┐
fix/…     ├─(MR + 完整 CI)→  dev  ─(MR + 完整 CI)→  master
chore/…   ┘
```

- **所有开发分支 (feature/fix/chore/docs/test/refactor) 只允许合并至 `dev`。**
  禁止开发分支直接合并进 `master`。
- **`master` 只能由 `dev` 合入。** 其他任何分支不得直接进 `master`。
  - 例外：`hotfix/*` 针对线上紧急修复，允许合入 `master`；合入后必须**回合 (back-merge)
    到 `dev`**，保证 `dev` 不落后于 `master`。
- **两级合并 (→ `dev` 与 `dev` → `master`) 均须通过 MR，且合入前必须跑完整 CI**
  (见 §4.3 / §4.4)，CI 未通过不得合并。
- **禁止向 `dev` 与 `master` 直接推送**，一律经 MR。
- `dev` 为集成分支 (日常开发汇聚)，`master` 为受保护的发布分支 (始终可发布状态)。

## §5.5 与 CI 联动

- 一切向 `dev` 与 `master` 的合并均通过 MR 并经 GitHub Actions CI (见 §4.3 / §4.4)。
