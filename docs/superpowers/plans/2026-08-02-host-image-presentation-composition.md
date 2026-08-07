# Host Image Presentation Composition Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement ADR-0006 D9 step 2 so Native presents Minecraft's complete main color target through a host-only fullscreen graphics pass with pixel-identical output.

**Architecture:** Extend the existing opaque presentation runtime with detachable host-image resources while preserving the clear path and every published Version 1 ABI. Core owns the additive FFM binding, Mod prepares and tracks Minecraft target generations, and Native prerecords a fixed frame-slot by swapchain-image command matrix. Generation work occurs before takeover or at resize notification; the steady frame path is allocation-free O(1) begin/select/submit.

**Tech Stack:** C++23, Vulkan 1.2 plus `VK_KHR_dynamic_rendering`, CMake 4.4, GLSL/glslc, Catch2, Java 25 FFM, JUnit 5, Fabric Mixin, Gradle, JaCoCo.

---

> **Progress provenance (2026-08-07):** Task 1–10 boxes are ticked from a verified end state, not a replay of each step. Evidence: every planned deliverable file exists, and the complete Task 10 Step 4 gate passed locally — Native ON/OFF variants 129/129 CTest each, Core `test demoTest nativeIntegrationTest` plus coverage verification, and Mod `check remapJar verifyRemappedJarContents` with 140/140 tests. The packaging guard was additionally proven to fail on an injected missing entry. Task 11 requires a real Minecraft client session and remains unticked.

## Execution Prerequisite

Merge `docs/spec-host-image-presentation-abi` into `dev` through MR and full CI first. Then update `dev`, create `feature/host-image-presentation-composition` from that exact merge, and use an isolated worktree as required by the execution skill. Do not implement against the unmerged docs branch.

The implementation is governed by:

- `docs/superpowers/specs/2026-08-02-host-image-presentation-composition-design.md`
- `vibe-docs/spec/FfmBinding.md` §6.7.4
- `vibe-docs/spec/Testing.md` §4.4-§4.5
- `vibe-docs/adr/ADR-0006-NativeOwnedMinecraftPresentation.md` D5-D10

Every commit must use the repository §5.3 body fields: `改动点`, `影响范围`, `相关文件`, `改动思路`, `功能性变化`, and `非功能性变化`.

## File Structure

### Native

- Create `Native/include/BarriEww/Interoperability/PresentationImageFormat.hpp`: presentation-only stable neutral format values.
- Create `Native/include/BarriEww/Vulkan/VulkanPresentationImageFormatMapping.hpp`: neutral format to `VkFormat` conversion.
- Create `Native/include/BarriEww/Vulkan/VulkanHostImagePresentationResources.hpp`: focused RAII owner for all resources that depend on the borrowed host image.
- Create `Native/src/Vulkan/VulkanHostImagePresentationResources.cpp`: transactional creation, prerecorded command matrix, detach waits, and destruction.
- Create `Native/src/Vulkan/Shaders/HostImagePresentationVertexShader.vert`: fullscreen triangle and Minecraft-compatible Y inversion.
- Create `Native/src/Vulkan/Shaders/HostImagePresentationFragmentShader.frag`: direct nearest-sampled host output.
- Create `Native/cmake/GenerateEmbeddedShaderHeader.cmake`: deterministic build-tree SPIR-V byte header generation.
- Modify `Native/include/BarriEww/Interoperability/NativePresentationRuntimeBoundary.hpp`: P28 record, assertions, and three additive symbols.
- Modify `Native/src/Interoperability/NativePresentationRuntimeBoundary.cpp`: P28 validation and exception containment.
- Modify `Native/include/BarriEww/Vulkan/VulkanPresentationRuntime.hpp`: host factory, host submit, detach, and optional host resource owner.
- Modify `Native/src/Vulkan/VulkanPresentationRuntime.cpp`: exact requested format/usage creation and host operation delegation.
- Modify `Native/CMakeLists.txt` and `Native/tests/CMakeLists.txt`: shader generation and test targets.
- Modify `Native/tests/NativePresentationRuntimeBoundaryTests.cpp`: mock Vulkan ABI/resource/failure coverage.
- Create `Native/tests/HostImagePresentationExecutionTests.cpp`: lavapipe corner-pattern execution/readback.

### Core

- Create `Core/src/main/java/barrieww/core/interoperability/PresentationImageFormat.java`: stable presentation format catalog.
- Create `Core/src/main/java/barrieww/core/interoperability/HostImagePresentationBinding.java`: immutable scalar borrowed-image metadata.
- Modify `Core/src/main/java/barrieww/core/interoperability/NativePresentationRuntime.java`: host-only factory and reusable detach/submit downcalls.
- Modify `Core/src/test/java/barrieww/core/interoperability/PresentationRuntimeUnitTests.java`: layout and format tests.
- Modify `Core/src/test/java/barrieww/core/interoperability/PresentationRuntimeBindingTests.java`: exact write/resolution/lifetime tests.
- Modify `Core/src/demoTest/java/barrieww/core/interoperability/NativePresentationRuntimeIntegrationTests.java`: real rejected-input calls through the built shared library.

