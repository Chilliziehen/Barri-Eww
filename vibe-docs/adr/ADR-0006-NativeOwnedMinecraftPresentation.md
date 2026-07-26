# ADR-0006: Native-owned Minecraft presentation

- **状态**: Proposed (2026-07-26)
  - **D1 方向已由所有者裁决**：presentation 由 Native 完全接管。
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

### D3. RDG 与 presentation 的职责边界 `待裁决`

| 归属 | 内容 |
| ---- | ---- |
| **RDG(编译期)** | 全部资源(含 imported image)、barrier 计算、命令录制 |
| **Presentation runtime(执行期微内核)** | acquire/present、fence/semaphore、frame slot、generation、recreation |

**presentation 不脱离 RDG 的资源与命令体系，但帧循环独立于 RDG。**

决定性理由：barrier 由编译器在编译期计算。若 Minecraft 的 GUI 纹理与 swapchain image
不在 RDG 资源体系内，合成步骤的 layout transition 与 access mask 便无法在编译期确定，
只能在执行期补算，直接违背 T0 与 ADR-0003。

次要理由：P22 ImportedImageBinding 契约已存在，swapchain image 与 Minecraft GUI 纹理
属同一类 borrowed 资源；HDR/色彩空间/frame generation 均需在合成处表达，故合成必须
是图可描述的对象。

### D4. 预录矩阵与合成尾段 `待裁决`

沿用 ADR-0004 D4 的两级结构：

```text
secondary[frameSlot][lane]      图工作，与 imageIndex 无关
primary[frameSlot][imageIndex]  presentation，含合成尾段
```

presentation primary 的固定构成：

```text
timestamp begin
execute secondary[frameSlot][*]                 // 世界
barrier worldColor    → shader read             // 编译期计算
barrier mcGuiTexture  → shader read             // 编译期计算
<composite>           → swapchainImage[imageIndex]
barrier swapchainImage → PRESENT_SRC
timestamp end
```

合成是图的**尾段**，由 RDG 编译器识别并单独编译进 presentation primary；只有尾段按
`imageIndex` 展开。若将世界渲染一并编入 primary，其命令需按 swapchain image 数量
(2–4)复制，命令缓冲内存按倍数增长。

Minecraft 的 GUI 纹理只被尾段消费，世界渲染期间不参与，barrier 编译器据此只需为它
生成一次 layout transition。

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

### D6. 提交边界 `待裁决`

Minecraft 的 GUI 由其自身经 `VulkanCommandEncoder.submit()` 提交，我方合成需读取该纹理，
跨提交必须同步。两条候选路径：

**方案 α — 注入 Minecraft 的 encoder(建议首版)**

```text
encoder.waitSemaphore(ourAcquireSemaphore, ...)
encoder.execute(ourPrerecordedPrimary[frameSlot][imageIndex])
encoder.signalSemaphore(ourPresentSemaphore, ...)
→ 我方 vkQueuePresentKHR
```

Minecraft 自身的 blit 即此结构，我方仅替换其命令缓冲内容。同队列提交顺序保证 GUI 先于
合成执行，仅需一次 memory barrier 保证可见性。swapchain / semaphore / present 仍归我方，
满足 D1；提交动作借用宿主机器，避免与 `VulkanCommandEncoder` 内部状态冲突。

**方案 β — 自行 `vkQueueSubmit`**

完全独立提交。控制力更强(frame generation、latency marker 需要)，但须自行处理与
Minecraft 提交的排序及 encoder 状态一致性，风险更高。

α → β 的切换不改变图的编译产物，仅影响提交层。

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

- D6 方案 α/β 的首版选择(建议 α)。
- D7 的 GUI alpha 方案，须实测后定案。
- D8 合成规则是否、何时对 TA 开放。
- `VulkanGpuSurface` 的具体屏蔽手法与其内部状态一致性保证。
- 与 ADR-0005 能力清单的协同：presentation 相关参数(输出分辨率、色彩空间)是否进入
  数据源清单。
