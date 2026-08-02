# §3 构建系统

> 服务 T0[1] (编译期优先) 与 T0[3] (多变体、高兼容)。核心思想：
> **一切平台/功能差异在编译期通过开关表达，不留运行时探测分支。**

---

## §3.1 构建系统归属

- **C++ (`Native/`) 使用 CMake 构建，版本锁定 CMake 4.4，语言标准锁定 C++23**
  (`set(CMAKE_CXX_STANDARD 23)` + `CMAKE_CXX_STANDARD_REQUIRED ON`)。
- **Java (`Core/` 与 `Mod/`) 使用 Gradle 构建**，两模块各自持有独立 Gradle 构建
  (职责边界见 spec.md 模块拓扑；`Mod/` 依赖 `Core/` 的产物)。
- **Editor (`Editor/`) 使用 npm + Vite + electron-builder 构建** (Node.js/Electron，
  语言 TypeScript，`tsc` 严格类型检查)，其构建脚本与依赖清单 (`package.json`、
  `tsconfig.json` 等) 只存在于 `Editor/` 内。
- **根目录严禁出现任何模块构建系统** (CMake / Gradle / package.json)。构建系统只存在于
  各模块目录内。
- 根目录仅提供 [`build.sh`](../build.sh) (POSIX) 与 [`build.bat`](../build.bat) (Windows)
  作为跨模块构建入口，负责按顺序/依赖调用各模块自身的构建系统并透传变体参数。

---

## §3.2 编译期开关注入 (C++)

功能启用性差异**通过 CMake option → 编译宏注入**实现，代码中用预处理条件包裹。

**范式 (以多线程指令录制为例)：**

```cmake
# Native/CMakeLists.txt
option(THREADED_RECORDING "Enable multithreaded command recording" ON)
if(THREADED_RECORDING)
    target_compile_definitions(BarriEwwNative PUBLIC THREADED_RECORDING=1)
endif()
```

```cpp
#if THREADED_RECORDING
    // 多线程分 lane 录制路径
#endif
```

**设计理由 (※重要※)：** 本项目性能敏感，可在编译期完成的 configuration 必须在编译期完成；
关闭的功能不应在最终二进制中留下任何运行时判断或死代码。

---

## §3.3 编译期开关注册表 (禁止散落式新增)

所有编译期开关**集中登记于此表**，新增开关必须先在此登记 (含默认值、影响范围)，
再在 CMake 与代码中使用。禁止未登记的临时宏。

| 开关宏               | 维度   | 默认 | 含义                                   | 影响模块 |
| -------------------- | ------ | ---- | -------------------------------------- | -------- |
| `THREADED_RECORDING`            | 功能 | ON  | 多线程 command 录制                         | Native |
| `BARRIEWW_BACKEND_VULKAN`       | 平台 | ON  | 启用 Vulkan 后端 (MVP 唯一后端)             | Native |
| `BARRIEWW_BACKEND_DX12`         | 平台 | OFF | 启用 DX12 后端 (非 MVP，占位)               | Native |
| `BARRIEWW_BACKEND_METAL`        | 平台 | OFF | 启用 Metal 后端 (非 MVP，占位)             | Native |
| `BARRIEWW_LOGGING`              | 功能 | ON  | 启用日志系统 (关闭则日志宏编译为 no-op)     | Native |
| `BARRIEWW_CRITICAL_HOTPATH_LOG` | 功能 | OFF | 允许热路径 (command 录制) 内的日志 I/O      | Native |
| `BARRIEWW_BUILD_TESTS`          | 构建 | ON  | 是否构建单元测试 (Catch2)                   | Native |
| `BARRIEWW_ENABLE_COVERAGE`      | 构建 | OFF | 是否为覆盖率插桩 (GCC/Clang)                | Native |
| `BARRIEWW_GPU_FRAME_METRICS`    | 功能 | OFF | 编译 GPU timestamp query 与完成帧 GPU 分段指标；Release Demo 变体开启 | Native |

> **维度说明**：`功能`/`平台` 维度的开关会被注入为产品代码宏 (`<SWITCH>=1`) 并由 `#if`
> 消费；`构建` 维度的开关 (`BARRIEWW_BUILD_TESTS`/`BARRIEWW_ENABLE_COVERAGE`) 仅影响
> CMake 构建什么，**不注入为产品 `#if` 宏**。两类均须登记于本表 (已确认：所有
> `BARRIEWW_` 私有开关一律登记，禁止散落式新增)。

