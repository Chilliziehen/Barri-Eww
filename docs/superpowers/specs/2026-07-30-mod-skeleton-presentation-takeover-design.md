# Mod Skeleton and D9 Step 1 Design

## Scope

This increment creates the independent `Mod/` Fabric module and implements only
ADR-0006 D9 step 1: Native owns the Minecraft surface swapchain, clears every
acquired image to one compile-time fixed color, and presents it. Minecraft world,
hand, HUD, and GUI rendering continue unchanged. The Minecraft blit is suppressed,
so none of that content reaches the window in this step.

The increment does not import or composite the host color texture, cancel world
rendering, materialize a graph module, create a presentation graphics pipeline, or
resolve Native symbols in `Mod/`.

## Governing Decisions

- T0 and ADR-0003 keep all frame work to fixed state transitions and O(1) Native
  calls. The successful frame path performs no graph interpretation, symbol lookup,
  Java heap allocation, or logging.
- `Mod/` is the only module that references Fabric, Minecraft, Mixin, or LWJGL host
  types. Core owns FFM binding and Native status translation. Native owns the
  swapchain and frame synchronization.
- ADR-0006 D1 and D10 make takeover a generation-level commitment. A generation is
  intercepted only after all readiness checks and Native runtime creation succeed.
  Once intercepted, the same generation never calls Minecraft swapchain methods.
- The D9 increment uses the ADR-0006 D6 independent-submit model. Native submits on
  the exact graphics queue borrowed from Minecraft and presents on that same queue.

## Module and Build Structure

`Mod/` is an independent Java 25 Gradle build using fabric-loom. It locks Minecraft
26.2 and the matching Fabric Loader and Fabric API versions. Its client run
configuration always includes `--enable-native-access=ALL-UNNAMED`.

The Mod build consumes two explicit artifacts:

1. The previously assembled Core jar, used as a file dependency and included in the
   remapped Mod jar.
2. The matching `BarriEwwNativeFfm` shared library, embedded under an
   operating-system and architecture-specific resource path.

The root build scripts preserve the fixed `Native -> Core -> Mod` order and pass the
absolute artifact paths plus the existing configuration/backend/threading variant
properties to Mod. Mod `assemble` fails before packaging if either artifact is
missing or does not match a supported platform. Mod `test` remains runnable with a
fake presentation runtime and does not load Vulkan.

At client initialization, Mod extracts the embedded shared library once to a
content-hash-named temporary location. The resulting absolute path is passed to
Core. Windows cleanup is registered for JVM exit because a loaded DLL cannot be
deleted while the process is alive.

## Core and Native Interface Extension

The host-neutral presentation binding currently in Core's `demo` source set moves
to Core `main`; the standalone demo keeps the same imports and observable behavior.
No Fabric or Minecraft type enters Core.

`VulkanPresentationRuntime` gains
`submitAndPresentClearFrame(const float clearColor[3])`. It requires an open frame,
records the existing fixed clear command into the command buffer selected by the
open frame's slot and image, then delegates to `submitAndPresentFrame`. Existing
`presentClearFrame` remains and becomes the composition of `beginFrame` followed by
the new method, preserving standalone behavior.

`NativePresentationRuntimeBoundary` adds the additive Version 1 C symbol
`barriEwwSubmitAndPresentClearFrameVersion1`. It accepts the opaque runtime address,
three clear-color scalars, and the existing fixed submit-result output record. The
entry point is non-critical, contains all C++ exceptions, validates the open-frame
precondition, and adds no Minecraft or Vulkan struct to the ABI.

Core resolves the new symbol once when `NativePresentationRuntime` is created. For
Mod's hot path, Core owns reusable fixed-layout begin/metrics/submit segments in the
runtime's confined Arena. `beginFrameStatus` and `submitAndPresentClearFrame` return
enum singletons and do not create a per-frame Arena, record, or `Optional`. Existing
metrics-bearing methods remain available for the standalone demo.

## Device Capability Negotiation and Handle Extraction

`VulkanBackendMixin` intercepts the argument set immediately before Minecraft calls
its private device creator. It queries and adds `bufferDeviceAddress` through
Minecraft's `VulkanFeature` mechanism when supported. Dynamic rendering is already
part of Minecraft 26.2's required feature set and is verified rather than duplicated.
The negotiation result is retained for the subsequently created Vulkan device; a
missing required feature leaves takeover unready without throwing from a non-Vulkan
path.

`GpuDeviceAccessor` exposes the private `GpuDevice.backend` field. Handle extraction
starts from `RenderSystem.getDevice()`, returns lazily when the backend is not a
`VulkanDevice`, and otherwise borrows:

- instance and physical-device addresses from the LWJGL parent chain;
- logical-device address;
- graphics queue address and family index;
- `VulkanGpuSurface`'s borrowed surface handle.

