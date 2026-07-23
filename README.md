# Barri-Eww

A RenderGraph-driven rendering toolchain for Minecraft Java Edition: a visual render
graph is compiled ("JIT") into native Java bytecode plus a flat command stream that
drives a C++ Vulkan backend directly, with no per-frame graph interpretation at runtime.

面向 Minecraft Java 版的 RenderGraph 驱动渲染工具链：把可视化渲染图编译（"JIT"）为
原生 Java 字节码加一段扁平命令流，直接驱动 C++ Vulkan 后端；运行时不做逐帧的渲染图
解释。

---

## Status / 项目状态

**Early development.** The BECS writer/validator/materializer/recorder path is functional
and exercised end to end on a real GPU: compute and indirect dispatch, dynamic-rendering
graphics pipelines, BDA vertex pulling, a committed full-frame module, and compute-produced
non-indexed `DrawIndirect`. Java SE 25 also reaches Native through a real versioned FFM
module-validation boundary. The visual editor, Fabric/Minecraft integration, a visible
window/swapchain execution runtime, full Java-to-Native materialization/execution, and the
DX12/Metal backends are not implemented yet.

**早期开发阶段。** BECS 写侧/校验/材质化/录制链已可用，并已在真实 GPU 上端到端跑通：
计算与间接调度、动态渲染图形管线、BDA 顶点拉取、入库的完整整帧模块，以及 compute 产出
参数的非索引 `DrawIndirect`。Java SE 25 也已通过真实的 versioned FFM 模块校验边界进入
Native。可视化编辑器、Fabric/Minecraft 集成、真实窗口/swapchain 执行运行时、完整
Java→Native 材质化/执行，以及 DX12/Metal 后端尚未实现。

---

## What it is / 这是什么

Barri-Eww is infrastructure for shader/rendering mod developers, aiming at the
Iris/OptiFine ecological niche but replacing fixed hook points with a render graph, so
authors gain full control over the pipeline while keeping integration points for modern
techniques (DLSS, real-time ray tracing, neural rendering). It anchors to the newest
Minecraft Java line (26.2+, experimental Vulkan backend, Java SE 25) and deliberately
does not chase compatibility with older versions.

Barri-Eww 是给光影/渲染 mod 开发者使用的基础设施，对标 Iris/OptiFine 的生态位，但用
render graph 取代固定 hook 点，让作者获得完整的管线控制力，同时为现代技术（DLSS、
实时光追、神经渲染）保留接入点。项目锚定最新的 Minecraft Java 版本线（26.2+，实验性
Vulkan 后端，Java SE 25），并有意不追求对旧版本的兼容。

The scope is intentionally narrow, to avoid the failure mode of over-broad engine
rewrites: an RDG compiler, a native execution backend, and the glue that integrates
with Minecraft's rendering. It is not a window system, not a general material system,
and not a multi-GPU scheduler.

范围刻意收窄，以避免"重写整个引擎"式的失败：一个 RDG 编译器、一个原生执行后端，以及
与 Minecraft 渲染集成的粘合层。它不是窗口系统、不是通用材质系统、也不是多 GPU 调度器。

---

## Core design / 核心设计

**Compile away what compilation can determine.** A conventional render graph keeps a
runtime that reads a topologically sorted execution stream every frame
(`for each pass: execute(pass)`). Barri-Eww instead compiles the graph — ordering,
barriers, resource lifetimes/aliasing, parallel-recording lanes — into straight-line
code. At runtime there is no graph, no pass loop, and no scheduler.

**凡编译期可确定者，不在执行期再确定。** 常规渲染图保留一个运行时，每帧读取拓扑排序后
的执行流（`for each pass: execute(pass)`）。Barri-Eww 则把渲染图——顺序、屏障、资源
生命周期/别名、并行录制分区——编译成直线代码。运行时不存在图、不存在 pass 循环、也
不存在调度器。

**Command-stream IR (BECS).** The compiler's cross-language transport format is a flat,
little-endian, fixed-layout byte stream. Resources are referenced by slot index, never
by raw handle, so the artifact stays pure data: fully validatable, sandboxable, and
backend-neutral. It is fully validated once at load time, then trusted; the recorder
turns it into reusable, prerecorded Vulkan command buffers.

**命令流 IR（BECS）。** 编译器的跨语言运输格式是一段扁平、小端、定长布局的字节流。
资源以槽位索引引用，绝不用原始句柄，因此产物保持纯数据：可完整校验、可沙箱、后端中立。
它在加载期被完整校验一次，此后被信任；录制器将其转为可复用的预录 Vulkan 命令缓冲。

**Microkernel runtime.** Only what genuinely requires a runtime remains: bindless
descriptor heap residency, frame pacing (acquire/present, fences, frames in flight),
queue submission, streamed resource residency, and load-time recording. Everything the
inputs already determine at compile time stays out of the runtime.

**微内核运行时。** 只保留真正需要运行时的部分：bindless 描述符堆驻留、帧节拍
（acquire/present、fence、frames in flight）、队列提交、流式资源驻留、以及加载期录制。
凡输入在编译期已确定者，都不进入运行时。

---

## Architecture / 模块架构

