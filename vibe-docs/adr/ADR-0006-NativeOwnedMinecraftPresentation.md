# ADR-0006: Native-owned Minecraft presentation

- **状态**: Proposed (2026-07-26)
  - **D1 已裁决**：presentation 由 Native 完全接管。
  - **D3 已裁决**：单一交接点(图产出 output image)，契约固化于 §9.17。
  - **D6 已裁决**：独立提交，不注入 Minecraft 的 encoder。
  - 其余条款为待逐项讨论的草案；标注 `待裁决` 或 `待实测` 的条目在批准前不得实现。
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

## Decision

### D1. Presentation 所有权：Native 完全接管 `已裁决`

- Minecraft 不得在 surface 上创建或驱动 swapchain。
- **Native 拥有**：swapchain 与 image views、acquire/present、per-frame 同步对象、
  frame slot、presentation generation 与 recreation。
- **Minecraft 保留**：`VkInstance`、physical device、`VkDevice`、queues、`VkSurfaceKHR`
  ——与 ADR-0004 中 Java bootstrap 的角色同构。
- 不引入第二种 presentation 模式；ADR-0004 的所有权矩阵在 Minecraft 宿主下按上述映射
  继续有效。

### D2. 四个接管点 `待裁决(实现细节)`

| 接管点 | 位置 | 动作 |
| ------ | ---- | ---- |
| ① swapchain 创建 | `VulkanGpuSurface.configure` | 阻止 Minecraft 建立 swapchain |
| ② acquire | `Minecraft.java:1245` → `acquireNextTexture()` | 由 Native 的 `beginFrame` 取代 |
| ③ 世界渲染 | `LevelRenderer.render` HEAD | readiness 满足时 cancel |
| ④ blit + present | `Minecraft.java:1294` / `VulkanGpuSurface.present()` | 由 Native 合成尾段 + present 取代 |

`VulkanGpuSurface` 持有内部状态(`currentImageIndex`、acquire/present semaphores、
`swapchainOutOfDate`)，接管后必须保证 Minecraft 侧代码路径不进入非法状态或抛出。
具体屏蔽方式(mixin 取消 / 重定向 / 空实现)在实现提案中确定。

**必须处理项 —— `isAcquired()` 门控**：`Minecraft.java:1309` 以
`if (this.windowSurface.isAcquired())` 决定是否调用 `present()`。接管 acquire 后
Minecraft 内部的 `currentImageIndex` 不再被设置，`isAcquired()` 可能返回 false，导致
`present()` 永不被调用而直接黑屏。该门控必须一并接管或使其返回真值。

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

### D4. 预录矩阵 `待裁决（须依 D3/D6 重写）`

> 本条初稿写于 D3 裁决之前，其中「合成尾段由 RDG 编译器识别并编译」的表述已与 D3
> 冲突：合成归 presentation 侧，swapchain image 与宿主 GUI 纹理不进 BECS。以下为
> 修正后的框架，具体条款待讨论。

沿用 ADR-0004 D4 的两级结构：

```text
secondary[frameSlot][lane]      图编译产物，与 imageIndex 无关
primary[frameSlot][imageIndex]  presentation 侧手写预录
```

presentation primary 的固定构成：

```text
timestamp begin
barrier hostGuiTexture → 合成读取布局        // 兼作与宿主提交的同步点（D6）
execute secondary[frameSlot][*]              // 图工作
barrier graphOutput    → 合成读取布局
<composite>            → swapchainImage[imageIndex]
barrier swapchainImage → PRESENT_SRC
timestamp end
```

只有 primary 按 `imageIndex` 展开；图的 secondary 与 swapchain 无关，因此世界渲染的命令
不随 swapchain image 数量(2–4)复制。

**待讨论**：presentation 侧预录 primary 时需要向已材质化的模块查询哪些信息——至少是
按 frameSlot 的 secondary 命令缓冲句柄与 graph output image(§9.17)。该查询面位于 Native
内部两个子系统之间，不跨 FFM，但同样应为编译期确定的固定形状。

### D5. 导入契约 `待裁决`