The D9 implementation requires graphics and present to be the same queue and passes
that queue in both Core bootstrap fields. A mismatch fails readiness before takeover.

## Backend Takeover Points

All presentation interception occurs in `VulkanGpuSurface`, not in the outer
`GpuSurface` wrapper. This is required by the verified Minecraft 26.2 state machine:

- after backend `acquireNextTexture` returns, `GpuSurface` sets
  `hasImageAcquired = true`;
- after backend `blitFromTexture` returns, it sets `hasBlittedTexture = true`;
- outer `present` requires both values and clears `hasImageAcquired` after the backend
  present returns.

Keeping those wrapper methods intact makes both `Minecraft.renderFrame` calls to
`isAcquired()` true and satisfies `GpuSurface.present()`'s blit precondition without
forging private wrapper state.

`VulkanGpuSurfaceMixin` delegates policy to one
`PresentationTakeoverCoordinator` and applies these backend hooks:

- `configure` HEAD: prepare a complete Native generation. Cancel Minecraft's method
  only after successful creation.
- `acquireNextTexture` HEAD: when taken over, call Core `beginFrameStatus` and cancel
  the Minecraft backend acquire.
- `blitFromTexture` HEAD: when taken over, cancel as an empty backend operation. The
  outer wrapper still completes its state transition.
- `present` HEAD: when taken over, call Core `submitAndPresentClearFrame` and cancel
  the Minecraft backend present.
- `close` HEAD: close the Native runtime before allowing Minecraft to destroy its own
  semaphores and surface.

The mixin also reflects Native `Suboptimal` and `RecreateRequired` outcomes through
the backend's existing suboptimal signal so Minecraft re-enters `configure`.

## Readiness and Generation Lifecycle

Before intercepting each `configure`, the coordinator verifies:

- the active backend is Vulkan;
- required device capability negotiation succeeded;
- borrowed instance/device/surface/queue handles are nonzero;
- graphics and present use the same queue and family;
- the host main color texture has the expected color format, matching extent, and
  sampled/copy-source usage required by the accepted final architecture;
- the embedded Native library is loaded and a Native runtime can be created for the
  requested nonzero extent.

On initial configuration there is no Minecraft swapchain. On resize, the coordinator
first closes the old Native runtime, then attempts to create the replacement. Success
cancels Minecraft `configure`; failure allows that same invocation to continue, so
Minecraft creates the replacement swapchain and owns the new generation.

`SurfaceUnavailable` and `RecreateRequired` mark the backend suboptimal and skip the
unavailable frame as allowed by ADR-0006 D10. An unrecoverable post-takeover failure
stores its complete context, requests reconfiguration, and sets delayed fallback. At
the next `configure`, the coordinator closes Native state and does not intercept, so
Minecraft resumes its original presentation path. Successful frames do not log.

The clear color is a compile-time constant chosen to be visually distinct from
Minecraft loading and menu backgrounds. It is not a runtime option.

## Error and Ownership Rules

Core continues to map Native operation results to checked exceptions on slow failure
paths. Mod presents those failures and does not duplicate numeric status mappings.
FFM documentation states thread confinement and ownership on every new boundary
method. Native owns runtime, swapchain, image views, command buffers, fences, and
semaphores; Minecraft owns and outlives the borrowed instance, physical device,
logical device, queue, and surface.

Teardown order is:

1. stop accepting intercepted frames;
2. close and drain the Native presentation runtime;
3. let `VulkanGpuSurface.close` destroy Minecraft semaphores and surface;
4. let Minecraft later destroy device and instance.

## Testing and Acceptance

Implementation follows test-driven development.

Native Catch2 coverage adds exact assertions for clear submission with and without an
open frame, fixed color recording, successful delegation, every new boundary error
result, and exception containment. Existing presentation and GPU tests remain green.

Core JUnit coverage verifies that moving the binding to `main` does not alter the
standalone API, that the new symbol is resolved, that the fixed output record is
decoded correctly, and that the status-only path reuses storage. The real FFM
integration task invokes the new entry point with rejected arguments and asserts the
exact operation result.

Mod JUnit coverage uses a fake runtime factory to verify the readiness truth table,
initial takeover, backend method ordering, resize success, resize failure returning to
Minecraft, delayed fallback, non-Vulkan lazy behavior, and teardown order. Mixin
classes remain thin so policy coverage resides in ordinary Java classes.

Regression commands include the Release Native test target and executable, Core
tests and real FFM integration, and Mod tests plus remapped assembly. The visible
Fabric client acceptance checks:

1. the window displays only the fixed clear color;
2. world and GUI rendering continue without reaching the swapchain;
3. repeated resize and minimize/restore continue presenting;
4. normal client exit produces no crash, Vulkan validation failure, or leaked-runtime
   diagnostic.

ADR-0006 D2 is updated in the implementation commit with the verified backend-layer
interception and preserved outer `GpuSurface` state-machine result.
