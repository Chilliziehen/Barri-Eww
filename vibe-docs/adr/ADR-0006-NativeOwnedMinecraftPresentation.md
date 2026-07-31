# ADR-0006: Native-owned Minecraft presentation

- **状态**: Accepted (2026-07-31，所有者逐项裁决 D1–D6、D8–D10)
  - **D2 接管点**：D9 第 1 步的实现细节已定案为仅注入 `VulkanGpuSurface` 后端；
    保留外层 `GpuSurface` 状态机自然满足两个 `isAcquired()` 门控与 blit 后 present 前置条件。
  - **D7 GUI alpha**：`待实测`，由 D9 第 3 步给出答案；**D8.4 的混合公式在其定案前不完备**，
    实现时不得自行假定。
  - 除上述两项外，本 ADR 条款为已批准决策。
- **决策者**: 项目所有者 (Chilliziehen)、Claude
- **关系**: 把 ADR-0004 的 Native-owned presentation 模型延伸到 Minecraft 宿主，
  **不引入第二种 presentation 模式**；沿用 ADR-0003 的执行期微内核约束与 P22
  ImportedImageBinding 契约。与 ADR-0005(场景数据/DSL)正交。

---

## Context

ADR-0004 已固化 standalone 形态：宿主(Java/GLFW)拥有 window/instance/surface/device/
queues，Native 拥有 swapchain、image views、per-frame 同步与 generation。

Minecraft 26.2 的现实是 `com.mojang.blaze3d.vulkan.VulkanGpuSurface` 自行持有 surface、
swapchain、swapchain images、acquire/present semaphores 与 present queue。一个 surface 上
不允许同时存在两个活动 swapchain，因此必须二选一。

曾评估的替代方案 **Minecraft-hosted presentation**(Blaze3D 保留 swapchain，Barri-Eww
只替换世界渲染)已被否决。否决理由**不是**"避免图像跨 API 回传 Java"——两种方案都不
存在图像回传，跨边界的只有句柄，内容全程驻留显存。真实理由是：

1. 单一 presentation 模型，ADR-0004 的运行时直接适用，不必长期维护双宿主模式；
2. 完整掌握 present，为 HDR/PQ、frame pacing、frame generation、latency marker 预留空间，
   将来落地不需要再次改动架构；
3. 帧循环不从属于 Minecraft 的提交时机。

需要明确记录的是：**Native 完全接管并不能消除跨边界的图像共享，只是把方向反转**。
Minecraft-hosted 是我方写入 MC 拥有的图；Native-owned 是 MC 的 GUI 图交由我方合成。
两者的导入机制与 P22 契约完全一致。

### 实测依据(2026-07-26 反编译 MC 26.2)

帧执行流：

```text
Minecraft.runTick()
├─ windowSurface.acquireNextTexture()                        Minecraft.java:1245
├─ gameRenderer.extract(deltaTracker, advanceGameTime)       Minecraft.java:1277
├─ gameRenderer.render(deltaTracker, advanceGameTime)        Minecraft.java:1283
│   ├─ LevelRenderer.render(...)
│   └─ 手 / HUD / GUI ────────────────────→ mainRenderTarget
├─ colorTexture = gameRenderer.mainRenderTarget().getColorTextureView()
│                                                            Minecraft.java:1289
├─ windowSurface.blitFromTexture(encoder, colorTexture)      Minecraft.java:1294
└─ windowSurface.present()                                   VulkanGpuSurface.java:404
```

四项关键事实：

1. **acquire 先于 render**(`Minecraft.java:1245`)。`imageIndex` 在图执行前已知，
   O(1) 选择预录命令缓冲成立，执行期无需任何录制。
2. **`mainRenderTarget` 句柄按 generation 稳定**：`GameRenderer` 的
   `private final RenderTarget mainRenderTarget`，构造期 `new MainTarget(w, h)`
   (`GameRenderer.java:163`)，此后仅 `resize()`(`GameRenderer.java:296`)。
   每帧变化的是内容而非句柄。
3. **Minecraft 每帧 clear 该 target**，使用 `guiRenderState.clearColorOverride`
   (`GameRenderer.java:407`)。
4. **`VulkanCommandEncoder` 暴露可注入接缝**：`waitSemaphore` / `execute(VkCommandBuffer)`
   / `signalSemaphore` / `submit`(`VulkanCommandEncoder.java:155/164/173/198`)。
   Minecraft 自身的 blit 正是经此路径提交(`VulkanGpuSurface.java:398-400`)。
