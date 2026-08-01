# Host Image Presentation Composition Design

## Scope

This increment implements ADR-0006 D9 step 2. Minecraft continues rendering the
world, hand, HUD, and GUI into its complete main color target. Native presentation
borrows that color image, samples it in one fullscreen graphics pass, writes the
Native-owned swapchain image, and presents it. The visible client area must match
Minecraft's original presentation for static menu and paused-world scenes.

This increment does not cancel `LevelRenderer.render`, determine GUI alpha semantics,
materialize graph output, perform scaling, perform color-space conversion, or expose
the composition pass to the task abstraction or BECS.

## Governing Decisions

- ADR-0006 D3 and D5 keep the borrowed Minecraft image entirely within
  presentation. It does not enter P22, an image table, or a graph module.
- ADR-0006 D6 requires Native to submit after Minecraft on the same graphics queue.
  Submission order plus an explicit memory barrier makes the complete host color
  target visible to the fragment shader without semaphore exchange.
- ADR-0006 D8 requires exact host, graph-output, and swapchain extents and prohibits
  MVP scaling and color-space conversion.
- T0 requires generation work at load/reconfigure time. The successful frame path
  performs one begin downcall, O(1) prerecorded command-buffer selection, one submit
  downcall, and no symbol lookup, descriptor update, command recording, allocation,
  or logging.
- The existing clear presentation ABI remains source- and binary-compatible. Host
  image composition is an additive Version 1 capability and does not repurpose any
  reserved field in a published record.

## Presentation ABI Extension

Presentation gains a backend-neutral `PresentationImageFormat` catalog with only the
formats needed by this increment:

| Value | Format |
| ----- | ------ |
| 1 | `R8G8B8A8Unorm` |
| 2 | `B8G8R8A8Unorm` |

The catalog belongs to presentation interoperability. Reusing the numeric values of
the command-stream catalog does not place the host image in BECS.

The additive `NativeHostImagePresentationRuntimeCreateInfoVersion1` record has this
fixed 96-byte layout:

| Offset | Type | Field |
| ------ | ---- | ----- |
| 0 | `uint64` | `instanceHandle` |
| 8 | `uint64` | `physicalDeviceHandle` |
| 16 | `uint64` | `logicalDeviceHandle` |
| 24 | `uint64` | `surfaceHandle` |
| 32 | `uint64` | `graphicsQueueHandle` |
| 40 | `uint64` | `presentQueueHandle` |
| 48 | `uint32` | `graphicsQueueFamilyIndex` |
| 52 | `uint32` | `presentQueueFamilyIndex` |
| 56 | `uint32` | `framebufferWidth` |
| 60 | `uint32` | `framebufferHeight` |
| 64 | `uint32` | `framesInFlightCount` |
| 68 | `uint32` | `reservedFlags`, required to be zero |
| 72 | `uint64` | `hostImageHandle` |
| 80 | `uint32` | `hostImageFormatValue` |
| 84 | `uint32` | `requestedSurfaceFormatValue` |
| 88 | `uint32` | `hostImageWidth` |
| 92 | `uint32` | `hostImageHeight` |

`barriEwwCreateHostImagePresentationRuntimeVersion1` accepts that record and the
existing `NativePresentationRuntimeCreateResultVersion1`. The create operation is
non-critical, borrows every Vulkan handle, and transfers ownership only of the opaque
runtime address on success. It rejects a null host image, unknown formats, a requested
surface format not advertised with `VK_COLOR_SPACE_SRGB_NONLINEAR_KHR`, and any host,
framebuffer, or selected swapchain extent mismatch. It never silently substitutes an
SRGB format for the requested UNORM format.

`barriEwwSubmitAndPresentHostImageFrameVersion1` accepts the opaque runtime address
and the existing submit result. It is non-critical because queue submission and
presentation can block. It requires an open frame and submits the prerecorded primary
for that frame's fixed slot/image pair. Existing create, clear-submit, and standalone
symbols retain their exact layouts and semantics.