| Module / 模块 | Language / 语言 | Build / 构建 | Role / 职责 |
| --- | --- | --- | --- |
| `Native/` | C++23 | CMake 4.4 | Native rendering backend (Vulkan): validates and records command streams, executes frame pacing and submission. / 原生渲染后端（Vulkan）：校验并录制命令流，执行帧节拍与提交。 |
| `Core/` | Java SE 25 | Gradle | The Java brain: RDG compiler, command-stream writer, and the Panama/FFM binding layer. / Java 大脑：RDG 编译器、命令流写侧、以及 Panama/FFM 绑定层。 |
| `Mod/` | Java SE 25 | Gradle | Thin Fabric loader that injects the JIT-compiled artifacts into the game (not implemented yet). / 薄 Fabric 加载器，把 JIT 产物注入游戏（尚未实现）。 |
| `Editor/` | TypeScript | npm + Electron | Visual render graph editor producing graph JSON (not implemented yet). / 可视化渲染图编辑器，产出图 JSON（尚未实现）。 |

Data flow / 数据流:

```
Visual graph (JSON)             |  可视化图 (JSON)
  |  Core: compile              |    |  Core: 编译
  v                             |    v
Java bytecode + BECS stream     |  Java 字节码 + BECS 命令流
  |  Panama/FFM (load time)     |    |  Panama/FFM (加载期)
  v                             |    v
Native: validate -> record      |  Native: 校验 -> 录制 VkCommandBuffer
  |  microkernel: submit/frame  |    |  微内核: 每帧提交
  v                             |    v
GPU                             |  GPU
```

Java writes a compact command stream into off-heap memory (no FFM call per command);
one call per lane hands it to the native side, which records reusable command buffers
once. Per-frame cost is then dominated by parameter writes into mapped memory and a
single submit, independent of scene complexity.

Java 把紧凑命令流写入堆外内存（每条命令无 FFM 调用）；每个 lane 一次调用把它交给
原生侧，后者一次性录制可复用命令缓冲。此后每帧成本主要是往映射内存写参数加一次提交，
与场景复杂度无关。

---

## Current capabilities / 当前能力

Implemented and covered by tests / 已实现且有测试覆盖:

- BECS command-stream format: module container, per-lane streams, all current resource and
  template tables, dynamic-rendering graphics state, `Draw`, and single-draw
  `DrawIndirect`, with load-time validators/materializers/recorders and dual Java/C++
  writers/readers.
- Cross-language conformance: committed golden binaries asserted byte-exactly by both
  Java and C++, plus a real Java `MemorySegment` → Panama → Native module/lane validation
  boundary with stable checked-error translation.
- Native execution on a real GPU: buffer/image transfers, barriers, frame pacing, compute,
  GPU-generated indirect dispatch, ordinary graphics, BDA vertex pulling, committed
  full-frame-module execution, and compute-produced non-indexed indirect draw.

已实现且有测试覆盖:

- BECS 命令流格式：模块容器、逐 lane 流、当前全部资源/模板表、动态渲染图形状态、`Draw`
  与单次 `DrawIndirect`；具备加载期校验/材质化/录制，以及 Java/C++ 双侧写读。
- 跨语言一致性：入库 golden 由 Java 与 C++ 逐字节共同断言；同一 Java-owned
  `MemorySegment` 还会经真实 Panama 调用进入 Native 模块/lane 校验边界，并稳定转译受检错误。
- 真实 GPU 原生执行：缓冲/图像传输、屏障、帧节拍、计算、GPU 生成的间接调度、普通图形、
  BDA 顶点拉取、完整整帧模块执行，以及 compute 产出参数的非索引间接绘制。

Not implemented yet / 尚未实现: the visual editor, Fabric/Minecraft integration,
visible WSI/swapchain presentation, full Java-to-Native materialization/execution, and
the DX12/Metal backends.

尚未实现: 可视化编辑器、Fabric/Minecraft 集成、可视 WSI/swapchain 呈现、完整
Java→Native 材质化/执行，以及 DX12/Metal 后端。

---

## Building / 构建

Prerequisites / 前置条件:

- CMake 4.4 or newer, a C++23 compiler, and the Vulkan SDK (for `Native/`).
- JDK 25 (for `Core/`; the Gradle wrapper is included).

- CMake 4.4 或更新版本、C++23 编译器、Vulkan SDK（用于 `Native/`）。
- JDK 25（用于 `Core/`；已内置 Gradle wrapper）。

Native backend / 原生后端:

```
cmake -S Native -B Native/build
cmake --build Native/build --config Debug
ctest --test-dir Native/build -C Debug --output-on-failure
```

Core (Java) / Core（Java）:

```
cd Core
./gradlew test
```

Compile-time feature and platform switches are injected via CMake options (for example
`-DTHREADED_RECORDING=ON`, `-DBARRIEWW_BACKEND_VULKAN=ON`); disabled features leave no
runtime branch in the binary.

编译期功能与平台开关通过 CMake 选项注入（例如 `-DTHREADED_RECORDING=ON`、
`-DBARRIEWW_BACKEND_VULKAN=ON`）；关闭的功能不会在二进制中留下任何运行时分支。

The root entry-point contract for `build.sh` / `build.bat` is fixed in §3.5; the scripts
are the next repository-debt increment and are not present in this documentation commit.

根 `build.sh` / `build.bat` 入口契约已在 §3.5 固化；脚本属于下一项仓库债务增量，本次
文档提交中尚未提供。

---

## Repository layout / 目录结构

```
Native/     C++ Vulkan backend            C++ Vulkan 后端
Core/       Java compiler + FFM           Java 编译器 + FFM
Mod/        Fabric loader (planned)       Fabric 加载器（计划中）
Editor/     Visual editor (planned)       可视化编辑器（计划中）
TestData/   Cross-language golden files   跨语言 golden 文件
```

---

## License / 许可证

Released under the MIT License. See [LICENSE](LICENSE).

以 MIT 许可证发布。见 [LICENSE](LICENSE)。