5. **Minecraft 全程将纹理保持在 `VK_IMAGE_LAYOUT_GENERAL`**：创建时即
   `UNDEFINED(0) → GENERAL(1)`(`VulkanGpuTexture.java:47/62-63`)且此后不再转换；
   `blitFromTexture` 对源纹理**不发任何 barrier**，直接以
   `srcImageLayout = 1 (GENERAL)` 调用 `vkCmdBlitImage`(`VulkanGpuSurface.java:366`)。
   Minecraft 以「一律 GENERAL」换取免除布局跟踪。
6. **main render target 的 color texture 含 `SAMPLED` usage**：`MainTarget` 以
   `usage = 15` 创建(`MainTarget.java:79`)，按 `VulkanConst.textureUsageToVk` 展开为
   `TRANSFER_DST | TRANSFER_SRC | SAMPLED | COLOR_ATTACHMENT`。因此合成可直接采样该纹理，
   无需先行拷贝。

## Decision

### D1. Presentation 所有权：Native 完全接管 `已裁决`

- Minecraft 不得在 surface 上创建或驱动 swapchain。
- **Native 拥有**：swapchain 与 image views、acquire/present、per-frame 同步对象、
  frame slot、presentation generation 与 recreation。
- **Minecraft 保留**：`VkInstance`、physical device、`VkDevice`、queues、`VkSurfaceKHR`
  ——与 ADR-0004 中 Java bootstrap 的角色同构。
- 不引入第二种 presentation 模式；ADR-0004 的所有权矩阵在 Minecraft 宿主下按上述映射
  继续有效。

### D2. D9 第 1 步接管点 `已裁决(实现细节)`

D9 第 1 步严格在 `VulkanGpuSurface` 后端层注入，不取消外层 `GpuSurface`、
`Minecraft.renderFrame` 或 `LevelRenderer`，也不注入 `VulkanCommandEncoder`：

| 接管点 | 后端位置 | D9 第 1 步动作 |
| ------ | -------- | -------------- |
| swapchain 创建 | `VulkanGpuSurface.configure` | generation readiness 完整且 Native 已呈现 priming 帧后，取消本次后端 configure |
| acquire | `VulkanGpuSurface.acquireNextTexture` | Native `beginFrame` 后取消后端 acquire |
| blit | `VulkanGpuSurface.blitFromTexture` | taken-over generation 中取消为空操作 |
| present | `VulkanGpuSurface.present` | Native 清色并 present 后取消后端 present |
| teardown | `VulkanGpuSurface.close` | 在 vanilla teardown 前关闭 Native coordinator，不取消 vanilla close |

外层 `GpuSurface` 先调用后端、再更新自身状态：configure 后保存 configuration，acquire 后置
`hasImageAcquired=true` / `hasBlittedTexture=false`，空 blit 后置 `hasBlittedTexture=true`，present
后清除 acquired。因此两个 `isAcquired()` 门控和「必须先 blit 才能 present」前置条件自然成立；
实现不得伪造 `GpuSurface` 私有 wrapper 状态，也不得伪造 `VulkanGpuSurface.currentImageIndex`。
接管期间 vanilla swapchain 保持为 0。

configure 取消属于 generation 决策，且只发生在 coordinator 已创建、acquire、清色并实际
present 一个 Native priming 帧之后。readiness 不完整或安全关闭旧 generation 后 replacement
失败时，本次不取消，允许 vanilla 创建 swapchain；旧 generation 关闭失败、Native 所有权不确定
时继续取消，避免同一 surface 上出现双 swapchain。

MC26.2 的 resize 顺序已实测并由字节码确认：`Minecraft.runTick` 先以新 window extent 调用后端
`configure`，`GameRenderer.render` 随后才把 `mainRenderTarget` resize 到新 extent，且该 resize
之后没有第二次后端 configure。D9 第 1 步不导入或读取宿主纹理，因此此阶段只校验 view/texture
非空、color aspect、`COPY_SRC | TEXTURE_BINDING` usage、正数二维单 layer 与有效 mip 等静态属性；
Native runtime 始终使用本次 `GpuSurface.Configuration` 的新 extent。D9 第 2 步起一旦导入并合成
宿主纹理，必须在 target resize/generation 通知之后对 imported generation 强制执行 exact extent
校验；本阶段澄清不修改 D8.2 的合成契约。

