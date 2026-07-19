# 项目设计意图:MC RenderGraph JIT 渲染管线工具链

## 概述

给Minecraft Java版光影/渲染mod开发者提供一套可视化RenderGraph编辑器,编辑完成后将
渲染图编译("JIT")为一份原生Java字节码包,该字节码包直接驱动一个C++实现的原生渲染
后端(优先Vulkan),通过mod动态加载,替换/接管游戏的世界渲染路径。

目标不是做一个具体的光影效果,而是做**其他光影开发者使用的基础设施**——对标Iris/
OptiFine的shaderpack生态位,但用render graph取代固定hook点约定,给开发者更大的管线
设计自由度,并为DLSS/实时光追/神经渲染等现代技术留出接入点。

## 背景与时间线判断

- Minecraft Java版从26.2(2026年6月)开始引入实验性Vulkan渲染后端,替换沿用17年的
  OpenGL,目标是`Vibrant Visuals`更新的一部分。当前要求Vulkan 1.2 + dynamic
  rendering + push descriptors,后续可能提高。
- 26.x线最低运行时已经是Java SE 25(微软构建的OpenJDK 25),GC也从G1GC换成了
  Generational ZGC。这意味着Panama(FFM API,JEP 454,JDK22转正)在这条版本线上是
  **可直接使用的稳定特性**,不需要预留兼容层等待。
- 项目明确选择锚定这条最新版本线,不追求对旧版本/旧mod生态的最大兼容性。

## 核心架构判断(已确定,不需要重新论证)

1. **图是纯数据,不接受lambda/闭包式自定义command。** 之前的原型用lambda录制
   command,现已确认这与编译期优化(barrier插入、资源别名分配、并行录制分区)互斥
   ——lambda对编译器是黑盒。图节点必须是预制、有限的command原语(draw/dispatch/
   blit/clear/resolve/barrier)+预制pass模板,自定义能力下沉到shader源码(文本,
   本来就是数据)。真正需要图灵完备表达力的场景,走**受控扩展机制**:进阶开发者
   实现约定的Java接口,编译为独立jar分发,图JSON只引用节点类型ID,由loader
   resolve——这样纯图数据依然可以被完整校验/sandbox,自定义代码的信任边界清晰。

2. **JIT的目的是消除CPU侧图解释开销,不是为了缩短编译时间。** 图在ta(光影作者)
   编辑完成后是**静态拓扑**(除非显式设计为permutation/条件分支),因此可以把
   资源版本、barrier插入、生命周期/别名分配、并行录制schedule全部解算到编译期,
   运行时只是一段特化过的直线执行代码,不做任何图遍历或查表。这与UE RDG这类
   运行时per-frame解算图的设计有本质区别,是本项目相对现有方案的核心差异化点。

3. **原生后端用C++,不用现成跨平台抽象层(wgpu/bgfx等)。** 因为目标是让开发者能
   使用各图形API的独有特性(Vulkan扩展、DX12 mesh shader/work graphs、Metal
   TBDR特性),而不是被迫使用抽象层的公共子集。MVP阶段只做Vulkan一个后端,
   DX12/Metal留作后续扩展点,不在当前范围内。

4. **Java↔Native调用走Panama(FFM API),不用JNI/LWJGL。** downcall handle在图
   编译期解析并作为常量嵌入生成的字节码(通过invokedynamic+常量bootstrap,保证
   调用点单态,便于HotSpot C2内联)。高频、非阻塞的command录制类调用
   (vkCmdBindPipeline/vkCmdDraw等)使用`Linker.Option.isTrivial`/critical路径;
   会阻塞或涉及driver同步的调用(vkQueueSubmit/vkWaitForFences等)不能标记为
   critical,需在IR层面提前标注区分。

## 编译管线分层(概览,细节在编译器设计文档中)