### Mod

- Create `Mod/src/main/java/barrieww/mod/MinecraftHostImagePresentationBindingExtractor.java`: Minecraft/LWJGL validation and neutral mapping.
- Create `Mod/src/main/java/barrieww/mod/PresentationGenerationPreparation.java`: checked generation slow-path callback.
- Create `Mod/src/main/java/barrieww/mod/MainRenderTargetGenerationTracker.java`: render-thread generation and listener lifecycle.
- Create `Mod/src/main/java/barrieww/mod/MainRenderTargetResizeListener.java`: complete-resize precondition and publication contract.
- Create `Mod/src/main/java/barrieww/mod/mixin/GameRendererMixin.java`: cancellable complete-resize HEAD notification.
- Create `Mod/src/main/java/barrieww/mod/mixin/RenderTargetMixin.java`: main-target resize TAIL generation publication.
- Modify runtime/factory/input/coordinator classes under `Mod/src/main/java/barrieww/mod`.
- Modify `Mod/src/main/java/barrieww/mod/mixin/VulkanGpuSurfaceMixin.java` and `Mod/src/main/resources/barrieww.mixins.json`.
- Replace `MinecraftClearTakeoverReadinessTests.java` with focused host-binding tests; extend coordinator and mixin structure tests.

## Task 1: Pin the Native P28 ABI and Neutral Formats

**Files:**
- Create: `Native/include/BarriEww/Interoperability/PresentationImageFormat.hpp`
- Create: `Native/include/BarriEww/Vulkan/VulkanPresentationImageFormatMapping.hpp`
- Modify: `Native/include/BarriEww/Interoperability/NativePresentationRuntimeBoundary.hpp`
- Modify: `Native/tests/NativePresentationRuntimeBoundaryTests.cpp`

- [x] **Step 1: Write failing compile-time and Catch2 ABI tests**

Add assertions for values `1` and `2`, unknown mapping rejection, create-info size `96`/alignment `8`, detach-result size `8`/alignment `4`, every P28 offset, standard layout, trivial copyability, and the exact three symbol declarations. The central test shape is:

```cpp
STATIC_REQUIRE(sizeof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1) == 96u);
STATIC_REQUIRE(alignof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1) == 8u);
STATIC_REQUIRE(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                        hostImageHandle) == 72u);
STATIC_REQUIRE(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                        hostImageHeight) == 92u);
STATIC_REQUIRE(sizeof(barrieww::NativePresentationDetachHostImageResourcesResultVersion1)
               == 8u);
STATIC_REQUIRE(offsetof(barrieww::NativePresentationDetachHostImageResourcesResultVersion1,
                        vulkanResult) == 0u);
REQUIRE(barrieww::toVulkanPresentationImageFormat(
            barrieww::PresentationImageFormat::R8G8B8A8Unorm)
        == VK_FORMAT_R8G8B8A8_UNORM);
```

- [x] **Step 2: Run the focused test target and verify RED**

Run:

```powershell
cmake -S Native -B Native/build-host-image-plan -DCMAKE_BUILD_TYPE=Debug -DBARRIEWW_BACKEND_VULKAN=ON -DTHREADED_RECORDING=ON -DBARRIEWW_BUILD_TESTS=ON
cmake --build Native/build-host-image-plan --config Debug --target BarriEwwNativePresentationRuntimeTests
```

Expected: compile failure because the P28 catalog and record do not exist.

- [x] **Step 3: Add the minimal catalog, mapping, record, assertions, and declarations**

Use these public shapes exactly:

```cpp
enum class PresentationImageFormat : std::uint32_t {
    R8G8B8A8Unorm = 1u,
    B8G8R8A8Unorm = 2u,
};

struct NativeHostImagePresentationRuntimeCreateInfoVersion1 {
    std::uint64_t instanceHandle;
    std::uint64_t physicalDeviceHandle;
    std::uint64_t logicalDeviceHandle;
    std::uint64_t surfaceHandle;
    std::uint64_t graphicsQueueHandle;
    std::uint64_t presentQueueHandle;
    std::uint32_t graphicsQueueFamilyIndex;
    std::uint32_t presentQueueFamilyIndex;
    std::uint32_t framebufferWidth;
    std::uint32_t framebufferHeight;
    std::uint32_t framesInFlightCount;
    std::uint32_t reservedFlags;
    std::uint64_t hostImageHandle;
    std::uint32_t hostImageFormatValue;
    std::uint32_t requestedSurfaceFormatValue;
    std::uint32_t hostImageWidth;
    std::uint32_t hostImageHeight;
};
```