### D3. RDG 与 presentation 的职责边界 `已裁决`

**采用单一交接点：图产出 output image，presentation 侧持有其余一切。**

| 归属 | 内容 |
| ---- | ---- |
| **RDG(编译期)** | 图内资源、barrier 计算、命令录制；**分配并拥有 output image**(每帧槽一张) |
| **Presentation runtime(宿主特定)** | swapchain 与 image views、宿主 GUI 纹理导入、合成 pipeline 及其固定 barrier 集合、acquire/present、fence/semaphore、frame slot、generation、recreation |

交接契约固化于 **§9.17 GraphOutputTable**：format/extent/final layout/帧槽多重性。
swapchain image 与宿主 GUI 纹理**不进入 BECS**。

#### 论证订正

本 ADR 初稿曾以「barrier 必须编译期计算，故这两个资源必须进 RDG 体系」为决定性理由。
**该论证不成立，特此订正**：presentation 的 barrier 是封闭集合(swapchain image 只有
`UNDEFINED`/`PRESENT_SRC` → 写入布局 → `PRESENT_SRC`；宿主 GUI 纹理只有「宿主写完 → 我方读」)，
不随图拓扑变化，硬编码于 C++ 同样是编译期确定的，T0 两种方案均满足。需要编译器计算 barrier
的是**拓扑可变**的图内部，不是固定的收尾段。

#### 真实理由：图的可移植性

选择单一交接点的决定性理由是**图必须能在 standalone、Minecraft 宿主与离屏测试中原样执行**。
若 swapchain image 与宿主 GUI 纹理进入图的资源模型，世界图即与 Minecraft 强耦合：离屏 GPU
测试须伪造宿主纹理，或图按宿主分叉。本项目的正确性建立在 GPU 级测试之上，该代价将长期偿付。

次要理由：presentation 的 barrier 集合封闭，手写一次、验证一次即可长期稳定；RDG 编译器
不必处理 swapchain image 的多重性展开；与 ADR-0004 已建成的实现一致，改动最小。

已评估并否决的替代方案：**全进 RDG**(swapchain image 与 GUI 纹理作为 imported resource
进图、合成为图的尾段)——牺牲图的可移植性，且迫使纯离屏模块也携带 presentation 尾段。

#### 零拷贝取舍

「全进 RDG」理论上允许世界直接写入 swapchain image，省去一次全屏读写。该优势被判定为不成立：
任何带后处理(tone mapping / bloom / TAA)的真实管线本就需要中间图像，无法直接写 swapchain；
仅「无后处理且不显示 UI」这一边缘场景可受益，不足以支撑第二条代码路径。

#### 可能的演进方向（非承诺）

若 presentation 侧的合成逻辑将来复杂到需要美术可控(见 D8)，一种可选形态是把 presentation
段本身表达为**第二张图**(世界图保持可移植，宿主特定的合成图导入 GUI 纹理与 swapchain image，
两者由同一 RDG 编译器编译)。**此为备选方向，非既定演进路径**——presentation 图化的需求是否
真实出现尚不确定，不得据此提前引入图组合机制。

### D4. 预录矩阵与查询面 `已裁决`

#### D4.1 图工作载体：多 primary 同批提交，不使用 secondary

**修正 ADR-0004 D4**：图工作不录入 secondary command buffer，而是与 presentation primary
一同作为多个 **primary** 提交：

```text
graphPrimary[frameSlot][lane]          图编译产物，与 imageIndex 无关
presentationPrimary[frameSlot][imageIndex]

vkQueueSubmit(queue, { graphPrimary[frameSlot][*], presentationPrimary[frameSlot][imageIndex] })
```

理由：

1. **现有实现已产出 primary**。`CommandBufferRecorder` 的 begin 路径不设
   `VkCommandBufferInheritanceInfo` 亦不设 `RENDER_PASS_CONTINUE`，测试夹具按
   `VK_COMMAND_BUFFER_LEVEL_PRIMARY` 分配；截至本 ADR 已有 61 个用例、3000+ 断言建立其上。
   改用 secondary 需改动 recorder begin 路径与全部夹具，并重新验证 dynamic rendering 在
   secondary 中的行为。