```
可视化编辑器(图数据: JSON)
    ↓
IR前端: 资源SSA化(每次写入产生新版本,依赖=版本链)
    ↓
中端优化pass: 死节点消除 → barrier插入 → 生命周期/别名分配(图着色)
             → 并行录制分区
    ↓
后端codegen: 每个IR节点类型 → ASM模板,emit字节码
             (VarHandle写入预分配MemorySegment + invokedynamic调用
             常量downcall handle)
    ↓
产物: 实现 CompiledRenderPipeline 接口的class,独立ClassLoader加载
      (init()一次性建资源走慢路径,recordLaneN()是纯直线热路径)
```

## 明确的非目标(防止范围蔓延——这是从Nova Renderer项目历史学到的教训)

Nova Renderer(C++,2017年前后,试图重写MC Java版渲染器)长期停留在"early
development",从未正式发布。关键败因:①从未有稳定mod加载接口,依赖手动改MC源码
打hook,每次MC更新都要重新维护;②范围从"render graph工具链"一路膨胀到"自研整个
引擎"(自己的材质系统、跨平台构建、多GPU调度等);③无deadline的个人passion
project,复杂度增长速度超过可投入精力。

本项目应严格避免重蹈覆辙:

- **必须挂在Fabric/NeoForge的正式mod接口上**,不手动patch MC源码,不追着Mojang
  内部实现类做mixin(除非官方明确表示这是预期的模组集成方式)。
- **不做窗口系统、不做通用材质系统、不做多GPU调度**。只做:RDG编译器 + native
  执行后端 + 与MC渲染管线的集成粘合层。
- **MVP先于完整功能集**,见下方MVP定义,不要在MVP达成前扩展范围。

## MVP定义(当前阶段目标)

**C++实现渲染后端,JIT生成Java包后成功植入游戏。**

具体验收标准建议(可讨论调整):
1. 一个手写/硬编码的极简RDG(例如:单一geometry pass + 单一fullscreen
   composite pass,固定shader),不要求编辑器可用。
2. 该RDG通过编译管线,产出一个实现`CompiledRenderPipeline`接口的Java class
   (ASM生成,含Panama downcall常量绑定)。
3. C++侧实现最小Vulkan后端,能接收上述Java侧发出的native调用,创建pipeline
   对象、录制command buffer。
4. 通过Fabric mod,在真实运行的Minecraft(26.2+,Vulkan后端开启)中动态加载
   上述产物,替换/接管至少一部分世界渲染,画面上有可验证的实际效果(哪怕只是
   替换clear color或者渲染一个三角形覆盖层)。
5. 全流程走完一次"编译期解析downcall handle→生成字节码→classload→执行"的
   完整链路,不是mock/no-op后端。

暂不要求:可视化编辑器UI、多pass复杂拓扑、资源别名/并行录制优化生效(可以先
正确性优先、性能优化后置)、DX12/Metal后端、DLSS等厂商SDK接入。

## 已知先例/参考(不是抄袭对象,是风险和思路的参照)

- **Iris/OptiFine**:当前MC光影生态标准,固定hook点(gbuffers_*/composite)
  扩展模式,验证了"CPU侧不需要图灵完备,表达力主要在shader里"这个判断。
- **Nova Renderer**:C++重写MC渲染器的失败先例,教训见上方"非目标"。
- **Function Compiler(mcfc,Modrinth)**:MC生态内已知的、真正意义上的JIT
  mod——运行时把datapack函数编译为Java字节码并classload执行。验证了"图/脚本→
  字节码→classload"这条技术路线在MC/JVM环境下可行、可发布、有人用,但问题域
  是命令脚本,不是渲染管线,没有native互操作这一层。
- **Caustica**:26.2 Vulkan后端上的实验性光追渲染器,固定实现,不做用户可编排
  管线,主要参考价值是DLSS集成方式,可作为"受控扩展机制"里opaque native节点的
  第一个具体用例。

## 开发者背景(上下文,便于协作时校准建议深度)

有Unity引擎底层架构团队render pipeline设计经验,对render graph模式、资源生命
周期管理、JIT/codegen动机有扎实理解,不需要基础概念铺垫,可以直接进入具体
工程决策层面讨论。
