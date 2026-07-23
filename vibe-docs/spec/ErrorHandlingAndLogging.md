# §7 错误处理与日志规约

> 适用范围：全模块 (C++ / Java / TypeScript)。服务 T0[1] (性能) 与 T0[2] (可维护)。
> 与 §6 (FFM 边界) 联动。

---

## §7.1 错误处理

### §7.1.1 C++ (`Native/`)

- **热路径 (command 录制) 不使用异常**：以状态码 / `std::expected<T, ErrorCode>` 表达失败。
- 慢路径 (init、资源创建) 允许使用异常，但**异常绝不得穿越 FFM 边界** (见 §6.4)。
- 错误码集中定义为强类型 `enum class`，语义完整命名，禁止裸整型/魔数。
- 不吞错：所有失败要么被处理，要么向上传播并保留上下文。

### §7.1.2 Java (`Core/` / `Mod/`)

- **热路径 (JIT 生成的录制代码) 不抛异常**，不做异常控制流。
- **慢路径 (图数据校验、编译、init) 抛受检异常**，异常须携带完整上下文
  (出错节点 ID、资源版本、原因)，便于定位。
- 自定义异常继承合适基类并语义完整命名 (如 `GraphValidationException`)。
- Native 返回的错误码在 `Core/` 慢路径转译为对应 Java 受检异常 (见 §6.4)；异常须保留
  原生错误类别及该类别适用的 byte offset、lane offset、slot、command index 或 backend
  result 等完整上下文。`Mod/` 只传播或呈现 Core 异常，不重复定义 FFM 错误映射。

### §7.1.3 TypeScript (`Editor/`)

- `throw` 只能抛 `Error` 及其子类；自定义错误继承 `Error` 并设 `name`。
- 禁止浮空 Promise；异步失败必须被 `await`+`try/catch` 或 `.catch` 处理。
- `catch` 变量类型为 `unknown` (见 §1T.0)，使用前必须收窄。

---

## §7.2 日志规约

### §7.2.1 编译期开关 (※本项目强制※)

**日志系统是否开启，必须在编译选项中指定；C++ 侧用宏保证编译期开关。**

- 主开关 **`BARRIEWW_LOGGING`** (默认 ON)：关闭时所有日志宏**编译期展开为 no-op**，
  最终二进制不含任何日志代码/字符串 (服务 T0[1])。
- 热路径日志开关 **`BARRIEWW_CRITICAL_HOTPATH_LOG`** (默认 OFF)：
  - T0[1] 要求热路径 (command 录制) **默认禁止日志 I/O**。
  - **此开关允许在排障时开启热路径日志 I/O**；关闭时热路径日志宏编译为 no-op。
  - 依赖 `BARRIEWW_LOGGING=ON` (见 [BuildSystem §3.3](BuildSystem.md))。
- 两开关登记于 [BuildSystem §3.3](BuildSystem.md) 编译期开关注册表。

### §7.2.2 日志宏使用范式 (C++)

```cpp
// 普通路径日志：受 BARRIEWW_LOGGING 控制
BARRIEWW_LOG_INFO("Vulkan device created: {}", deviceName);

// 热路径日志：受 BARRIEWW_CRITICAL_HOTPATH_LOG 控制，默认编译为 no-op
BARRIEWW_HOTPATH_LOG_TRACE("Recorded draw on lane {}", laneIndex);
```

```cpp
#if BARRIEWW_CRITICAL_HOTPATH_LOG
    #define BARRIEWW_HOTPATH_LOG_TRACE(/* format, args... */) /* real I/O */
#else
    #define BARRIEWW_HOTPATH_LOG_TRACE(/* format, args... */) ((void)0)  // no-op
#endif
```

### §7.2.3 通用日志规则

- 统一日志层级：`TRACE < DEBUG < INFO < WARN < ERROR < FATAL`，语义一致跨模块。
- 统一日志格式 (时间戳、层级、模块标签、消息)。
- **热路径默认禁止日志 I/O** (仅 `BARRIEWW_CRITICAL_HOTPATH_LOG` 开启时例外)。
- 日志不得吞掉错误上下文；ERROR/FATAL 须包含定位所需信息。
- Java 侧热路径同样禁止日志 I/O；日志集中在慢路径。
- TypeScript 侧使用统一 logger 抽象，禁止裸 `console.log` 散落 (lint 强制)。