2. **排序机制与 D6 同源**：同批内命令缓冲按 submission order 执行，presentation primary
   起始的 barrier 覆盖同批更早的命令，无需 `vkCmdExecuteCommands`，亦无 inheritance info。
3. **所有权更清晰**：图拥有自己的 primary，presentation 拥有自己的 primary，提交仅为数组
   拼接；任一方都不录制对方的命令。

代价：`VulkanPresentationRuntime::submitAndPresentFrame` 的参数由单个 `VkCommandBuffer`
改为 `std::span<const VkCommandBuffer>`。该改动同样使 standalone 侧天然支持多 lane。

图的 primary 与 `imageIndex` 无关，因此世界渲染命令不随 swapchain image 数量(2–4)复制；
只有 presentation primary 按 `imageIndex` 展开。

#### D4.2 presentation primary 的固定构成

```text
timestamp begin
barrier hostGuiTexture → 合成读取布局        // 兼作与宿主提交的同步点（D6）
barrier graphOutput    → 合成读取布局
<composite graphics pass> → swapchainImage[imageIndex]
barrier swapchainImage → PRESENT_SRC
timestamp end
```

#### D4.3 查询面

presentation 对已材质化模块的查询**全部发生在装载期**，结果快照进 presentation 自有的
定长数组；每帧仅按 `frameSlot` 索引，无查询、无分配。

采用普通 C++ 访问器（`std::span` 传递命令缓冲序列），不做定长 POD ABI 记录——该接口位于
Native 内部两个子系统之间，既不跨 FFM 也不跨版本边界：

```text
frameSlotCount()                       // = §9.17 outputCount
commandBuffers(frameSlot) -> std::span<const VkCommandBuffer>
outputImage(frameSlot)    -> VkImage
outputFinalLayout()       -> VkImageLayout      // §9.17 header
outputFormat() / outputExtent()
```

依赖方向严格单向：presentation 知晓模块的输出契约，**模块不知晓 presentation 存在**——
模块内不含 swapchain、宿主类型与合成逻辑。

#### D4.4 output 的 sampled view 由 presentation 构造

§9.17 只暴露 image slot。图为渲染进 output 自有 color-attachment view；合成所需的 sampled
view 由 presentation 自行创建并拥有，销毁时机随 presentation generation。理由：view 开销
极低，且避免让图为自身不使用的用途声明 view。

#### D4.5 合成为图形 pass；presentation 不受 §9.8 约束

`vkCmdBlitImage` 无法进行 alpha 混合，而世界在下、宿主 GUI 在上必须混合，故合成必须是
一次全屏图形 pass（采样两张图像并 blend）。

采样图像必须经 descriptor set（buffer device address 无法取得 sampled image）。**合成管线
不在图内**（D3 的直接后果），属 presentation 侧手写实现，不经 BECS，因此**不受 §9.8
「v0.1 管线布局仅 push constants、资源经 device address 到达着色器」的约束**，可自由使用
descriptor set。为支持合成而向 BECS ABI 增加 descriptor table 是不必要的。

### D5. 宿主纹理导入契约 `已裁决`

#### D5.1 导入范围：仅 color texture

只导入 `gameRenderer.mainRenderTarget()` 的 **color texture**，不导入 depth texture。
合成为 2D alpha 混合，不需要宿主深度。

#### D5.2 不走 P22；这是 presentation 侧的借用句柄

**订正初稿**：初稿称「以 P22 ImportedImageBinding 导入，按 importIdentifier 绑定」。
该表述与 D3 冲突并已废止——P22 是 **BECS** 的 imported entry 机制(`importIdentifier`
位于 BECS image table)，而 D3 已裁决宿主 GUI 纹理**不进入 BECS**。

三类资源的归属如下，presentation 路径**完全不依赖 P22**：

| 资源 | 归属 | 进入 BECS |
| ---- | ---- | --------- |
| graph output image | RDG 创建并拥有；image table 内的 **created** 条目 | 是（§9.17） |
| swapchain images | presentation 拥有 | 否 |
| 宿主 GUI 纹理 | presentation **借用** | 否 |

宿主 GUI 纹理仅是 presentation 侧持有的一个借用 `VkImage` 句柄，其校验契约为 presentation
内部实现，无 `importIdentifier`、无 BECS binding record。P22 仍为将来的**图级**导入
（例如图需采样 Minecraft 方块图集）所需，但**不是本路径的前置依赖**。

