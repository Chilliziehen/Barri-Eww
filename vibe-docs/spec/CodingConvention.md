# §1 代码规范

> 适用范围：**C++ 与 Java 同时适用**，除非条款显式标注仅适用一方。
> 违反本规范的代码不得合入。本规范服务于 T0[2] (长期可维护)。

---

## §1.1 命名规范

### §1.1.1 命名法总表

| 目标                     | 命名法                | 前缀   | 示例                                  |
| ------------------------ | --------------------- | ------ | ------------------------------------- |
| 文件名                   | 大驼峰 (PascalCase)   | —      | `RenderGraphCompiler.cpp`             |
| 类 / 结构体 / 接口       | 大驼峰 (PascalCase)   | —      | `CompiledRenderPipeline`              |
| 成员变量                 | 小驼峰 (camelCase)    | `m_`   | `m_deviceHandle`                      |
| 静态变量                 | 小驼峰                | `s_`   | `s_instanceCount`                     |
| 全局变量                 | 小驼峰                | `g_`   | `g_vulkanLoader`                      |
| 局部变量                 | 小驼峰                | —      | `commandBuffer`                       |
| 函数形参 (定义 **与** 实现) | 小驼峰             | —      | `renderPassIndex`                     |
| 函数 / 方法名            | 小驼峰                | —      | `recordDrawCommand`                   |

### §1.1.2 严格规则 (※无例外※)

1. **禁止任何语义缩减 / 缩写。** 写 `context`，不写 `ctx`；写 `descriptor`，不写 `desc`；
   写 `commandBuffer`，不写 `cmdBuf` / `cb`。循环下标等惯例名 (`i`/`j`) 亦应尽量替换为
   语义名 (`passIndex` / `laneIndex`)，仅在纯数学式无语义的紧凑循环中可保留单字母。
2. **函数形参必须使用语义完整的小驼峰命名**，且**函数定义与函数实现中的形参名必须一致**
   (C++ 声明与定义、Java 接口与实现，参数名不得漂移)。
3. 缩写唯一豁免：**行业公认且不可再展开的专有缩写**，如 `Vulkan` API 名 (`vkCmdDraw`)、
   `IR`、`JIT`、`SSA`、`GPU`、`RDG`。新增豁免需登记到本节末尾清单。
4. 布尔量使用 `is` / `has` / `should` / `can` 前缀 (`isTrivialCall`, `hasDepthAttachment`)。

**公认缩写豁免清单：** `Vulkan/vk*`, `DX12`, `Metal`, `IR`, `JIT`, `SSA`, `GPU`, `CPU`,
`RDG` (Render Dependency Graph), `MVP`, `FFM`, `ASM`, `MR`, `CI`, `id` (标识符),
`Info`/`CreateInfo` (仅限镜像 Vulkan `Vk*Info` / `Vk*CreateInfo` 惯用语的类型、参数与
成员命名，2026-07-19 所有者批准)。

> `Info`/`CreateInfo` 豁免的边界：仅当命名镜像图形 API 的惯用概念时可用
> (如 `VulkanContextCreateInfo` 对应 Vulkan 的 `Vk*CreateInfo` 模式)；
> 不得作为任意 `Information` 的通用缩写 (如日志上下文不得写 `logInfo` 之类)。
> 理由：Vulkan 开发者对 `CreateInfo` 的流利度高于展开写法，强行展开反而降低可读性
> (T0[2] 的目的在于可读，而非机械展开)。

---

## §1.2 "一个文件一个类"

- **类 / 结构体 / 接口的定义与实现，按"一个文件一个类"拆分。**
  - Java：一个 `.java` 文件恰含一个顶层类型 (语言强约束)，文件名 == 类名。
  - C++：一个类的声明 (`.hpp`) 与实现 (`.cpp`) 各自独立成对文件，文件名 == 类名。
- **C++ 例外 (仅限非类逻辑)：** 自由函数 (free functions) 允许在单文件内实现多个，
  但文件仍须**按功能划分**并以功能命名 (如 `BarrierUtils.cpp` 汇集 barrier 相关自由函数)，
  不得成为杂物堆 (`Utils.cpp` / `Common.cpp` 之类无功能边界的命名禁止)。
- 嵌套类 / 局部私有 helper 类不受"顶层一文件一类"约束，但应保持在其宿主功能文件内。

---

## §1.3 关键算法注释要求 (与 §2 联动)

关键算法 (barrier 插入、资源 SSA 化、生命周期/别名图着色、并行录制分区、IR→ASM 模板等)
**必须在其实现处注释中：**

1. 详细标注**算法原理**；
2. 给出**语义完整的伪代码** (伪代码中的标识符同样禁止缩写)。

具体格式见 [`CommentConvention.md`](CommentConvention.md) §2.4。

---

## §1.4 C++ 附加约束 (服务 T0[1] 性能 / T0[2] 可维护)

- 资源管理走 RAII，禁止裸 `new` / `delete` (Vulkan 句柄用封装的 RAII wrapper 或显式
  ownership 类型)；所有权语义用类型表达 (`std::unique_ptr` / 自定义 owner 类型)。
- `const` 正确性：不修改状态的成员函数标 `const`，只读参数传 `const &`。
- 平台 / 功能差异走编译期宏 (见 §3)，不写运行时探测分支。
- 头文件不放重量级实现 (除模板 / `constexpr` / `inline` 必要者)，避免编译期膨胀。

## §1.5 Java 附加约束

- Panama/FFM 相关：`Arena`、`MemorySegment`、downcall `MethodHandle` 的所有权与生命周期
  必须显式，且在注释中用 `@warning MemoryOwnership` 标注 (见 §2.3)。
- 热路径 (JIT 生成代码调用的录制方法) 禁止装箱、禁止反射、禁止 lambda 捕获堆对象。
- 生成字节码的 ASM 模板代码，其"生成什么"与"如何生成"应分离并各自注释。
