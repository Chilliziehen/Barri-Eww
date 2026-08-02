# §6 Panama / FFM 绑定层规约

> 适用范围：`Core/` 中一切经 Java Panama (FFM API, JEP 454) 调用 `Native/` 的代码，
> 及 IR 中对 native 调用的标注；`Mod/` 只负责加载 Core 产物和挂接游戏生命周期。
> 这是 Java↔Native 边界，**项目风险最高处**。服务 T0[1] (性能) 与 T0[2] (可维护)。
> 本规约与 §2.3 (`@note ThreadSafety` / `@warning MemoryOwnership`) 互为落地。

---

## §6.1 downcall handle 的解析与常量嵌入 (服务 T0[1])

### §6.1.1 生成热路径

- 生成热路径的 downcall `MethodHandle` 在图编译期 (JIT codegen) 解析完成，不在执行期解析。
- 解析结果**以常量方式嵌入生成字节码**：通过 `invokedynamic` + 常量 bootstrap
  (`ConstantBootstraps` / `condy`) 绑定，**保证调用点单态 (monomorphic)**，便于
  HotSpot C2 内联。
- 执行期热路径**禁止**出现 handle 查表、反射、`Linker` 二次解析。
- 生成代码对 native 函数的每个调用点，其 `FunctionDescriptor` 在编译期确定并作为常量。

### §6.1.2 手写 load/init 慢路径

在 IR/codegen 尚未参与的手写 load/init binding 中，允许在 binding 实例创建时解析一次
native symbol 并创建一次 `MethodHandle`，随后以该实例的不可变成员保存。每次业务调用中
禁止重复 symbol lookup、`Linker` 解析或 `FunctionDescriptor` 构造；handle 不得放入全局
static cache，且生命周期不得超过持有 lookup 的有界 `Arena`。

## §6.2 non-critical / critical 标注 (编译期显式区分，强制)

每个 native 调用**必须在 IR 或手写 binding 声明处显式标注**其调用属性；codegen/binding
据此在构造 handle 时选择固定的 `Linker.Option`：

| 类别         | 判据                                   | JDK 25 处理                                        | 示例                                  |
| ------------ | -------------------------------------- | -------------------------------------------------- | ------------------------------------- |
| critical     | 高频、非阻塞、不涉及 driver 同步       | `Linker.Option.critical(boolean allowHeapAccess)`  | `vkCmdBindPipeline`, `vkCmdDraw`      |
| non-critical | 会阻塞、涉及 driver 同步、分配或长耗时 | **省略** `Linker.Option.critical(...)`             | module validation, `vkQueueSubmit`    |

- **误标 non-critical 为 critical 会导致 JVM 安全点/GC 交互异常**，属严重缺陷。
- IR 节点定义处必须携带该属性字段；codegen 不得对未标注的 native 调用生成代码。
- 手写 binding 须在类型/方法文档中固定分类，不得因调用参数在运行时切换。
- 该属性是**编译期或 binding 创建期决定**，禁止业务调用时判断。
- P19 的 module-validation 调用会遍历不定长模块、排序并分配临时容器，固定为
  **non-critical**，不得传 `Linker.Option.critical(...)`。

## §6.3 跨边界内存所有权矩阵 (强制，落地 §2.3 @warning MemoryOwnership)

每个跨边界传递内存的接口，必须在文档注释中用 `@warning MemoryOwnership` 明确下表信息：

| 场景                              | 分配方 | 释放方 | Arena 归属 / 生命周期约束                     |
| --------------------------------- | ------ | ------ | -------------------------------------------- |
| Java 传 `MemorySegment` 给 Native | Java   | Java   | 由调用侧 `Arena` 持有；调用期间不得关闭 Arena |
| Native 返回指针给 Java            | Native | Native | Java 侧仅借用；不得 `free`；生命周期由 Native 侧对象界定 |
| 编译期常量资源 (init 阶段建立)    | —      | init 慢路径释放 | 与 pipeline 生命周期一致，`CompiledRenderPipeline` 关闭时统一回收 |

- `Arena` 生命周期必须显式且可推理：优先 `Arena.ofConfined` (单线程) 或有界作用域，
  避免 `Arena.global` 泄漏。跨线程共享须用 `Arena.ofShared` 并在注释标明。
- **禁止将短生命周期 Arena 关联的 `MemorySegment`/`MethodHandle` 逃逸到更长生命周期。**
- P19 module validation 是同步借用：Java 拥有 module 与 status segment；Native 仅在调用期间
  读取/写入，不保留 pointer/view。lookup Arena 由 binding 实例拥有，status Arena 每次调用
  单独创建并在返回后关闭。

## §6.4 异常与错误跨边界 (对齐 §7)

- **禁止 Java 异常穿越 FFM 边界**；upcall (若有) 内部必须捕获所有异常并转为错误码/状态。
- **Native 以错误码 / 状态返回**表达失败，Java 侧在慢路径转译为受检异常 (见 §7)。
- Native C ABI wrapper 必须捕获全部 C++ 异常并转为固定的 internal-failure 状态。
- 热路径的 native 调用不做异常控制流。