Declare the exact P28 create, detach, and host-submit symbols from §6.7.4. Keep every old record and symbol byte-for-byte unchanged.
The detach symbol receives the opaque address and a writable 8-byte detach result.

- [x] **Step 4: Rebuild and verify GREEN**

Run the build command from Step 2 and:

```powershell
ctest --test-dir Native/build-host-image-plan -C Debug --output-on-failure -R NativePresentation
```

Expected: build succeeds and existing presentation tests pass.

- [x] **Step 5: Commit the ABI catalog**

Commit subject: `feature/host-image-presentation-abi` with the mandatory §5.3 body naming only the four files in this task.

## Task 2: Compile and Embed the Fixed Vulkan 1.2 Shaders

**Files:**
- Create: `Native/src/Vulkan/Shaders/HostImagePresentationVertexShader.vert`
- Create: `Native/src/Vulkan/Shaders/HostImagePresentationFragmentShader.frag`
- Create: `Native/cmake/GenerateEmbeddedShaderHeader.cmake`
- Modify: `Native/CMakeLists.txt`

- [x] **Step 1: Add a build assertion that requires both generated headers**

Declare generated outputs under `${CMAKE_CURRENT_BINARY_DIR}/generated/BarriEww/Vulkan/Shaders` and add them as private sources of `BarriEwwNative`. Configure before adding generation commands.

- [x] **Step 2: Verify RED at configure/build**

Run the Task 1 configure and build commands.

Expected: configure or build fails because `Vulkan::glslc`, shader sources, and generated headers are not wired.

- [x] **Step 3: Add complete shaders and deterministic generation**

The vertex shader must emit one fullscreen triangle and apply the same Y inversion as Minecraft's original destination blit:

```glsl
#version 450
layout(location = 0) out vec2 textureCoordinate;
void main() {
    const vec2 positions[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    const vec2 coordinates[3] = vec2[3](vec2(0.0, 0.0), vec2(2.0, 0.0), vec2(0.0, 2.0));
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    textureCoordinate = coordinates[gl_VertexIndex];
}
```

The fragment shader is a direct sample:

```glsl
#version 450
layout(set = 0, binding = 0) uniform sampler2D hostColorTexture;
layout(location = 0) in vec2 textureCoordinate;
layout(location = 0) out vec4 outputColor;
void main() {
    outputColor = texture(hostColorTexture, textureCoordinate);
}
```

Use `Vulkan::glslc --target-env=vulkan1.2 -O`. The CMake script must read the generated SPIR-V as hexadecimal and emit an `alignas(std::uint32_t) inline constexpr std::array<std::byte, N>` header without `xxd`, Python, or runtime file lookup. Require `Vulkan::glslc` only inside `if(BARRIEWW_BACKEND_VULKAN)`.

- [x] **Step 4: Verify generated SPIR-V and backend-off isolation**

Run:

```powershell
cmake --build Native/build-host-image-plan --config Debug --target BarriEwwNative
cmake -S Native -B Native/build-host-image-plan-backend-off -DBARRIEWW_BACKEND_VULKAN=OFF -DBARRIEWW_BUILD_TESTS=ON
cmake --build Native/build-host-image-plan-backend-off --config Debug
```

Expected: both builds pass; generated Vulkan headers exist only in the Vulkan build tree.

- [x] **Step 5: Commit shader build assets**

Commit subject: `feature/host-image-presentation-shaders` with the mandatory §5.3 body.

## Task 3: Build Transactional Host-Image Resources and the Prerecorded Matrix

**Files:**
- Create: `Native/include/BarriEww/Vulkan/VulkanHostImagePresentationResources.hpp`
- Create: `Native/src/Vulkan/VulkanHostImagePresentationResources.cpp`
- Modify: `Native/tests/NativePresentationRuntimeBoundaryTests.cpp`
- Modify: `Native/tests/CMakeLists.txt`

- [x] **Step 1: Write failing mock-Vulkan tests**

Cover exact creation order, every injected failure, reverse cleanup, temporary shader module destruction, nearest/clamp sampler, descriptor image layout `GENERAL`, requested output format, `TRANSFER_DST | COLOR_ATTACHMENT` swapchain requirement, and matrix count. Pin the command sequence and barrier fields:

