# §6 Panama / FFM 绑定层规约

> 适用范围：`Mod/` 中一切经 Java Panama (FFM API, JEP 454) 调用 `Native/` 的代码，
> 及 IR 中对 native 调用的标注。这是 Java↔Native 边界，**项目风险最高处**。
> 服务 T0[1] (性能) 与 T0[2] (可维护)。本规约与 §2.3 (`@note ThreadSafety` /
> `@warning MemoryOwnership`) 互为落地。

---

## §6.1 downcall handle 的编译期解析与常量嵌入 (服务 T0[1])

- **downcall `MethodHandle` 在图编译期 (JIT codegen) 解析完成**，不在运行时解析。
- 解析结果**以常量方式嵌入生成字节码**：通过 `invokedynamic` + 常量 bootstrap
  (`ConstantBootstraps` / `condy`) 绑定，**保证调用点单态 (monomorphic)**，便于
  HotSpot C2 内联。
- 运行时热路径**禁止**出现 handle 查表、反射、`Linker` 二次解析。
- 生成代码对 native 函数的每个调用点，其 `FunctionDescriptor` 在编译期确定并作为常量。

## §6.2 trivial / critical 标注 (IR 层显式区分，强制)

每个 native 调用**必须在 IR 层显式标注**其调用属性，codegen 据此选择 `Linker.Option`：

| 类别         | 判据                                   | 处理                                        | 示例                                  |
| ------------ | -------------------------------------- | ------------------------------------------- | ------------------------------------- |
| trivial      | 高频、非阻塞、不涉及 driver 同步       | 标 `Linker.Option.isTrivial` (critical 路径) | `vkCmdBindPipeline`, `vkCmdDraw`      |
| non-trivial  | 会阻塞 / 涉及 driver 同步 / 长耗时     | **禁止**标 trivial/critical                 | `vkQueueSubmit`, `vkWaitForFences`    |

- **误标 non-trivial 为 trivial 会导致 JVM 安全点/GC 交互异常**，属严重缺陷。
- IR 节点定义处必须携带该属性字段；codegen 不得对未标注的 native 调用生成代码。
- 该属性是**编译期决定**，禁止运行时判断。

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

## §6.4 异常与错误跨边界 (对齐 §7)

- **禁止 Java 异常穿越 FFM 边界**；upcall (若有) 内部必须捕获所有异常并转为错误码/状态。
- **Native 以错误码 / 状态返回**表达失败，Java 侧在慢路径转译为受检异常 (见 §7)。
- 热路径的 native 调用不做异常控制流。

## §6.5 线程安全标注 (落地 §2.3 @note ThreadSafety)

- 每个 Panama 调用点/包装方法的注释**首行**必须标出所调 Native 侧的并发安全性
  (是否可并发录制、是否需外部同步)，与 `Native/` 侧对应函数的 `@note ThreadSafety` 一致。
- 多线程指令录制 (见 `THREADED_RECORDING`) 的 lane 划分假设必须在两侧注释对齐。