#### D5.3 布局策略：保持 GENERAL，只发内存 barrier

依 Context 事实 5，宿主纹理恒为 `VK_IMAGE_LAYOUT_GENERAL`。我方**不做布局转换**，
只发内存 barrier（`srcAccess = MEMORY_WRITE` → `dstAccess = SHADER_READ`），
采样时仍以 GENERAL 为布局。

决定性理由是**宿主对该纹理的其他用途同样假定 GENERAL**，不止 blit：

```java
postChain.process(this.mainRenderTarget, this.resourcePool);   // GameRenderer.java:234 / :431
Screenshot.takeScreenshot(this.mainRenderTarget, ...);         // GameRenderer.java:473
```

后处理链与截图都会读取它。若我方将其转为 `SHADER_READ_ONLY_OPTIMAL` 而未转回，这些路径
即进入未定义行为，且症状（截图花屏、后处理异常）与渲染主线索无关，极难定位。保持 GENERAL
使之**构造上安全**，而非依赖「记得转回」的约定。D10 的延迟回退路径同样受益。

代价：GENERAL 下的采样在部分硬件上略逊于 `SHADER_READ_ONLY_OPTIMAL`。该代价为一次全屏
采样，可忽略。

**禁止**以 `VK_IMAGE_LAYOUT_UNDEFINED` 作为 `oldLayout`——那将丢弃图像内容，GUI 随之消失。

#### D5.4 Generation 触发：hook `resize()` + 句柄比对兜底

- **主路径**：Mod 侧 mixin 钩住 `RenderTarget.resize()`，通知 presentation 换代。
  无每帧开销。
- **兜底**：generation 校验时比对 `VkImage` 句柄（指针比较，开销可忽略），防止宿主在
  未钩住的路径上重建纹理。
- 换代时重建导入绑定、sampled view 与依赖其句柄的预录命令缓冲；沿用 ADR-0004 D3 的
  old-swapchain 与 generation 退休策略，不依赖全局 `vkDeviceWaitIdle`。
- 句柄在一个 generation 内不变（Context 事实 2），故可被预录命令缓冲直接引用。
- 若未来实测发现该纹理改为来自 `GraphicsResourceAllocator` 池化分配，回退方案为按
  `(frameSlot, textureIndex)` 展开预录矩阵，与 swapchain image 同法处理。

#### D5.5 已知限制：手部遮挡不正确

取消 `LevelRenderer.render` 后，Minecraft 的深度缓冲内不含世界深度，而**手部**由
Minecraft 以深度测试绘入 main render target。缺少世界深度时手部将无条件绘出；合成为
「宿主 GUI + 手 覆盖于世界之上」，因此**手部恒在最前**。

绝大多数情形下观感正确（手本就贴近相机），但**玩家嵌入方块、手部本应被遮挡时会穿模**。

该项**不阻塞 MVP**，在此记录以免日后被反复当作缺陷排查。将来修正的两条路径：将我方深度
提供给 Minecraft 的手部渲染，或将手部一并纳入我方渲染。

### D6. 提交边界：独立提交 `已裁决`

**Barri-Eww 自行 `vkQueueSubmit`，完全不注入 Minecraft 的 `VulkanCommandEncoder`。**
首版即终局形态，无后续迁移成本。

#### 决定性依据：Minecraft 的提交与呈现只隔一行

```java
RenderSystem.getDevice().createCommandEncoder().submit();   // Minecraft.java:1308
if (this.windowSurface.isAcquired()) {
    this.windowSurface.present();                            // Minecraft.java:1310  ← 接管点
}
```

Minecraft 每帧只有这一次 `submit()`(`blitFromTexture` 在 `:1294` 仅录入 submissionBuilder，
不提交)。我方在 `present()` 接管点提交时，Minecraft 的全部 GUI 工作已提交完毕。

由此，**同步无需任何 semaphore 交互**：Vulkan 的 submission order 语义规定 pipeline barrier
的第一同步域覆盖同一队列上提交顺序更早的所有命令，因此我方 primary 起始处的一次 barrier
(src = `COLOR_ATTACHMENT_OUTPUT | TRANSFER`，srcAccess = `COLOR_ATTACHMENT_WRITE |
TRANSFER_WRITE`)即与 Minecraft 的 GUI 写入建立执行与内存依赖。