```cpp
REQUIRE(g_hostBarrier.oldLayout == VK_IMAGE_LAYOUT_GENERAL);
REQUIRE(g_hostBarrier.newLayout == VK_IMAGE_LAYOUT_GENERAL);
REQUIRE(g_hostBarrier.srcAccessMask == VK_ACCESS_MEMORY_WRITE_BIT);
REQUIRE(g_hostBarrier.dstAccessMask == VK_ACCESS_SHADER_READ_BIT);
REQUIRE(g_hostSourceStageMask
        == (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT));
REQUIRE(g_hostDestinationStageMask == VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
REQUIRE(g_recordedDrawVertexCount == 3u);
```

Also assert `matrixIndex == frameSlotIndex * swapchainImageCount + imageIndex`.

- [x] **Step 2: Run focused tests and verify RED**

Run:

```powershell
cmake --build Native/build-host-image-plan --config Debug --target BarriEwwNativePresentationRuntimeTests
ctest --test-dir Native/build-host-image-plan -C Debug --output-on-failure -R NativePresentation
```

Expected: compile failure because `VulkanHostImagePresentationResources` does not exist.

- [x] **Step 3: Implement the focused RAII owner**

Expose only this operational surface:

```cpp
class VulkanHostImagePresentationResources {
public:
    struct CreateInfo {
        VkDevice logicalDevice;
        VkImage hostImage;
        VkFormat hostImageFormat;
        VkFormat swapchainImageFormat;
        VkExtent2D extent;
        std::span<const VkImage> swapchainImages;
        std::uint32_t frameSlotCount;
    };

    static std::expected<VulkanHostImagePresentationResources, CreationFailure>
    create(const CreateInfo& createInfo);
    VkCommandBuffer commandBuffer(std::uint32_t frameSlotIndex,
                                  std::uint32_t imageIndex) const noexcept;
    std::expected<void, VkResult> destroyAfterSubmittedFrames(
        std::span<const VkFence> submittedFrameFences,
        VkFence unsubmittedOpenFrameFence);
};
```

Create and own the image view, sampler, descriptor layout/pool/set, pipeline layout,
pipeline, dedicated command pool, and matrix. Use `vkCmdBeginRenderingKHR` and
`vkCmdEndRenderingKHR`. Do not place host command buffers in the clear command pool.
Rollback every partial create in strict reverse order and preserve the first raw
`VkResult`.

- [x] **Step 4: Implement detach fence selection and idempotency**

When an open frame exists, omit its reset/unsubmitted fence and wait every other
submitted slot fence before destroying host resources. A second detach is success and
does no Vulkan call. Do not destroy the borrowed `VkImage`.

- [x] **Step 5: Run failure and success tests**

Run the focused build/CTest commands from Step 2.

Expected: all creation, recording, matrix, detach, and cleanup cases pass.

- [x] **Step 6: Commit host resource ownership**

Commit subject: `feature/host-image-presentation-resources` with the mandatory §5.3 body.

## Task 4: Integrate Host Resources into the Runtime and Native Boundary

**Files:**
- Modify: `Native/include/BarriEww/Vulkan/VulkanPresentationRuntime.hpp`
- Modify: `Native/src/Vulkan/VulkanPresentationRuntime.cpp`
- Modify: `Native/src/Interoperability/NativePresentationRuntimeBoundary.cpp`
- Modify: `Native/tests/NativePresentationRuntimeBoundaryTests.cpp`

- [x] **Step 1: Add failing host create/detach/submit boundary tests**

Test null records, zero host image, reserved flags, unknown formats, unsupported requested format/color space, missing surface usage, unequal host/framebuffer/actual extents, exact output clearing, exception containment, no-open-frame host submit, detach with open frame, host submit after detach, and old clear create without host symbol dependencies.

- [x] **Step 2: Verify RED**

Run the focused Native presentation test target.

Expected: unresolved P28 symbol implementations.

- [x] **Step 3: Add host runtime operations without changing old creation policy**

Add:

```cpp
static std::expected<VulkanPresentationRuntime, CreationFailure>
createHostImagePresentation(const HostImageCreateInfo& createInfo);

std::expected<void, VkResult> detachHostImagePresentationResources();
SubmitFrameResult submitAndPresentHostImageFrame();
```

Factor common swapchain creation, but keep old clear creation's SRGB preference and
observable result unchanged. Host creation must select only the exact requested UNORM
surface format with `SRGB_NONLINEAR`, require both swapchain usages, validate exact
extent, then attach `VulkanHostImagePresentationResources`.

- [x] **Step 4: Implement the three P28 C symbols**

Clear output records before validation, validate fixed-width scalars before casting,
contain every C++ exception, and map `InvalidArgument`, `UnsupportedSurface`,
`VulkanFailure`, and `InternalFailure` without losing raw `VkResult`. Detach is
idempotent and writes fence-wait failure into the reusable 8-byte result without
destroying host resources. Host submit selects the prerecorded matrix entry and
delegates to the existing queue submit/present path.

- [x] **Step 5: Verify all Native presentation regressions**

