# ADR-0004: Java-owned Vulkan bootstrap 与 Native presentation runtime

- **状态**: Accepted (2026-07-23，所有者确认)
- **决策者**: 项目所有者 (Chilliziehen)、Claude
- **关系**: 闭合 ADR-0001 的 device bring-up 开放问题，落实 ADR-0003 D3 的 swapchain
  微内核职责；不放宽“执行期零图解释”。

---

## Context

当前 `dev` 已用真实 GPU 证明 Pure Compute、普通 dynamic-rendering graphics 和 compute
产出参数的单次非索引 `DrawIndirect`，但均为 Native/offscreen tests。首个真实 Java 可视
运行时必须同时验证：Java→Native execution FFM、跨平台 window/surface/swapchain、resize/
minimize/recreation、预录 command buffer 的 steady-state 性能，以及 completed-frame 指标。

Native 不能变成 window system；Java 也不能每帧解释 BECS 或发出逐条 Vulkan command。

## Decision

### D1. Demo 与 bootstrap 归属

- Standalone Demo 位于 `Core` 独立 `demo` source set，不增加顶层模块，不放入 `Mod`。
- Java 使用 LWJGL GLFW，拥有 window/event callbacks、`VkInstance`、`VkSurfaceKHR`、
  physical-device selection、`VkDevice` 和 queues。
- Native 只借用上述 bootstrap handles；拥有 swapchain/views、command pools/buffers、
  semaphores/fences、query pools、materialized modules 与 frame generations。
- Native 不链接 GLFW，不创建/销毁 Java-owned handles。
- teardown：停止 Java frame loop → Native graph/runtime destroy/drain → Java device → surface
  → instance → GLFW window/terminate。

### D2. Surface/swapchain policy

- present mode 配置期查询：优先 MAILBOX，回退 Vulkan 强制支持的 FIFO；基线禁用 IMMEDIATE。
- surface format：优先 `B8G8R8A8_SRGB + SRGB_NONLINEAR`，再
  `R8G8B8A8_SRGB + SRGB_NONLINEAR`，最后采用 surface 首个支持项并记录实际值。
- frames-in-flight 固定 2；image count 取 `minImageCount + 1`，受非零 `maxImageCount` 限制。
- 优先同一个 graphics+present queue family；分族时首版使用 `VK_SHARING_MODE_CONCURRENT`
  和固定 family indices，避免首版引入 ownership-transfer 状态机。实际 sharing mode 进入指标。
- Pure Compute 可视路径要求 surface 支持 `VK_IMAGE_USAGE_TRANSFER_DST_BIT`；不假设 swapchain
  支持 STORAGE，不支持 transfer destination 时明确初始化失败。

### D3. Frame 与 recreation semantics

- Java GLFW callback 维护 framebuffer extent 和单调 resize generation，并在 beginFrame 提供。
- extent 任一维为 0 返回 `SurfaceUnavailable`；Java `glfwWaitEvents()`，禁止 busy loop。
- acquire/present `VK_ERROR_OUT_OF_DATE_KHR` 返回 `RecreateRequired`；`VK_SUBOPTIMAL_KHR`
  允许当前 frame 完成并要求后续 recreation。
- Native 创建 replacement 时传 oldSwapchain，仅等待引用旧 generation 的 frame/image fences；
  steady state 和常规 recreation 不调用 `vkDeviceWaitIdle`。
- 新 swapchain/views/bindings/primary buffers 全部成功后原子发布新 generation，再退休旧 generation。

### D4. Command-buffer hierarchy 与热路径

- graph work 在 load/recreation 录入 secondary command buffers，按 frame slot/lane 固定。
- presentation primary 按 `(frame slot, swapchain image)` 预录：timestamp、execute secondary、
  final copy/transition。

> **2026-07-26 修正（ADR-0006 D4.1 裁决）**：上两条中「secondary command buffers +
> `vkCmdExecuteCommands`」已被取代。图工作与 presentation 一同作为多个 **primary**
> 在同一次 `vkQueueSubmit` 中提交；同批内按 submission order 排序，presentation primary
> 起始的 barrier 覆盖同批更早命令，无需 inheritance info。
> 修正依据：`CommandBufferRecorder` 实现后产出的即为 primary（begin 路径不设
> `VkCommandBufferInheritanceInfo`），已有 61 个用例建立其上；改用 secondary 需改动
> recorder 与全部测试夹具而无对应收益。本条其余内容（按 `(frame slot, swapchain image)`
> 预录、O(1) 选择、每帧禁止 walk/allocation/recording/symbol lookup）不变。
- acquire 后只按两个整数 O(1) 选择预录 primary；每帧禁止 BECS walk、graph walk、allocation、
  command recording 或 symbol lookup。
- 三个场景均是预编译/预加载产物；运行时 scene switch 只选择现成 owner，不构建图。

### D5. Milestone 顺序

1. Java bootstrap + Native swapchain/no-op clear，验证 ownership/resize/teardown。
2. Pure Compute：compute 写 frame-slot storage buffer，primary `CopyBufferToImage` 到 swapchain。
3. Ordinary Graphics：dynamic rendering + direct Draw 到稳定 offscreen image，再 copy 到 swapchain。
4. GPU-driven：compute 写一个 `VkDrawIndirectCommand`，barrier 后执行
   `DrawIndirect(drawCount=1, stride=16)`；CPU 不读回或修补参数。

当前单次非索引 DrawIndirect 是首版 GPU-driven milestone；indexed/count-buffer 是后续 additive
增量，不作为本 ADR 的阻塞项。

### D6. Performance metrics

- CPU 分段计时常驻，仅使用 monotonic clock 和 fixed record：fence wait、acquire、Java parameter
  write、Native submit、present call、total frame interval。
- GPU timestamp 由注册后的 `BARRIEWW_GPU_FRAME_METRICS` 编译期开关控制：production 默认
  OFF，Release Demo 默认 ON；关闭后 query/timestamp code 不进入产物。
- `beginFrame` 返回该 slot 上一次完成 frame 的 metrics，必须携带原 frameSequence 和
  swapchainGeneration；禁止把未完成 frame 称为当前 FPS。
- Java 报告 warmup/sample count、resolution、device/driver、present/sharing mode、frames in
  flight，以及 FPS、median/p95/p99。present blocking 与 GPU work 分开报告。

## Consequences

- 需要先固化 imported-image binding、execution FFM POD/status、frame metrics record 与必要的
  §9 transfer payload；C++ class/std types 不跨 ABI。
- LWJGL 依赖只进入 Core demo configuration，不污染 Core API。
- Linux CI 优先 Xvfb+lavapipe WSI smoke；仍保留 injected swapchain state-machine 和 offscreen
  GPU oracles，visible Windows/Linux 运行作为 milestone acceptance。
- 若后续测量证明 split-family EXCLUSIVE 更优，再通过 ADR 修改 queue ownership policy；不在
  首版同时维护两套同步路径。