#### 接管点与现有 API 一一对应

| Minecraft 方法 | Barri-Eww 动作 |
| -------------- | -------------- |
| `acquireNextTexture()` | `beginFrame(width, height)` — 等 frame-slot fence + 以我方 semaphore acquire |
| `blitFromTexture(...)` | 空实现 |
| `present()` | `submitAndPresentFrame(primary[frameSlot][imageIndex])` |

`VulkanPresentationRuntime` 无需任何改动：fence 仍为 fence，与 standalone 共用同一条代码路径。

#### 前提条件

同队列。β 的 barrier 同步依赖我方与 Minecraft 使用同一 graphics queue；按 D1 我方借用
Minecraft 的 device 与 queues，该前提天然成立，但实现中必须显式断言，不得默认。

#### 已评估并否决：方案 α（注入 Minecraft 的 encoder）

初稿曾建议首版采用 α(`encoder.waitSemaphore` / `execute(ourPrimary)` / `signalSemaphore`，
我方 present)。**该建议被推翻**，否决理由：

1. **`VulkanCommandEncoder` 无 `signalFence` API**(仅 `waitSemaphore` / `execute` /
   `signalSemaphore` / `submit`)。走 α 则我方 frame-slot fence 必须迁移为 timeline semaphore，
   导致 presentation runtime 出现 standalone(fence)与 Minecraft(timeline)两套同步路径——
   正是 D3 已否决的双模式问题。
2. **继承 Minecraft 的节流**：`submit()` 内含
   `awaitSubmitCompletion(currentSubmitIndex - 2)` 主机等待，Minecraft 自身已做 2 帧深流控；
   叠加我方 frame slot 形成双重节流，且提交时机受宿主控制。
3. α 的唯一优点(「Minecraft 的 blit 本就是该形状」)不成立：**β 完全不触碰 Minecraft 的
   encoder，耦合更少而非更多**。

保留此记录，以免日后重新提出「注入宿主 encoder 更省事」。

### D7. GUI alpha 与 clear 策略 `待实测`

取消 `LevelRenderer.render` 后，`mainRenderTarget` 仅含手/HUD/GUI，而 Minecraft 每帧以
`guiRenderState.clearColorOverride` clear 该 target(`GameRenderer.java:407`)。若 clear 为
不透明色，合成时我方世界将被完全遮蔽。

候选方案：(a) 将 GUI 重定向至我方持有的 render target；(b) 修改 `clearColorOverride`
使其 clear 为透明。**该项不得以推理定案，必须在原型阶段实测确认。**

### D8. 合成规则 `已裁决`

#### D8.1 结构固定，不对 TA 开放

MVP 阶段合成为**一次全屏图形 pass**：采样 graph output 与宿主 GUI 纹理，写入
swapchain image；世界在下、宿主 GUI 在上。该 pass 为 presentation 侧手写实现，不对 TA
开放，亦不进入图。

推迟开放**不承诺任何实现机制**——是否、以及以何种形式开放，待 HDR/色彩空间或 frame
generation 产生真实需求时另行裁决（参见 D3「可能的演进方向（非承诺）」）。

#### D8.2 输入约束：三者 extent 必须相等

graph output、宿主 GUI 纹理与 swapchain image 的 extent 必须**完全相等**，装载期校验；
不等则 readiness 关闭（D10）。**MVP 不做缩放**。

该约束不构成实际限制：宿主 main render target 按窗口尺寸创建（`GameRenderer.java:163`
及 `:296` 的 resize），swapchain 亦然，graph output 由我方控制。渲染缩放/超分作为后续
特性，届时再引入合成期缩放。

#### D8.3 MVP 不做色彩空间转换

合成 pass 只做混合，不做 tone mapping、不做色彩空间变换。graph output 与 swapchain 的
格式必须兼容，装载期校验。HDR 落地时本条重写，届时由真实需求驱动。

#### D8.4 混合公式依赖 D7

本条仅固定合成的**结构**。具体 blend factor 取决于宿主 GUI 纹理的 alpha 语义，
须待 D7 实测后填入。**D8 在 D7 定案前不完备**，实现时不得自行假定混合公式。

### D9. 四步验证路径 `已裁决`

**每一步只改变一个变量**，各步独立可回退：