Run:

```powershell
cmake --build Native/build-host-image-plan --config Debug
ctest --test-dir Native/build-host-image-plan -C Debug --output-on-failure
```

Expected: all Native tests pass, including the unchanged clear boundary suite.

- [x] **Step 6: Commit runtime and boundary integration**

Commit subject: `feature/host-image-presentation-runtime` with the mandatory §5.3 body.

## Task 5: Add Real Vulkan Pixel Execution Coverage

**Files:**
- Create: `Native/tests/HostImagePresentationExecutionTests.cpp`
- Modify: `Native/tests/CMakeLists.txt`
- Modify: `Native/tests/TestVulkanDeviceHarness.hpp`

- [x] **Step 1: Write the corner-distinct execution test**

Create a 4x4 RGBA8 host image in `GENERAL` with distinct red/green/blue/white corners,
execute the same fullscreen pipeline into a 4x4 UNORM destination image, copy to a
host-visible buffer, and compare all 64 bytes. The expected array must encode the
Minecraft Y inversion explicitly so a flipped or channel-swapped result fails.

- [x] **Step 2: Verify RED or harness skip reason**

Run:

```powershell
ctest --test-dir Native/build-host-image-plan -C Debug --output-on-failure -R HostImagePresentationExecution
```

Expected: test is absent/fails before registration. Local SKIP is allowed only for the
existing documented missing-driver/capability condition; CI lavapipe must execute it.

- [x] **Step 3: Register and make the harness graphics-capable**

Require a queue family with `VK_QUEUE_GRAPHICS_BIT`, enable dynamic rendering through
the Vulkan 1.2 KHR feature chain, and use the generated production shader bytes. Do not
duplicate shader source or composition logic in the test.

- [x] **Step 4: Run the real execution test and full CTest**

Expected: exact byte equality and full Native GREEN.

- [x] **Step 5: Commit GPU execution coverage**

Commit subject: `test/host-image-presentation-execution` with the mandatory §5.3 body.

## Task 6: Implement the Core Host-Image FFM Binding

**Files:**
- Create: `Core/src/main/java/barrieww/core/interoperability/PresentationImageFormat.java`
- Create: `Core/src/main/java/barrieww/core/interoperability/HostImagePresentationBinding.java`
- Modify: `Core/src/main/java/barrieww/core/interoperability/NativePresentationRuntime.java`
- Modify: `Core/src/test/java/barrieww/core/interoperability/PresentationRuntimeUnitTests.java`
- Modify: `Core/src/test/java/barrieww/core/interoperability/PresentationRuntimeBindingTests.java`
- Modify: `Core/src/demoTest/java/barrieww/core/interoperability/NativePresentationRuntimeIntegrationTests.java`

- [x] **Step 1: Write failing Core layout and binding tests**

Pin every P28 offset, create byte size `96`/alignment `8`, detach-result byte size
`8`/alignment `4`, values `1/2`, exact symbol names,
absence of `Linker.Option.critical`, host-only symbol resolution, exact scalar writes,
reused detach/submit segment identity, raw detach `VkResult`, idempotent detach status,
host submit decoding, Arena
closure after create failure, and one-attempt destroy.

- [x] **Step 2: Run Core tests and verify RED**

Run:

```powershell
& "Core/gradlew.bat" -p Core test --tests "barrieww.core.interoperability.PresentationRuntime*" --no-daemon
```

Expected: compilation failures for the new format/binding/factory APIs.

- [x] **Step 3: Add immutable neutral values and the host binding**

Use this Core shape:

```java
public enum PresentationImageFormat {
    R8G8B8A8_UNORM(1),
    B8G8R8A8_UNORM(2);
}

public record HostImagePresentationBinding(
    long hostImageHandle,
    PresentationImageFormat hostImageFormat,
    PresentationImageFormat requestedSurfaceFormat,
    int width,
    int height) {
}
```

Validate nonzero handle, nonnull formats, and positive exact dimensions in the host
factory rather than adding Minecraft concepts to Core.

- [x] **Step 4: Add a host-only runtime factory and reusable operations**

Add `createHostImagePresentation(...)`, `detachHostImagePresentationResources()`, and
`submitAndPresentHostImageFrame()`. Only the host factory resolves the three additive
symbols. Keep old `create(...)` able to load an older Native Version 1 library. Store
all post-create handles as final members tied to the confined lookup Arena; perform no
per-frame Arena creation or symbol lookup.

- [x] **Step 5: Run unit and real FFM rejected-input tests**

Build Native first, then run:

```powershell
$nativeLibraryPath = (Resolve-Path "Native/build-host-image-plan/ffm/BarriEwwNativeFfm.dll").Path
& "Core/gradlew.bat" -p Core clean test demoTest nativeIntegrationTest jacocoTestReport jacocoTestCoverageVerification -PbarriewwNativeLibraryPath="$nativeLibraryPath" --no-daemon
```

