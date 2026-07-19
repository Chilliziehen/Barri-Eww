# §1T 代码规范 (TypeScript / Electron)

> 适用范围：`Editor/` 模块 (Node.js / Electron，语言 **TypeScript**)。本规范是 §1
> (C++/Java 代码规范) 与 §2 (注释规范) 在 TS 生态中的**对齐落地**——让不熟悉前端的
> 维护者也能以与 C++/Java 一致的心智模型阅读/维护代码。服务 T0[2] (长期可维护)。
>
> 凡本文件未特别说明处，一律沿用 §1 / §2 精神 (严格禁缩写、语义完整、一文件一类、
> 关键算法注释、注释全英文)。

---

## §1T.0 语言基线与"编译期优先"落地 (服务 T0[1])

TypeScript 的类型系统即"编译期契约"，是本模块贯彻 T0[1] 的核心手段：

- **源码全部为 `.ts` / `.tsx`**，编译到 ES2023 + ESM。**禁止 `.js` 源文件** (生成产物除外)。
- **`tsconfig.json` 开启 `strict: true`** 全家桶，并额外开启：
  `noImplicitAny`、`noUncheckedIndexedAccess`、`exactOptionalPropertyTypes`、
  `noImplicitOverride`、`noFallthroughCasesInSwitch`、`noImplicitReturns`、
  `useUnknownInCatchVariables`。类型错误 == 编译失败，不得合入。
- **禁用 `any`** (用 `unknown` + 收窄)；禁用 `@ts-ignore` (确需时用 `@ts-expect-error`
  并附英文理由注释)；禁用非空断言 `!` (用显式收窄/校验替代，边界处例外须注释)。
- **强制 ESLint (`@typescript-eslint`) + Prettier**：承担 C++/Java 里"编译器 + 规范"的
  强约束角色 (命名、未用符号、禁 `var`、禁浮空 Promise 等)；二者在 CI 中为硬门槛。
- 本可静态决定的差异不写运行时探测分支；构建期差异走构建配置 (§3.4)。

---

## §1T.1 命名规范 (对齐 §1.1)

| 目标                       | 命名法             | 前缀   | 示例                                 |
| -------------------------- | ------------------ | ------ | ------------------------------------ |
| 文件名                     | 大驼峰 PascalCase  | —      | `RenderGraphCanvas.ts`               |
| 类 / 接口 / 类型别名 / 枚举 | 大驼峰            | —      | `RenderGraphCanvas` / `GraphNode`    |
| 实例成员字段               | 小驼峰             | `m_`   | `this.m_selectedNode`                |
| 私有实例字段 (语言级私有)  | 小驼峰             | `#m_`  | `this.#m_internalState`              |
| 静态字段                   | 小驼峰             | `s_`   | `RenderGraphCanvas.s_instanceCount`  |
| 模块级全局变量             | 小驼峰             | `g_`   | `g_activeDocument`                   |
| 局部变量                   | 小驼峰             | —      | `selectedNode`                       |
| 函数 / 方法名              | 小驼峰             | —      | `serializeGraphToJson`               |
| 函数形参 (声明处与实现处)  | 小驼峰、语义完整   | —      | `graphDocument`                      |
| 泛型类型参数               | 大驼峰、语义完整   | —      | `TGraphNode` (禁止裸 `T`/`K`/`V`)    |
| 常量 (模块级不可变字面量)  | 全大写下划线       | —      | `MAX_NODE_COUNT`                     |

严格规则 (同 §1.1.2，无例外)：

- **禁止任何缩写 / 语义缩减**：`context` 不写 `ctx`，`document` 不写 `doc`，
  `configuration` 不写 `config` (标识符层面；`package.json`/`tsconfig.json` 等生态既定
  文件名不受此限)。
- **泛型参数也须语义完整**：写 `TGraphNode`、`TPayload`，禁止裸单字母。
- 形参使用语义完整小驼峰；接口/实现的形参名必须一致。
- 布尔量用 `is` / `has` / `should` / `can` 前缀。
- 公认缩写豁免沿用 §1.1.2，另增 TS/前端生态既定名：`json`、`html`、`css`、`url`、
  `dom`、`ipc`、`api`、`id`、`props`、`ref`。
- **接口不加 `I` 前缀** (与项目 PascalCase 一致；用领域名区分，如 `GraphNode` 而非 `IGraphNode`)。

---

## §1T.2 "一个文件一个类/一个主类型" (对齐 §1.2)

- 一个 `.ts` 文件恰含**一个 `export` 的顶层类**，文件名 == 类名。
- 纯类型模块：一个文件可聚合强相关的 `interface`/`type`/`enum`，但须按功能命名
  (如 `GraphTypes.ts`)，禁止 `Types.ts` 之类无边界命名。