Core resolves both new downcall handles once per binding instance and retains them as
immutable members of the lookup Arena owner. Java call segments remain confined and
bounded by the runtime. Destroy permanently invalidates the runtime address after one
attempt, including an unsuccessful attempt.

## Host Generation Preparation

Minecraft 26.2 calls `VulkanGpuSurface.configure` and acquire before
`GameRenderer.render` normally notices and resizes `mainRenderTarget`. D9 step 2 cannot
wait for that default resize because takeover readiness must know the exact borrowed
image before Minecraft swapchain creation is cancelled.

The coordinator therefore accepts a generation preparation callback and orders a
transition as follows:

1. stop frame access to the old Native generation;
2. drain and destroy the old Native generation and every view that references the old
   Minecraft image;
3. invoke the callback on the render thread;
4. if needed, call the complete public `GameRenderer.resize`, which clears its resource
   pool and resizes both the main target and level renderer;
5. extract and validate the new borrowed host image and original Minecraft surface
   format;
6. create the Native host-image candidate;
7. prime the candidate with the existing fixed clear presentation;
8. commit takeover and cancel Minecraft configure.

The callback is never invoked if old-generation retirement fails. A preparation or
candidate failure before commit leaves that configure invocation uncancelled, allowing
Minecraft to create and own its original swapchain.

A render-thread-only main-target tracker observes `RenderTarget.resize`. Its HEAD
notification retires a still-borrowing presentation generation before Minecraft
destroys the old image; its TAIL notification publishes a monotonically increasing
generation. The normal window path has already retired the generation before calling
the complete resize and therefore treats the HEAD notification idempotently.

If another host path replaces the main target outside configure, interception remains
committed even while no Native runtime is temporarily available. Backend acquire,
blit, and present remain suppressed, the last successful presented image remains on
screen, and the backend requests reconfiguration. This state must never call a
Minecraft swapchain method because no Minecraft swapchain exists for that generation.
The next configure follows the normal preparation path. Configure validates both the
tracker generation and the current `VkImage` handle so an unobserved replacement still
cannot reuse a stale binding.

The tracker listener is registered by the one Minecraft Vulkan surface and removed at
surface close. It must not retain a closed coordinator or cross the Mod class-loader
lifetime.

## Host Binding Validation

The Mod binding extractor accepts only the current live main color texture and view
when all of these conditions hold:

- the texture is a `VulkanGpuTexture` and its handle is nonzero;
- the format is exactly Minecraft's `RGBA8_UNORM` main-target format;
- copy-source and texture-binding usage bits are present;
- texture and full view are open, color-aspect, two-dimensional, single-layer,
  single-mip objects;
- texture, view, requested framebuffer, and target generation extents are exact and
  positive;
- Minecraft's selected surface format maps to RGBA or BGRA UNORM.

The Mod passes only scalar handles and neutral presentation formats into Core. No
Minecraft, Fabric, LWJGL object, or raw Vulkan format number crosses into Core.

## Native Composition Resources

Each host-image presentation generation owns:

- one Native-created sampled image view over the borrowed host image;
- one nearest/clamp sampler;
- one descriptor set layout, descriptor pool, and immutable descriptor set;
- one pipeline layout and host-only fullscreen graphics pipeline;
- one primary command buffer for every frame-slot and swapchain-image pair;
- the existing Native-owned swapchain views and synchronization objects.

Minecraft continues to own the host image and its allocation. Native destroys its
dependent descriptor and image-view resources before Minecraft may close that image.
Shader modules are temporary pipeline-creation objects and are destroyed immediately
after successful or failed pipeline creation.

The fixed vertex and fragment GLSL sources use PascalCase filenames under
`Native/src/Vulkan/Shaders`. CMake locates `Vulkan::glslc`, compiles them for Vulkan
1.3 during the build, and converts the resulting SPIR-V into a build-tree `constexpr`
byte header linked into the Native library. The released library performs no runtime
shader compilation or file lookup. Linux Vulkan CI installs `glslc`; Vulkan-disabled
Windows matrix cells do not require it.

