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
| C++        | **Catch2**  | `Native/`         |
| Java       | **JUnit 5** | `Mod/`            |
| TypeScript | **Vitest**  | `Editor/`         |

## §4.2 回归与集成测试 (强制于新 feature)

- **每次新 feature 生成，必须执行回归测试与集成测试。**
- 集成测试须覆盖跨语言链路 (Java IR → ASM 字节码 → classload → Panama downcall →
  Native 执行) 的正确性，而非仅单侧 mock。

## §4.3 CI 门禁 (强制于向 main 的 MR)

- **所有向 `main` 的 MR 必须经过 CI，CI 使用 GitHub Actions。**
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
| Java (`Mod/`)     | **≥ 90%**    |
| TypeScript (`Editor/`) | **≥ 70%** |

- 覆盖率由 CI 采集 (C++: gcovr/llvm-cov；Java: JaCoCo；TS: Vitest coverage)，
  低于阈值即阻断合并。
- 阈值按模块整体计；不得以关闭关键路径测试的方式凑数。