- Minecraft `mainRenderTarget` 的 color texture 以 P22 ImportedImageBinding 导入，
  按 importIdentifier 绑定，携带 format/extent/mips/layers/samples/usage/layout/
  queue family 的中立描述并在装载期校验。
- 句柄在一个 generation 内不变(依据 Context 事实 2)，因此可被预录命令缓冲直接引用。
- `resize` / render-scale 变化 → presentation generation 换代 → 重建导入绑定与依赖的
  命令缓冲；沿用 ADR-0004 D3 的 old-swapchain 与 generation 退休策略，不依赖全局
  `vkDeviceWaitIdle`。
- 若未来实测发现该纹理来自 `GraphicsResourceAllocator` 池化分配而非稳定句柄，回退方案为
  按 `(frameSlot, textureIndex)` 展开预录矩阵，与 swapchain image 同法处理。

**必须查证项 —— Minecraft 渲染完成后该纹理的 image layout**(D6 依赖)：我方 primary 起始
barrier 需以该布局为 `oldLayout`。**不得以 `VK_IMAGE_LAYOUT_UNDEFINED` 代替**——那会丢弃
图像内容，GUI 随之消失。若实测表明该布局不唯一或随路径变化，则需在接管点显式转换或
扩展导入契约携带该布局。实现前必须查清，不得以推断定案。

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

### D8. 合成规则的开放程度 `待裁决`

MVP 阶段合成规则硬编码(世界在下、GUI 在上、alpha blend)，不对 TA 开放。待 HDR/色彩
空间或 frame generation 实际落地时，再评估是否将合成尾段开放为图可编辑的 pass
(与 ADR-0005 的 TA 创作面协同设计)。

### D9. 三步验证路径 `待裁决`

每一步独立可回退，失败不影响游戏可玩性：

1. **接管 present，输出纯色**。阻止 Minecraft 建 swapchain，我方创建、acquire、清屏、
   present；不合成 GUI、不取消世界。验证接管本身、resize 与 teardown。
2. **合成 Minecraft GUI**。导入 `mainRenderTarget`，尾段将其输出至 swapchain；世界仍由
   Minecraft 渲染(不取消 `LevelRenderer.render`)。验证导入契约、跨提交同步与 D7。
   **判据：画面与原版一致。**
3. **取消世界，接入 Barri-Eww 图**。完整链路。

第 2 步的"与原版一致"判据把"接管 present + 导入 + 同步"与"我方图渲染正确性"完全解耦，
是本 ADR 首选的正确性判据。

### D10. Readiness 与回退 `待裁决`

仅当 backend 为 Vulkan、能力协商通过、Native runtime 健康、导入 generation 当前、
frame slot 获取成功时才接管；任一条件不满足则不接管，保持 Minecraft 原生路径。
不允许出现"已接管但无输出"的状态。失败以值上报并记入 failure context。

## Consequences

- ADR-0004 的 presentation runtime、frame slot、generation retirement、frame metrics
  全部复用；Minecraft 宿主与 standalone demo 共享同一 Native 实现。
- 需要 P22 ImportedImageBinding 先行落地(ADR-0004 已登记，尚未实现)。
- `Mod/` 需承载对 `VulkanGpuSurface` / `Minecraft` / `LevelRenderer` 的 mixin；
  这些 Minecraft 内部类型不得出现在 `Core`/`Native` 的 ABI 中。
- 接管 present 与 `LevelRenderer.render` 会影响注入这些阶段的其他模组；renderer
  替换类模组之间应显式互斥，不为兼容性引入逐 draw 翻译或执行期解释层。
- Minecraft 版本升级时，四个接管点与 `mainRenderTarget` 生命周期需重新核验；
  适配层集中在 `Mod/`，Core/Native 通过中立 ABI 隔离。

## Open

- D4 依 D3/D6 重写后的具体条款，含 presentation 侧对已材质化模块的查询面形状。
- D7 的 GUI alpha 方案，须实测后定案。
- D8 合成规则是否、何时对 TA 开放。
- `VulkanGpuSurface` 的具体屏蔽手法与其内部状态一致性保证。
- 与 ADR-0005 能力清单的协同：presentation 相关参数(输出分辨率、色彩空间)是否进入
  数据源清单。
