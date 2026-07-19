# §3 构建系统

> 服务 T0[1] (编译期优先) 与 T0[3] (多变体、高兼容)。核心思想：
> **一切平台/功能差异在编译期通过开关表达，不留运行时探测分支。**

---

## §3.1 构建系统归属

- **C++ (`Native/`) 使用 CMake 构建，版本锁定 CMake 4.4，语言标准锁定 C++23**
  (`set(CMAKE_CXX_STANDARD 23)` + `CMAKE_CXX_STANDARD_REQUIRED ON`)。
- **Java (`Mod/`) 使用 Gradle 构建。**
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

> `BARRIEWW_CRITICAL_HOTPATH_LOG` 默认 **OFF**：T0[1] 要求热路径禁止日志 I/O，此开关仅供
> 排障时临时开启；开启后热路径的日志宏才展开为实际 I/O，否则编译为 no-op。
> 依赖关系：`BARRIEWW_CRITICAL_HOTPATH_LOG=ON` 要求 `BARRIEWW_LOGGING=ON`
> (CMake 配置期校验，冲突则报错)。日志规约详见 [ErrorHandlingAndLogging.md](ErrorHandlingAndLogging.md)。

> 命名约定 (已确认)：项目私有开关统一 **`BARRIEWW_`** 前缀；历史/领域约定名
> (如 `THREADED_RECORDING`) 保留原名并在此表标注。

---

## §3.4 Java 构建变体 (Gradle)

- Java 侧同样需提供多种构建变体 (平台目标 / 功能开关组合)。
- 变体通过 Gradle 的构建参数/product flavor 或等价机制表达；与 C++ 侧开关维度**语义对齐**
  (同名功能在两侧启停一致)，避免 Java 期望某功能而 Native 未编入。
- 具体 Gradle 变体维度定义见 ProposedExtensions (待确认技术选型后回填)。

---

## §3.5 根构建入口契约

`build.sh` / `build.bat` 须支持 (最低)：

- 选择目标模块 (`Native` / `Mod` / 全部)；
- 透传变体/开关 (如 `--threaded-recording=off`)，转译为各模块构建系统的对应参数；
- 统一 Debug/Release 配置入口；
- 保证两侧功能开关一致性 (§3.4)。

> 具体 CLI 契约在实现前进入 ProposedExtensions 讨论定稿。