Expected: all tasks pass and Core line coverage is at least 90%.

- [x] **Step 6: Commit the Core binding**

Commit subject: `feature/host-image-presentation-binding` with the mandatory §5.3 body.

## Task 7: Extract and Validate the Minecraft Host Binding

**Files:**
- Create: `Mod/src/main/java/barrieww/mod/MinecraftHostImagePresentationBindingExtractor.java`
- Modify: `Mod/src/main/java/barrieww/mod/PresentationGenerationInputs.java`
- Delete: `Mod/src/main/java/barrieww/mod/MinecraftClearTakeoverReadiness.java`
- Create: `Mod/src/test/java/barrieww/mod/MinecraftHostImagePresentationBindingExtractorTests.java`
- Delete: `Mod/src/test/java/barrieww/mod/MinecraftClearTakeoverReadinessTests.java`

- [x] **Step 1: Write failing extractor truth-table tests**

Accept only live `VulkanGpuTexture`/`VulkanGpuTextureView`, exact `RGBA8_UNORM`, nonzero
`vkImage`, required copy-source and texture-binding usage, full base mip, one mip/layer,
2D positive exact texture/view/configuration extents, and original surface formats
`37 -> R8G8B8A8_UNORM`, `44 -> B8G8R8A8_UNORM`. Reject SRGB values `43/50` and all
unknown values.

- [x] **Step 2: Run Mod tests and verify RED**

Run:

```powershell
& "Mod/gradlew.bat" -p Mod test --tests "barrieww.mod.MinecraftHostImagePresentationBindingExtractorTests" --no-daemon
```

Expected: test compilation fails because the extractor does not exist.

- [x] **Step 3: Implement the Minecraft-only extractor**

Return `Optional<HostImagePresentationBinding>` and keep all casts to Minecraft Vulkan
classes inside this file. `PresentationGenerationInputs` replaces the readiness boolean
with the nonnull binding and target generation; `isReady()` requires exact extents.

- [x] **Step 4: Run extractor and existing Mod tests**

Expected: extractor GREEN and no Core/Native package imports of Minecraft or LWJGL.

- [x] **Step 5: Commit host binding extraction**

Commit subject: `feature/minecraft-host-image-binding` with the mandatory §5.3 body.

## Task 8: Add Resize Tracking, Detach, and Coordinator Preparation

**Files:**
- Create: `Mod/src/main/java/barrieww/mod/PresentationGenerationPreparation.java`
- Create: `Mod/src/main/java/barrieww/mod/MainRenderTargetGenerationTracker.java`
- Create: `Mod/src/main/java/barrieww/mod/MainRenderTargetResizeListener.java`
- Modify: `Mod/src/main/java/barrieww/mod/PresentationRuntime.java`
- Modify: `Mod/src/main/java/barrieww/mod/PresentationRuntimeFactory.java`
- Modify: `Mod/src/main/java/barrieww/mod/CorePresentationRuntime.java`
- Modify: `Mod/src/main/java/barrieww/mod/CorePresentationRuntimeFactory.java`
- Modify: `Mod/src/main/java/barrieww/mod/PresentationTakeoverCoordinator.java`
- Modify: `Mod/src/test/java/barrieww/mod/PresentationTakeoverCoordinatorTests.java`
- Create: `Mod/src/test/java/barrieww/mod/MainRenderTargetGenerationTrackerTests.java`

- [x] **Step 1: Write failing state-machine tests**

Pin `close old -> preparation -> create host -> begin -> clear prime -> commit`, no
preparation after old close failure, vanilla continuation after pre-commit failure,
terminal interception after uncertain close, detach before resize publication,
idempotent detach, clear submit after detach with an open frame, host submit while
attached, and no taken-over/no-runtime state.

- [x] **Step 2: Verify RED**

Run the two focused Mod test classes.

Expected: missing preparation/tracker/detach APIs.

- [x] **Step 3: Extend runtime interfaces and Core adapters**

Add:

```java
void detachHostImagePresentationResources()
    throws NativePresentationRuntimeException;

PresentationFrameStatus submitAndPresentHostImageFrame()
    throws NativePresentationRuntimeException;
```

The factory receives `HostImagePresentationBinding`. The Core adapter delegates
without duplicating numeric status mapping.

- [x] **Step 4: Implement coordinator preparation and attached-state selection**

`configure(Preparation)` must retire the old complete runtime before invoking the
callback. A committed runtime tracks whether host resources are attached. Normal
present selects host submit; successful detach switches the already-open and subsequent
frames to clear submit and sets `requiresReconfiguration`. Detach failure preserves the
borrowed image, marks terminal interception, logs once, and returns failure to the
resize listener.

