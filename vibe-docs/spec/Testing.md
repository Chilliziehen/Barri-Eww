# §4 软件测试

> 服务 T0[2] (可维护)。测试是合入的硬门槛。

---

## §4.1 单元测试 (强制)

- **所有 C++ / Java / TypeScript 功能实现必须编写单元测试。** 无单元测试的功能实现不得合入。
- 单元测试与被测功能同变体可构建、可运行。
- 关键算法 (barrier 插入、SSA 化、别名分配、并行分区、IR→ASM) 须有针对边界条件的用例。

**测试框架 (已确认)：**

| 语言 | 框架        | 所属模块          |
| ---- | ----------- | ----------------- |
| C++        | **Catch2**  | `Native/`          |
| Java       | **JUnit 5** | `Core/` / `Mod/`   |
| TypeScript | **Vitest**  | `Editor/`           |

## §4.2 回归与集成测试 (强制于新 feature)

- **每次新 feature 生成，必须执行回归测试与集成测试。**
- 集成测试须覆盖跨语言链路 (Java IR → ASM 字节码 → classload → Panama downcall →
  Native 执行) 的正确性，而非仅单侧 mock。

### §4.2.1 BECS staged integration gate（分阶段替换）

在 Java IR → ASM → classload → Panama → Native execution path 尚未完整实现并纳入测试的阶段，
**仅扩展既有 BECS resource table 的 feature** 可以暂以以下 staged integration gate
满足本节的跨语言验证要求：

1. Core JUnit writer test 必须生成 byte-exact 的 committed shared golden；
2. Native Catch2 test 必须读取同一 fixture、验证、解码并断言完整 schema fields；
3. 涉及 native materialization 的 feature 必须有 headless Vulkan integration coverage；
4. Java 与 Native 完整 unit/regression suite 及 Native `THREADED_RECORDING` on/off
   构建变体必须通过。

P19 landing 时须完成该 gate 的第一阶段替换：同一 Core JUnit 场景先 byte-exact 生成
committed module，再把**同一 Java-owned native `MemorySegment`** 经真实 Panama downcall
交给 Native module/lane validator，并精确断言成功或受检异常上下文。显式选择该 FFM
integration task 时，缺 library/symbol 必须 FAIL，不得 SKIP。该阶段只证明 §6.6 validation
boundary，不宣称 resource materialization 或 GPU execution 已跨 FFM。

当 IR/compiler/generated ClassLoader 与完整 Native loader feature 落地时，必须继续把 gate
替换为完整 Java IR → ASM → classload → Panama → Native execution integration test；P19 不得
为尚未存在的接口创建占位实现。现有 headless Vulkan tests 在本地缺 driver/required feature
时可 SKIP；CI 负责提供已验证的软件 Vulkan 环境，环境缺失不得作为通过。

该条只是一项可验证的分阶段门禁，不放宽最终的 Java-to-Native end-to-end 要求。

## §4.3 CI 门禁 (强制于向 dev / master 的 MR)

- **所有向 `dev` / `master` 的 MR 必须经过 CI，CI 使用 GitHub Actions。**
- CI 未通过的 MR 不得合并。
- CI 至少运行：两侧构建 (多变体)、单元测试、回归测试、集成测试。

---

## §4.4 CI 构建矩阵 (已确认基线)

MVP 阶段 CI 矩阵：`OS {Windows, Linux} × 后端 {Vulkan} × 开关 {THREADED_RECORDING on/off}`。
Vulkan 集成测试在 CI 无 GPU 环境下，使用 headless + 软件实现 (SwiftShader / lavapipe) 运行。

## §4.5 覆盖率门槛 (已确认，CI 硬门禁)

| 语言              | 最低行覆盖率 |
| ----------------- | ------------ |
| C++ (`Native/`)   | **≥ 90%**    |
| Java (`Core/` / `Mod/`) | **≥ 90%**    |
| TypeScript (`Editor/`) | **≥ 70%** |

- 覆盖率由 CI 采集 (C++: gcovr/llvm-cov；Java: JaCoCo；TS: Vitest coverage)，
  低于阈值即阻断合并。
- 阈值按模块整体计；不得以关闭关键路径测试的方式凑数。