| 步 | 改变什么 | 判据 | 验证内容 |
| -- | -------- | ---- | -------- |
| 1 | 接管 present，清为纯色 | 出现纯色、不崩溃、resize 与 alt-tab 正常 | 接管机制、`isAcquired()` 门控(D2)、generation 换代、teardown |
| 2 | 导入宿主纹理并合成（世界仍由 Minecraft 渲染） | **画面与原版一致** | 导入契约(D5)、GENERAL 布局策略、跨提交同步(D6) |
| 3 | 取消世界渲染，宿主 GUI 合成于**纯色**背景之上 | 纯色背景 + GUI 正常显示 | **D7 的 alpha 语义在此单独暴露** |
| 4 | 纯色背景替换为 graph output | 世界渲染正确 | 交接契约(§9.17)、图执行 |

第 2 步的「与原版一致」判据把「接管 + 导入 + 同步」与「我方图的渲染正确性」完全解耦，
是本 ADR 首选的正确性判据。

第 3 步为必需：第 2 步中宿主 main render target 含完整世界因而完全不透明，合成退化为
拷贝，**不触发 alpha 问题**。若省略第 3 步，D7 的 alpha 语义与图自身的正确性将在第 4 步
同时引入而相互混淆。第 3 步的成本仅为把第 2 步的世界来源换成 clear。

#### D9.1 2026-07-31 owner-approved step 1 empirical record

本记录是 Windows 真实窗口的**经验验收**，不是 CI 自动化结果。Native clear 输入为线性
`(0.08, 0.72, 0.93)`；按标准分段 raw-linear → sRGB 转换并量化到 8-bit 后，预期像素为
`RGB(80, 221, 247)`。owner-approved 验收记录如下：

| 状态 | client extent | samples | mean RGB | expected match |
| ---- | ------------- | ------- | -------- | -------------- |
| 初始 | `854x480` | 11,360 | `(80, 221, 247)` | 100% |
| resize 1 | `1100x700` | 21,528 | `(80, 221, 247)` | 100% |
| resize 2 | `760x520` | 11,049 | `(80, 221, 247)` | 100% |
| minimize 5 s / restore | `760x520` | 11,049 | `(80, 221, 247)` | 100% |
| focus away / back | `760x520` | 11,049 | `(80, 221, 247)` | 100% |

验收过程按 Minecraft PID 关联真实 HWND，启用 per-monitor-v2 DPI awareness，以
`PrintWindow(PW_RENDERFULLCONTENT)` 获取窗口位图，再用 `GetClientRect` 与
`ClientToScreen` 精确裁出 client；依次执行两个 client resize、最小化 5 秒后恢复、聚焦到
真实 `cmd.exe` HWND 后再聚焦回 Minecraft，最后正常关闭窗口并检查 `latest.log`。日志中无
Mixin/Vulkan/presentation runtime 错误，且
`Presentation takeover closed before Minecraft Vulkan surface teardown` 恰好出现一次。

2026-07-31 Release/Vulkan/threaded-on 复验截图 SHA-256（截图本身为未跟踪验收产物，不提交）：

| 截图 | SHA-256 |
| ---- | ------- |
| `task6-quality-initial-854x480.png` | `7fa843a8b51a8e93eef868c82c9eb7d92891d9c3e6078e5541c1b94448746f39` |
| `task6-quality-resize-1100x700.png` | `9bd04ec5b4ca78c626282114e306b2d93494211012fb99d234db49ee8b48c6d4` |
| `task6-quality-resize-760x520.png` | `d053f5db3eade8188d181013d41cec40d79327ca86084b9f714a1a8a7ab03437` |
| `task6-quality-restore-after-minimize.png` | `d053f5db3eade8188d181013d41cec40d79327ca86084b9f714a1a8a7ab03437` |
| `task6-quality-restore-after-focus.png` | `d053f5db3eade8188d181013d41cec40d79327ca86084b9f714a1a8a7ab03437` |

CI 自动化只覆盖 coordinator terminal 状态、Mixin 的 configure 镜像、close 调用次数与
INFO/ERROR 互斥、测试和覆盖率门禁；它不宣称自动验证 OS 窗口生命周期或屏幕像素。

### D10. Readiness 与回退 `已裁决`

#### D10.1 两层结构，回退能力不同

