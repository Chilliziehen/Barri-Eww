# §8 生成产物与 ClassLoader 约定

> 适用范围：`Mod/` 中 JIT codegen 产出的字节码、其加载隔离，以及受控扩展机制。
> 服务 T0[1] (性能) 与 T0[2] (可维护)，并保障与 MC/其他 mod 的类空间隔离。

---

## §8.1 生成类命名约定

- 生成的实现 `CompiledRenderPipeline` 接口的类，命名须**唯一且可追溯**：

  ```
  barrieww.generated.CompiledRenderPipeline$<graphHash>
  ```

  - `<graphHash>` 为图数据 (规范化 JSON) 的稳定哈希，保证同图复用、异图隔离。
  - 命名空间固定 `barrieww.generated`，禁止与 MC/其他 mod 包名冲突。
- 生成方法约定 (对齐 project-brief)：`init()` 一次性建资源 (慢路径)，
  `recordLaneN()` 为纯直线热路径 (N 为 lane 序号)。
- 生成类/方法命名同样遵守 §1 (语义完整、禁缩写)；生成器代码中拼接的名字集中定义为常量。

## §8.2 ClassLoader 隔离策略

- **每个编译产物用独立 `ClassLoader` 加载**，与 MC 主类加载器、其他 mod 类加载器隔离，
  防止类空间污染与内存泄漏。
- 卸载/重载管线时，其 `ClassLoader` 及所加载类须可被 GC 回收 (不得被静态引用钉住)；
  与之绑定的 `Arena` 生命周期须随之释放 (对齐 §6.3)。
- 生成类对 Panama handle 的常量引用 (见 §6.1) 的生命周期不得超过其 ClassLoader。

## §8.3 受控扩展机制 (Controlled Extension)

对标 project-brief 的"受控扩展"：真正需要图灵完备表达力的场景，进阶开发者实现约定的
Java 接口、编译为独立 jar 分发，**图 JSON 仅引用节点类型 ID**，由 loader resolve。

- **信任边界**：纯图数据 (JSON) 始终可被完整校验/sandbox；自定义代码 (扩展 jar) 是
  独立信任域，其加载、能力、隔离须显式约定并成文。
- **节点类型 ID → 实现**的 resolve 契约：
  - 图 JSON 只含节点类型 ID (字符串)，不含代码/闭包 (对齐"图是纯数据"架构判断)。
  - loader 依 ID 在已注册扩展中解析实现；**未注册 ID 必须校验期报错** (对齐 §7 慢路径异常)。
  - 扩展 jar 用独立 ClassLoader 加载，与生成产物、MC、彼此隔离。
- opaque native 节点 (如 DLSS 等厂商 SDK 集成) 是受控扩展的一类具体用例，
  其 native 调用同样遵守 §6 (FFM) 的 trivial/critical 与内存所有权约定。

## §8.4 校验与安全

- 加载任何生成产物/扩展前，图数据须通过拓扑与语义校验 (慢路径，见 §4 单测覆盖)。
- 生成字节码在 classload 前应通过 JVM 校验器 (不生成会被 verifier 拒绝的字节码)。