## §6.5 线程安全标注 (落地 §2.3 @note ThreadSafety)

- 每个 Panama 调用点/包装方法的注释**首行**必须标出所调 Native 侧的并发安全性
  (是否可并发录制、是否需外部同步)，与 `Native/` 侧对应函数的 `@note ThreadSafety` 一致。
- 多线程指令录制 (见 `THREADED_RECORDING`) 的 lane 划分假设必须在两侧注释对齐。

## §6.6 P19 CommandStream module validation Version 1 ABI

P19 只建立 BECM container 与内嵌 BECS lane 的真实 FFM 校验边界，不包含 resource-table
schema validation、Vulkan materialization、command-buffer recording、opaque native owner、
`CompiledRenderPipeline` 或 ClassLoader。接口名必须使用 validation 语义，不得宣称完整 load。

Native shared library target 固定为 `BarriEwwNativeFfm`，唯一 Version 1 entry point 为：

```cpp
enum class NativeCommandStreamModuleValidationOperationResult : std::uint32_t {
    Success = 0,
    InvalidArgument = 1,
    ValidationFailure = 2,
    InternalFailure = 3,
};

struct NativeCommandStreamModuleValidationStatus {
    std::uint32_t moduleValidationErrorCode;      // offset 0
    std::uint32_t laneStreamValidationErrorCode; // offset 4
    std::uint64_t moduleByteOffset;               // offset 8
    std::uint64_t laneStreamByteOffset;           // offset 16
}; // sizeof == 24, alignof == 8

extern "C" NativeCommandStreamModuleValidationOperationResult
barriEwwValidateCommandStreamModuleVersion1(
    const void* moduleBytes,
    std::uint64_t moduleByteSize,
    NativeCommandStreamModuleValidationStatus* validationStatus) noexcept;
```

固定语义：

1. `validationStatus` 非空时，边界在执行其他工作前将全部字段清零；成功时保持全零。
2. `ValidationFailure` 原样承载 `CommandStreamModuleValidationError` 与可选的嵌套
   `CommandStreamValidationError` 稳定数值；`LaneStreamInvalid` 同时携带 module-relative
   lane-section offset 与 lane-relative nested offset。
3. 空 pointer、无法表示为 host `std::size_t` 的长度返回 `InvalidArgument`；意外 C++ 异常
   返回 `InternalFailure`。任何异常均不得穿越 ABI。
4. ABI 仅使用固定宽度标量与 pointer；`std::expected`/`std::span`/`std::optional`/class
   不得跨边界。布局须由 `sizeof`/`alignof`/`offsetof`/standard-layout/trivially-copyable
   compile-time assertions 钉死。
5. Version 1 symbol/layout 一经发布不得原位 breaking 修改；不兼容版本增加新 Version symbol。
6. Java binding 接受明确的 absolute library `Path`，不使用全局 symbol cache、
   `System.loadLibrary` 搜索或 Mod 资源解压。对应生产打包规则留待 Mod feature 固化。

## §6.7 Presentation execution Version 1 ABI（P21/P23）

### §6.7.1 Ownership 与 symbol 集合

Java 创建并拥有 Vulkan bootstrap handles；Native presentation runtime 只借用它们，并通过
versioned C ABI 拥有 swapchain/frame/module resources。Version 1 固定提供语义完整的：

- create/destroy presentation runtime；
- load/destroy one BECS module owner；
- begin frame；
- submit and present frame；
- recreate swapchain；
- query mapped frame-slot region；
- copy completed-frame metrics/failure context。

最终 C symbol 必须带 `Version1` 后缀且使用完整单词；不得暴露 C++ class、`std::*`、Vulkan
struct 或 Java object identity。Native owner 以 opaque address 返回，由 Java final
`AutoCloseable` 独占；destroy 返回后 address 永久失效。Java device/surface/queue 生命周期必须
覆盖 runtime；module backing segment 生命周期必须覆盖 loaded owner，且先 destroy Native owner
再关闭 Java Arena。

所有 create/load/begin/submit/present/recreate/destroy 都是 non-critical downcall；只允许经证明
bounded/nonblocking 的 scalar query 使用 `Linker.Option.critical(false)`。每个 handle 与
`FunctionDescriptor` 在 binding 创建期解析一次，不得进入 frame path。

### §6.7.2 Frame status

正常 frame 状态是值而非异常：`Success`、`SurfaceUnavailable`、`RecreateRequired`、
`Suboptimal`。初始化、materialization、recording 与不可恢复 Vulkan failure 使用 operation
result + stable error + raw VkResult，并在 Java 慢路径转受检异常。Native 必须 containment
全部 C++ 异常；Java exception 不穿越 upcall/downcall。

### §6.7.3 FrameMetricsVersion1（P23）