**订正初稿**：初稿称「任一条件不满足则不接管，保持 Minecraft 原生路径」，该表述假定了
一种不存在的运行期回退。**阻止 Minecraft 创建 swapchain 发生在 `configure()`，是
generation 级的一次性决定**；一旦阻止，Minecraft 便无 swapchain 可用，「运行中回退原版
present」在物理上不可能。

两件事必须分层判定，其回退能力截然不同：

| 层 | 判定时机 | 回退能力 |
| -- | -------- | -------- |
| **presentation 接管** | generation 级（swapchain 创建/重建时） | 决定**之前**可放弃接管；接管**之后不可**运行期回退 |
| **世界替换**（取消 `LevelRenderer.render`） | 每帧 | **随时可回退**——不取消即恢复原版世界渲染 |

该分解使两者的风险等级不再绑定：世界替换廉价可逆，presentation 接管是一次性承诺。

#### D10.2 presentation 接管的 readiness（generation 级）

readiness 按 D9 验证阶段递进。当前 D9 第 1 步全部满足方可接管，任一不满足则**不阻止**
Minecraft 创建 swapchain，游戏以原版路径运行：

- 宿主图形后端为 Vulkan；
- 设备能力协商成功（D2 的扩展/特性追加）；
- 借用的 instance/device/surface/queue handles 完整且 queue/family 约束成立；
- Native presentation runtime 创建成功；
- 宿主 color texture/view 的静态属性通过：非空、color aspect、`COPY_SRC` 与
  `TEXTURE_BINDING` usage、正数二维单 layer 与有效 mip；D9 第 1 步不导入/读取该纹理，
  因而不把尚未 resize 的旧 host extent 与本次 configure extent 比较；
- graph module（若参与）材质化成功且健康。

D9 第 2 步起导入/合成宿主 GUI 纹理时，readiness 必须在 Minecraft 完成
`mainRenderTarget.resize` 或发出等价 generation 通知后，按 D5 与 D8.2 强制校验 imported texture
与目标 generation 的 exact extent。该后续门禁不得前移为 D9 第 1 步的无读取依赖。

#### D10.3 接管后的失败处理（无原版回退）

已接管则**只能在我方路径内处理**：

- `RecreateRequired` / `SurfaceUnavailable` → 换代重建，跳过本帧；
- 不可恢复失败 → 记入 failure context，本帧呈现黑帧或上一帧内容，并置位
  **延迟回退**标志：下一个 generation（如 resize）不再接管，由 Minecraft 自建 swapchain 恢复原版路径。

**不变式：已接管即每帧必须有输出。** 不允许出现「已接管但不 present」的状态——那将表现为
画面冻结或黑屏而无任何诊断线索。

#### D10.4 世界替换的 readiness（每帧级）

仅当 presentation 已接管、graph module 健康、imported generation 当前、相机与帧参数可用时
才取消 `LevelRenderer.render`；任一不满足则不取消，该帧由 Minecraft 渲染世界并经我方合成
呈现。此路径自由可逆，不置位延迟回退。

## Consequences

- ADR-0004 的 presentation runtime、frame slot、generation retirement、frame metrics
  全部复用；Minecraft 宿主与 standalone demo 共享同一 Native 实现。
- **不依赖 P22 ImportedImageBinding**：初稿曾将其列为前置依赖，该判断依 D5.2 已废止——
  presentation 路径涉及的三类资源均不经 BECS imported entry。P22 仍为将来的图级导入所需。
- `Mod/` 需承载对 `VulkanGpuSurface` / `Minecraft` / `LevelRenderer` 的 mixin；
  这些 Minecraft 内部类型不得出现在 `Core`/`Native` 的 ABI 中。
- 接管 present 与 `LevelRenderer.render` 会影响注入这些阶段的其他模组；renderer
  替换类模组之间应显式互斥，不为兼容性引入逐 draw 翻译或执行期解释层。
- Minecraft 版本升级时，四个接管点与 `mainRenderTarget` 生命周期需重新核验；
  适配层集中在 `Mod/`，Core/Native 通过中立 ABI 隔离。

## Open

- **D7 GUI alpha**：由 D9 第 3 步实测定案；在此之前 D8.4 的混合公式不完备。
- D8 合成规则是否、何时对 TA 开放——**无既定计划**，待真实需求出现时另行裁决。
- 与 ADR-0005 能力清单的协同：presentation 相关参数(输出分辨率、色彩空间)是否进入
  数据源清单。