## Prerecorded Command Flow

Every matrix entry records the following fixed sequence:

1. issue a host image memory barrier with old and new layout both `GENERAL`, source
   stages `COLOR_ATTACHMENT_OUTPUT | TRANSFER`, source access
   `COLOR_ATTACHMENT_WRITE | TRANSFER_WRITE`, destination stage `FRAGMENT_SHADER`, and
   destination access `SHADER_READ`;
2. transition the acquired swapchain image from `UNDEFINED` to
   `COLOR_ATTACHMENT_OPTIMAL` because the fullscreen draw completely overwrites it;
3. begin dynamic rendering with discard load behavior, bind the fixed pipeline and
   host descriptor, and draw one fullscreen triangle;
4. end rendering and transition the swapchain image to `PRESENT_SRC_KHR`.

`UNDEFINED` is valid only for the fully overwritten swapchain image. It is forbidden
for the borrowed host image because it would discard Minecraft's rendered content.
Exact equal extents plus nearest sampling provide a one-to-one copy without filtering,
scaling, tone mapping, or color-space conversion.

After Minecraft submits all world and GUI work, the backend present hook calls the
host-image submit operation. Native selects the matrix entry from the already-open
frame's slot and acquired image index, submits it on the borrowed Minecraft graphics
queue, and presents. Queue submission order and the first barrier establish the D6
dependency without semaphores shared with Minecraft.

## Error Handling

Host validation and candidate creation occur before takeover commit. Unsupported host
or surface properties return readiness failure and preserve vanilla presentation.
Every Vulkan creation failure returns the exact raw result and destroys partial Native
resources in reverse ownership order.

After takeover commit, acquire, submit, or present failure follows ADR-0006 D10. The
coordinator stores complete context, suppresses every Minecraft swapchain operation,
retains the last successful output where possible, and requests a generation
transition. It never attempts an in-place fallback to a vanilla swapchain. Successful
frames perform no logging.

## Testing

Implementation follows test-driven development.

Native Catch2 tests cover neutral format mapping, requested format selection and
rejection, null image and extent validation, every resource-creation failure cleanup
edge, descriptor and pipeline configuration, exact barrier fields, command-matrix
indexing, no steady-state command recording, boundary result mapping, and exception
containment.

Core JUnit tests pin every new record offset, size, and alignment; eager symbol
resolution; non-critical handle construction; exact argument transfer; result mapping;
bounded Arena ownership; and consumed-address destroy semantics. Real FFM integration
tests call the new symbols with rejected inputs and assert stable operation results.

Mod JUnit tests cover host extraction, surface-format mapping, the
`retire -> prepare -> create -> prime -> commit` order, callback suppression after
retirement failure, resize listener registration and removal, handle comparison,
temporary no-runtime interception, and one-attempt teardown. Mixin and remapped-jar
tests pin target descriptors and packaged classes.

The release gate runs the complete Native operating-system and threaded-recording
matrix, Native coverage at 90 percent or higher, Core unit and real FFM integration
coverage at 90 percent or higher, and Mod check, 70-percent coverage, and `remapJar`.

## Visible Acceptance

Visible comparison uses a static menu page and a paused real world. Original and
takeover captures use the same client extent and exclude window borders and the mouse
cursor. The client image must have no vertical flip, channel swap, gamma shift, or
pixel difference. If the operating-system compositor prevents exact capture equality,
the evidence records that limitation and compares fixed interior samples and full
histograms against the Minecraft main-target screenshot instead of weakening the
rendering requirement.

Regression acceptance repeats initial display, two resizes, minimize/restore, and
focus away/back. It checks the first frame after each generation, validation output,
and single-attempt teardown. ADR-0006 D9.2 records the complete command, packaged
Native library hash, screenshot and log hashes, and empirical results. Build outputs,
screenshots, and runtime logs remain untracked evidence.