固定 record 至少包含：`frameSequence`、`swapchainGeneration`、`frameSlotIndex`、`imageIndex`、
`validFlags`、`fenceWaitNanoseconds`、`acquireNanoseconds`、`nativeSubmitCallNanoseconds`、
`presentCallNanoseconds`、`totalCpuFrameIntervalNanoseconds`、`computeGpuNanoseconds`、
`graphicsGpuNanoseconds`、`finalTransferGpuNanoseconds`、`totalSubmittedGpuNanoseconds`、
present result/mode 与 sharing mode。所有字段使用固定宽度；完整 offset/sizeof 在 ABI header
实现前追加到 §6.7 表并由双侧 compile-time/layout assertions 钉死。

metrics 表示**已完成 frame**：`beginFrame` 在复用一个 slot 时返回该 slot 上一次 frame 的记录，
并保留原 `frameSequence`；无效 GPU query 由 validFlags 表达，不返回虚假 0 duration。Java 自行
测 `JavaParameterWriteNanoseconds` 并按 frameSequence 合并。

### §6.7.4 Host-image presentation additive Version 1 ABI（P28）

ADR-0006 D9 第 2 步以 additive Version 1 ABI 导入并采样宿主 color image。该路径属于
presentation interoperability，不进入 BECS、P22 或图模块。

presentation 中立 format 固定为：

| 值 | `PresentationImageFormat` |
| -- | ------------------------- |
| 1 | `R8G8B8A8Unorm` |
| 2 | `B8G8R8A8Unorm` |

新增 `NativeHostImagePresentationRuntimeCreateInfoVersion1`，布局固定为：

| offset | 字段 | 类型 |
| ------ | ---- | ---- |
| 0 | `instanceHandle` | `uint64` |
| 8 | `physicalDeviceHandle` | `uint64` |
| 16 | `logicalDeviceHandle` | `uint64` |
| 24 | `surfaceHandle` | `uint64` |
| 32 | `graphicsQueueHandle` | `uint64` |
| 40 | `presentQueueHandle` | `uint64` |
| 48 | `graphicsQueueFamilyIndex` | `uint32` |
| 52 | `presentQueueFamilyIndex` | `uint32` |
| 56 | `framebufferWidth` | `uint32` |
| 60 | `framebufferHeight` | `uint32` |
| 64 | `framesInFlightCount` | `uint32` |
| 68 | `reservedFlags`（必须为 0） | `uint32` |
| 72 | `hostImageHandle` | `uint64` |
| 80 | `hostImageFormatValue` | `uint32` |
| 84 | `requestedSurfaceFormatValue` | `uint32` |
| 88 | `hostImageWidth` | `uint32` |
| 92 | `hostImageHeight` | `uint32` |

该 record 固定 `sizeof == 96`、`alignof == 8`，必须以 Native compile-time assertions 与
Core layout tests 双侧钉死。新增三个完整命名的 C symbols：

```cpp
NativePresentationRuntimeOperationResult
barriEwwCreateHostImagePresentationRuntimeVersion1(
    const NativeHostImagePresentationRuntimeCreateInfoVersion1* createInfo,
    NativePresentationRuntimeCreateResultVersion1* createResult) noexcept;

NativePresentationRuntimeOperationResult
barriEwwDetachHostImagePresentationResourcesVersion1(
    std::uint64_t runtimeAddress) noexcept;

NativePresentationRuntimeOperationResult
barriEwwSubmitAndPresentHostImageFrameVersion1(
    std::uint64_t runtimeAddress,
    NativePresentationSubmitFrameResultVersion1* submitResult) noexcept;
```

固定语义：

1. 旧 `NativePresentationRuntimeCreateInfoVersion1`、旧 symbol 与 status record 原样保留；
   禁止复用旧 `reservedFlags` 传递 host-image 数据。
2. Native 借用 host `VkImage`，不拥有 image 或 memory；Native 只拥有为 sampled presentation
   创建的 view、descriptor、pipeline 与 command buffers，且必须在宿主销毁 image 前退休。
3. `hostImageHandle` 非零；host、framebuffer 与实际 swapchain extent 必须 exact match；format
   必须为上述中立 UNORM 值。
4. `requestedSurfaceFormatValue` 必须由 surface 以 `SRGB_NONLINEAR` color space 实际支持；
   不允许静默替换为 SRGB format，也不在 shader 内做色彩空间抵消。
5. detach 幂等；等待仍可能引用 host resources 的已提交 frame-slot fences 后销毁 host
   command matrix、pipeline、descriptor、sampler 与 sampled view，但保留 swapchain、frame sync、
   open acquired frame 与 clear path。frame open 时禁止等待刚 reset 且尚未 submit 的当前 slot fence。
6. create、detach 与 submit 涉及分配、driver synchronization 或 queue operation，FFM downcall固定
   non-critical；binding 构造期解析一次 handle，frame path 不做 symbol lookup。
7. submit 要求已有 open frame 且 host resources 仍 attached，并以该 frame 的固定 slot/image
   index 选择预录 command；
   operation/result/exception containment 与既有 presentation Version 1 规则一致。
8. 旧 clear/standalone create binding 不得为普通 runtime 强制解析新增 host-image symbols；
   只有 host-image factory 在构造对应 binding 时解析并固定持有新增 create/detach/submit handles。
   因此旧 Core → 新 Native 与新 Core clear path → 旧 Native 均保持可用。