> `BARRIEWW_CRITICAL_HOTPATH_LOG` 默认 **OFF**：T0[1] 要求热路径禁止日志 I/O，此开关仅供
> 排障时临时开启；开启后热路径的日志宏才展开为实际 I/O，否则编译为 no-op。
> 依赖关系：`BARRIEWW_CRITICAL_HOTPATH_LOG=ON` 要求 `BARRIEWW_LOGGING=ON`
> (CMake 配置期校验，冲突则报错)。日志规约详见 [ErrorHandlingAndLogging.md](ErrorHandlingAndLogging.md)。

> 命名约定 (已确认)：项目私有开关统一 **`BARRIEWW_`** 前缀；历史/领域约定名
> (如 `THREADED_RECORDING`) 保留原名并在此表标注。

---

## §3.4 Java 构建变体 (Gradle)

- Java 侧 (`Core/`、`Mod/`) 同样需提供多种构建变体 (平台目标 / 功能开关组合)。
- 变体通过 Gradle 的构建参数/product flavor 或等价机制表达；与 C++ 侧开关维度**语义对齐**
  (同名功能在两侧启停一致)，避免 Java 期望某功能而 Native 未编入。
- 具体 Gradle 变体维度定义见 ProposedExtensions (待确认技术选型后回填)。

---

## §3.5 根构建入口契约

根 `build.sh` 与 `build.bat` 必须提供完全同构的 CLI、默认值、参数校验、模块顺序、退出码
与变体翻译：

```text
build.sh|build.bat
  [--module native|core|mod|editor|all]
  [--configuration debug|release]
  [--backend vulkan]
  [--threaded-recording on|off]
  [--tests on|off]
  [--coverage on|off]
  [--help]
```

| 参数 | 默认值 | 语义 |
| ---- | ------ | ---- |
| `--module` | `all` | 构建一个指定模块，或按 `Native → Core → Mod → Editor` 固定依赖顺序构建全部模块 |
| `--configuration` | `debug` | 映射到各模块等价的 Debug/Release 构建配置 |
| `--backend` | `vulkan` | 当前仅接受已实现的 Vulkan；未知/未实现后端必须失败 |
| `--threaded-recording` | `on` | 映射到 Native `THREADED_RECORDING=ON/OFF` 及 Java 语义对齐的 Gradle property |
| `--tests` | `on` | 控制 Native `BARRIEWW_BUILD_TESTS` 与 Java/TypeScript test task 是否执行 |
| `--coverage` | `off` | 控制 Native `BARRIEWW_ENABLE_COVERAGE` 与 Java/TypeScript coverage gate |

固定行为：

1. `--coverage on` 要求 `--tests on`；显式冲突必须在调用任何模块构建前失败。
2. `--help` 仅输出 usage 并成功退出；未知参数、缺值、非法值均输出错误并非零退出。
3. `all` 始终表示固定拓扑中的全部模块，不得静默退化为“当前可构建模块”。任一模块缺少其
   固定 build manifest 时，显式构建该模块或 `all` 必须清晰失败；若未来需要“可用模块”
   模式，须新增不同名称并先登记规约。
4. 两脚本只负责编排并打印所执行的模块命令；CMake/Gradle/npm 配置与 dependency lock
   仍仅存在于模块目录。根目录不得新增模块 build system。
5. Native 变体翻译：configuration → CMake config，backend → `BARRIEWW_BACKEND_*`，
   threaded/tests/coverage → §3.3 已登记开关。Core/Mod/Editor 使用各自等价 task/property；
   不支持某个已请求维度时须失败，不得忽略。
6. 脚本必须原样传播第一个失败模块的非零退出码并停止后续模块；路径必须相对脚本自身目录
   解析，从任意 working directory 调用及 repository path 含空格时行为一致。

该契约由所有者裁决 P20 固化；后续增加参数、模块模式或默认值均须走规约修订。

---

## §3.6 Native FFM shared-library contract (P19)

- `BarriEwwNative` 是内部 **STATIC** C++ implementation/test target；须启用 position-independent
  code，以便被 shared target 复用。
- `BarriEwwNativeFfm` 是 Java FFM 唯一可直接加载的 **SHARED** target：Windows 产出
  `BarriEwwNativeFfm.dll`，Linux 产出 `libBarriEwwNativeFfm.so`。
- shared target 默认隐藏 symbol，只显式导出 §6 固化的 Versioned C ABI。export decoration
  只控制编译期可见性，不是功能开关，不新增 CMake option 或运行时分支。
- 所有 generator/configuration 的 shared artifact 统一输出到所选 Native build directory 下的
  `ffm/`。Core integration test 通过 Gradle property
  `barriewwNativeLibraryPath=<absolute-library-file>` 获得精确文件，不做运行时平台搜索。
- `BarriEwwNativeFfm` 必须在 `BARRIEWW_BACKEND_VULKAN=ON/OFF` 与
  `THREADED_RECORDING=ON/OFF` 组合下可构建；P19 boundary 本身 backend-neutral。