- [x] **Step 5: Implement the render-thread tracker**

The tracker supports one active surface listener, main-target identity checking,
monotonic TAIL publication, listener removal before surface close, and no retained
closed coordinator. Keep operations allocation-free after registration.

- [x] **Step 6: Run focused and full Mod unit tests**

Expected: all coordinator/tracker tests pass.

- [x] **Step 7: Commit lifecycle policy**

Commit subject: `feature/host-image-presentation-generation` with the mandatory §5.3 body.

## Task 9: Wire Minecraft Resize and Vulkan Surface Mixins

**Files:**
- Create: `Mod/src/main/java/barrieww/mod/mixin/GameRendererMixin.java`
- Create: `Mod/src/main/java/barrieww/mod/mixin/RenderTargetMixin.java`
- Modify: `Mod/src/main/java/barrieww/mod/mixin/VulkanGpuSurfaceMixin.java`
- Modify: `Mod/src/main/resources/barrieww.mixins.json`
- Modify: Mod mixin structure tests under `Mod/src/test/java/barrieww/mod`

- [x] **Step 1: Write failing bytecode/resource structure tests**

Pin cancellable `GameRenderer.resize(II)V` HEAD, `RenderTarget.resize(II)V` TAIL,
surface `swapchainImageFormat` shadow, registration/unregistration, host submit at
present, and both mixin names in `barrieww.mixins.json`.

- [x] **Step 2: Verify RED**

Run Mod `test`.

Expected: missing mixins and surface behavior.

- [x] **Step 3: Implement complete-resize ownership hooks**

`GameRendererMixin` HEAD asks the tracker/listener to detach. Cancel the complete
`GameRenderer.resize` on failure so neither main target nor level renderer is changed.
`RenderTargetMixin` TAIL publishes only when `(Object)this` is the current main target.

- [x] **Step 4: Implement configure-time host preparation**

In `VulkanGpuSurfaceMixin.configure` pass a callback that, after coordinator retirement,
calls public `GameRenderer.resize(width,height)` when needed, extracts the exact binding,
and snapshots generation/handle. Shadow Minecraft's selected raw surface format only
long enough to map `37/44`; do not pass raw `VkFormat` into Core.

At present, submit the host image while attached or clear while detached. Keep all
interception at backend methods and preserve outer `GpuSurface` state transitions.
Unregister the resize listener before the coordinator's sole close attempt.

- [x] **Step 5: Run Mod tests and remapJar**

Run:

```powershell
$coreLibraryPath = (Resolve-Path "Core/build/libs/BarriEwwCore-0.1.0.jar").Path
$nativeLibraryPath = (Resolve-Path "Native/build-host-image-plan/ffm/BarriEwwNativeFfm.dll").Path
& "Mod/gradlew.bat" -p Mod clean test remapJar -PbarriewwCoreLibraryPath="$coreLibraryPath" -PbarriewwNativeLibraryPath="$nativeLibraryPath" --no-daemon
```

Expected: tests pass and the remapped jar contains both new mixins.

- [x] **Step 6: Commit Minecraft integration**

Commit subject: `feature/minecraft-host-image-presentation` with the mandatory §5.3 body.

## Task 10: Complete Build, Packaging, CI, and Coverage Gates

**Files:**
- Modify: `.github/workflows/ci.yml`
- Modify: `Mod/build.gradle.kts`
- Modify: `Native/CMakeLists.txt`
- Modify: `Native/tests/CMakeLists.txt`

- [x] **Step 1: Add failing package-content and CI dependency checks**

Register `verifyRemappedJarContents` in `Mod/build.gradle.kts`, make it depend on
`remapJar`, open `tasks.jar.archiveFile`, and require entries for both mixins,
tracker/extractor classes, updated Core classes, mixin JSON, and the platform Native
library. Make `check` depend on this task. Require `glslc` in all Linux Vulkan jobs:
Native matrix, Native coverage, and Mod packaging.

- [x] **Step 2: Verify package check RED**

Run Mod `check remapJar` against freshly built Native/Core artifacts.

Expected: package verification fails before the new entries/task wiring exists.

- [x] **Step 3: Wire build dependencies without adding runtime switches**

Install `glslc` only in Linux Vulkan CI cells. Preserve P29: Linux Vulkan ON with
lavapipe for `THREADED_RECORDING=ON/OFF`; Windows backend OFF for both variants. Keep
Native/Core/Mod coverage gates at 90%. Do not add a feature CMake option for composition;
it is part of the Vulkan presentation implementation.

- [x] **Step 4: Run the complete automated gate**

Run both Native variants, full Core FFM integration, and Mod packaging:

```powershell
cmake -S Native -B Native/build-host-image-on -DCMAKE_BUILD_TYPE=Release -DBARRIEWW_BACKEND_VULKAN=ON -DTHREADED_RECORDING=ON -DBARRIEWW_BUILD_TESTS=ON
cmake --build Native/build-host-image-on --config Release
ctest --test-dir Native/build-host-image-on -C Release --output-on-failure
cmake -S Native -B Native/build-host-image-off -DCMAKE_BUILD_TYPE=Release -DBARRIEWW_BACKEND_VULKAN=ON -DTHREADED_RECORDING=OFF -DBARRIEWW_BUILD_TESTS=ON
cmake --build Native/build-host-image-off --config Release
ctest --test-dir Native/build-host-image-off -C Release --output-on-failure
$nativeLibraryPath = (Resolve-Path "Native/build-host-image-on/ffm/BarriEwwNativeFfm.dll").Path
& "Core/gradlew.bat" -p Core clean test demoTest nativeIntegrationTest jacocoTestReport jacocoTestCoverageVerification -PbarriewwNativeLibraryPath="$nativeLibraryPath" --no-daemon
& "Core/gradlew.bat" -p Core assemble --no-daemon
$coreLibraryPath = (Resolve-Path "Core/build/libs/BarriEwwCore-0.1.0.jar").Path
& "Mod/gradlew.bat" -p Mod clean check jacocoTestReport jacocoTestCoverageVerification remapJar verifyRemappedJarContents -PbarriewwCoreLibraryPath="$coreLibraryPath" -PbarriewwNativeLibraryPath="$nativeLibraryPath" --no-daemon
```

Expected: every command passes; Native/Core/Mod line coverage is at least 90%.

- [x] **Step 5: Commit build and CI integration**

Commit subject: `feature/host-image-presentation-build` with the mandatory §5.3 body.

## Task 11: Perform Visible D9.2 Acceptance and Record Evidence

**Files:**
- Modify: `vibe-docs/adr/ADR-0006-NativeOwnedMinecraftPresentation.md`
- Do not track: screenshots, runtime logs, extracted DLLs, or build directories

- [ ] **Step 1: Build the exact Release artifacts and record hashes**

Use the repository's Release command with Vulkan/threaded recording on. Record the
exact command and SHA-256 of the packaged Native library and remapped Mod jar.

- [ ] **Step 2: Capture the original baseline**

Run the pre-D9.2 `dev` build at a fixed client extent. Capture a static menu page and a
paused real world with fixed camera, excluding cursor and window border.

- [ ] **Step 3: Capture takeover output and lifecycle regressions**

Run the D9.2 build at the same extent and scenes. Repeat initial view, two resizes,
minimize/restore, and focus away/back. Capture the first settled frame after each
generation and normal teardown logs.

- [ ] **Step 4: Compare pixels and diagnostics**

Require zero client-area pixel difference, no Y flip, channel swap, or gamma shift. If
the OS compositor prevents exact capture alignment, compare fixed interior samples and
full histograms against the Minecraft main-target screenshot and record the exact
capture limitation. Require no Vulkan validation error and one destroy attempt.

- [ ] **Step 5: Write the D9.2 empirical record**

Add a dated ADR subsection with commands, extents, sample counts/differences, hashes,
resize/focus results, and teardown result. Do not claim automated OS-window coverage.

- [ ] **Step 6: Run final verification and inspect the diff**

Run the complete Task 10 gate again, then:

```powershell
git status --short --branch
git diff --check origin/dev...HEAD
git diff --stat origin/dev...HEAD
```

Expected: only intended source/docs changes and known untracked evidence/build
directories remain; every automated gate passes.

- [ ] **Step 7: Commit acceptance evidence record**

Commit subject: `test/host-image-presentation-acceptance` with the mandatory §5.3 body.

## Task 12: Review the Whole Branch Before MR

**Files:**
- Review every file in `git diff origin/dev...HEAD`

- [ ] **Step 1: Invoke the requesting-code-review skill**

Request review against the merged design and §6.7.4, emphasizing ABI compatibility,
borrowed-image destruction order, open-frame detach fences, barrier correctness,
allocation-free hot path, and missing tests.

- [ ] **Step 2: Resolve findings with receiving-code-review and TDD**

For each accepted finding, add a failing regression test, verify RED, make the minimal
fix, verify GREEN, and create a new compliant commit. Do not amend prior commits.

- [ ] **Step 3: Re-run verification-before-completion**

Run all Task 10 commands and inspect final status/diff. Push the feature branch only
after fresh evidence confirms every gate.

- [ ] **Step 4: Open the MR to `dev`**

Review all commits, include the D9.2 visible evidence summary, and use the branch
`feature/host-image-presentation-composition`. Do not merge until full CI passes.