- 纯函数工具模块允许单文件多函数，须**按功能命名** (如 `GraphSerialization.ts`)，
  禁止 `Utils.ts` / `Helpers.ts`。
- 一个模块只有一个"主导出"。

---

## §1T.3 关键算法注释 (对齐 §1.3)

图布局、拓扑校验、序列化/反序列化、撤销栈 diff 等关键算法，其实现处注释须给出
**算法原理 + 语义完整的英文伪代码** (伪代码标识符禁缩写)。

---

## §1T.4 注释规范 (对齐 §2，TSDoc 落地)

- 类、方法、导出函数用 **`/** ... */` (TSDoc/JSDoc 语法)**，禁止用 `//` 连行代替。
- **注释全英文** (同 §2.1)。
- 每个参数用 **`@param parameterName Description`**：TS 已在签名给出类型，故 `@param`
  不重复写类型 (这是 §2.1"显式给出数据类型"在 TS 下的落地——类型由签名强制且编译期校验，
  比注释更强)。参数名语义完整不缩写。有返回值可用 `@returns`。
- 会 `throw` 的函数用 `@throws` 说明失败语义与边界。
- 自定义标签 (与 §2.3 对齐，按 TS/Electron 语境重释)：
  - **`@note ThreadSafety`（必须首行标出）**：说明相对 JS 并发模型的安全性——是否可被
    异步任务/微任务重入、是否可被 Worker 线程调用、是否假定只在 main 或 renderer 单进程运行。
  - **`@warning MemoryOwnership`**：跨 IPC 传递的 `ArrayBuffer`/`MessagePort`、需显式
    `dispose()`/`close()` 的 Electron 资源 (BrowserWindow、native handle、fd)，
    必须写明**由谁释放、生命周期归属**。

### TSDoc 示例

```ts
/**
 * @note ThreadSafety: Renderer-process only. Not re-entrant; must not be invoked
 *       again before the returned Promise settles (it mutates the shared undo stack).
 * Serializes the in-memory graph document into its on-disk JSON representation and
 * hands the encoded bytes to the main process over IPC for writing.
 * Boundary: rejects with GraphValidationError when the document fails topology checks.
 *
 * @param graphDocument The graph document to serialize
 * @param destinationFilePath Absolute path the main process will write to
 * @returns Number of bytes written on success
 * @throws GraphValidationError When the document topology is invalid
 * @warning MemoryOwnership: The transferred ArrayBuffer is detached and its ownership
 *          moves to the main process; the caller must not touch it after the await.
 */
async function serializeGraphToJson(
    graphDocument: GraphDocument,
    destinationFilePath: string,
): Promise<number> {
    // ...
}
```

---

## §1T.5 Electron 架构与安全规约 (强制)

Electron 三类进程职责严格分离，各自"一文件一类/一功能"：

- **Main / Preload / Renderer 代码分目录存放**，互不越界导入。
- **安全基线 (硬性)：** 所有 `BrowserWindow` 必须 `contextIsolation: true`、
  `nodeIntegration: false`、`sandbox: true`；**禁用 `@electron/remote`**。
- **Renderer 不直接触碰 Node/系统 API**；一切特权操作 (文件读写、调用 Java/Native
  工具链) 经 **preload 的 `contextBridge` 暴露的窄接口 + IPC**，接口面最小化。
- **IPC 通道名与其 payload 类型集中登记**于单一 typed 模块 (类比 §3.3 的开关注册表)，
  禁止散落式字符串字面量；IPC payload 必须有显式 TS 类型。
- IPC 传入数据一律视为**不可信**，跨边界前后都要运行时校验 (类型断言不等于校验)。

---

## §1T.6 语言使用约束 (服务 T0[2])

- 只用 `const` / `let`，**禁用 `var`**；默认 `const`。
- **禁止浮空 Promise**：异步调用要么 `await`，要么显式 `.catch`。优先 `async/await`。
- `throw` 只能抛 `Error` 及其子类；自定义错误继承 `Error` 并设 `name` (对齐 §7)。
- 相等判断只用 `===` / `!==`。
- 优先 `readonly` / 不可变数据；不修改传入参数 (除非职责即原地变换且注释写明)。
- 优先具名 `export`，避免 `export default` (利于重构与一致命名)。

---

## §1T.7 工具链与强制门禁 (对齐 §3 / §4)

- `Editor/` 内自持 `package.json`、`tsconfig.json`、ESLint 配置、Prettier 配置。
- 构建：**Vite (renderer) + tsc/electron-builder (打包)**；细节见 [BuildSystem §3](BuildSystem.md)。
- CI 对 `Editor/` 至少运行：`tsc --noEmit` (严格类型检查)、ESLint、Prettier check、
  Vitest 单元测试 (覆盖率门槛见 §4)。任一失败即阻断合并。
