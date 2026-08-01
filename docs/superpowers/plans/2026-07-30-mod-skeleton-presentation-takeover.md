# Mod Skeleton and Presentation Takeover Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the independent Fabric `Mod/` module and complete ADR-0006 D9 step 1 by replacing Minecraft's Vulkan backend acquire/blit/present operations with a Native-owned pure-color swapchain path.

**Architecture:** Keep Minecraft/Fabric/LWJGL host knowledge in `Mod/`, move the host-neutral presentation FFM binding into Core main, and add one additive Native Version 1 clear-submit entry point. A thin `VulkanGpuSurfaceMixin` preserves the outer `GpuSurface` state machine while a testable coordinator owns generation readiness, resize, delayed fallback, and teardown.

**Tech Stack:** C++23, Vulkan 1.3, CMake 4.4, Catch2, Java 25 FFM, Gradle 9.6.1, JUnit 5, fabric-loom 1.17, Fabric Loader 0.19.3, Fabric API 0.153.0+26.2, Mixin, Minecraft 26.2.

---

## File Map

**Native interface and tests**

- Modify `Native/include/BarriEww/Vulkan/VulkanPresentationRuntime.hpp`: declare clear submission for an already-open frame.
- Modify `Native/src/Vulkan/VulkanPresentationRuntime.cpp`: split existing combined clear operation.
- Modify `Native/include/BarriEww/Interoperability/NativePresentationRuntimeBoundary.hpp`: declare the additive Version 1 C entry point.
- Modify `Native/src/Interoperability/NativePresentationRuntimeBoundary.cpp`: contain and translate the new call.
- Modify `Native/tests/NativePresentationRuntimeBoundaryTests.cpp`: exact split-operation and boundary assertions.

**Core binding**

- Move five host-neutral classes from `Core/src/demo/java/barrieww/core/interoperability/` to `Core/src/main/java/barrieww/core/interoperability/`.
- Move two binding tests from `Core/src/demoTest/java/barrieww/core/interoperability/` to `Core/src/test/java/barrieww/core/interoperability/`.
- Modify `Core/src/main/java/barrieww/core/interoperability/NativePresentationRuntime.java`: resolve the new symbol once and provide reusable status-only frame storage.
- Modify `Core/build.gradle.kts`: keep visible-demo tests separate while running presentation FFM integration from the main test source set.

**Mod build and loading**

- Create `Mod/settings.gradle.kts`, `Mod/build.gradle.kts`, `Mod/gradle.properties`, and a Gradle wrapper copied byte-for-byte from Core.
- Create `Mod/src/main/resources/fabric.mod.json` and `Mod/src/main/resources/barrieww.mixins.json`.
- Create `Mod/src/main/java/barrieww/mod/BarriEwwClientInitializer.java`: Fabric client entry point.
- Create `Mod/src/main/java/barrieww/mod/NativeLibraryExtractor.java`: content-hash extraction of the embedded shared library.
- Create `Mod/src/test/java/barrieww/mod/NativeLibraryExtractorTests.java`: extraction and reuse tests.

**Mod capability and host adaptation**

- Create `Mod/src/main/java/barrieww/mod/VulkanDeviceCapabilityState.java`: one-device capability result.
- Create `Mod/src/main/java/barrieww/mod/mixin/VulkanBackendMixin.java`: `vkCreateDevice`-time BDA feature augmentation.
- Create `Mod/src/main/java/barrieww/mod/mixin/GpuDeviceAccessor.java`: lazy backend accessor.
- Create `Mod/src/main/java/barrieww/mod/MinecraftPresentationGenerationInputs.java`: validate and flatten borrowed handles.
- Create `Mod/src/test/java/barrieww/mod/VulkanDeviceCapabilityStateTests.java`: capability state tests.

**Mod takeover policy and hooks**

- Create `Mod/src/main/java/barrieww/mod/PresentationRuntime.java`: fakeable allocation-free frame interface.
- Create `Mod/src/main/java/barrieww/mod/PresentationRuntimeFactory.java`: fakeable generation factory.
- Create `Mod/src/main/java/barrieww/mod/CorePresentationRuntime.java`: Core binding adapter.
- Create `Mod/src/main/java/barrieww/mod/CorePresentationRuntimeFactory.java`: production factory.
- Create `Mod/src/main/java/barrieww/mod/PresentationGenerationInputs.java`: host-neutral generation readiness record.
- Create `Mod/src/main/java/barrieww/mod/PresentationTakeoverCoordinator.java`: generation state machine.
- Create `Mod/src/test/java/barrieww/mod/PresentationTakeoverCoordinatorTests.java`: readiness, frame, fallback, and teardown tests.
- Create `Mod/src/main/java/barrieww/mod/mixin/VulkanGpuSurfaceMixin.java`: backend-only interception.

**Build orchestration and architecture record**

- Modify `build.sh` and `build.bat`: pass exact Core and Native artifacts to Mod.
- Modify `vibe-docs/adr/ADR-0006-NativeOwnedMinecraftPresentation.md`: close D2 with verified backend-layer hooks and preserved wrapper state.

### Task 1: Split Native Clear Submission and Extend the Version 1 Boundary

**Files:**
- Modify: `Native/tests/NativePresentationRuntimeBoundaryTests.cpp`
- Modify: `Native/include/BarriEww/Vulkan/VulkanPresentationRuntime.hpp`
- Modify: `Native/src/Vulkan/VulkanPresentationRuntime.cpp`
- Modify: `Native/include/BarriEww/Interoperability/NativePresentationRuntimeBoundary.hpp`
- Modify: `Native/src/Interoperability/NativePresentationRuntimeBoundary.cpp`

- [ ] **Step 1: Write failing split-operation boundary tests**

Add observed clear channels beside the existing replacement state:

```cpp
std::array<float, 4> g_observedClearColor{};
```

Reset it in `resetPresentationState`, copy `clearColor->float32` in the
`vkCmdClearColorImage` replacement, and add these exact cases:

```cpp
TEST_CASE("Presentation clear submission requires an open frame",
          "[presentationRuntime]") {
    resetPresentationState();
    const std::uint64_t runtimeAddress = openRuntime();
    barrieww::NativePresentationSubmitFrameResultVersion1 submitResult{};

    REQUIRE(barriEwwSubmitAndPresentClearFrameVersion1(
                runtimeAddress, 0.1f, 0.3f, 0.7f, &submitResult)
            == NativePresentationRuntimeOperationResult::InvalidArgument);
    REQUIRE(barriEwwSubmitAndPresentClearFrameVersion1(
                runtimeAddress, 0.1f, 0.3f, 0.7f, nullptr)
            == NativePresentationRuntimeOperationResult::InvalidArgument);
    REQUIRE(barriEwwDestroyPresentationRuntimeVersion1(runtimeAddress)
            == NativePresentationRuntimeOperationResult::Success);
}

TEST_CASE("Presentation clear submission completes an open frame",
          "[presentationRuntime]") {
    resetPresentationState();
    const std::uint64_t runtimeAddress = openRuntime();
    barrieww::NativePresentationBeginFrameResultVersion1 beginResult{};
    barrieww::NativePresentationFrameMetricsVersion1 priorMetrics{};
    barrieww::NativePresentationSubmitFrameResultVersion1 submitResult{};

    REQUIRE(barriEwwBeginPresentationFrameVersion1(
                runtimeAddress, 1280u, 720u, &beginResult, &priorMetrics)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(barriEwwSubmitAndPresentClearFrameVersion1(
                runtimeAddress, 0.1f, 0.3f, 0.7f, &submitResult)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(submitResult.frameStatusValue
            == static_cast<std::uint32_t>(
                barrieww::VulkanPresentationRuntime::FrameStatus::Success));
    REQUIRE(g_observedClearColor == std::array<float, 4>{0.1f, 0.3f, 0.7f, 1.0f});
    REQUIRE(g_clearImageCallCount == 1u);
    REQUIRE(g_submitCallCount == 1u);
    REQUIRE(g_presentCallCount == 1u);
    REQUIRE(barriEwwDestroyPresentationRuntimeVersion1(runtimeAddress)
            == NativePresentationRuntimeOperationResult::Success);
}
```

- [ ] **Step 2: Build the focused target and verify RED**

Run:

```powershell
cmake --build Native/build --target BarriEwwNativePresentationRuntimeTests --config Release
```

Expected: compilation fails because
`barriEwwSubmitAndPresentClearFrameVersion1` is undeclared.

- [ ] **Step 3: Add the Native runtime split method**

Declare this immediately before `presentClearFrame`:

```cpp
/**
 * @brief Records a clear for the currently open frame, submits it and presents it.
 * @param const float[3] clearColor RGB clear channels; alpha is forced to 1
 * @return SubmitFrameResult Submit/present status; VK_NOT_READY when no frame is open
 */
[[nodiscard]] SubmitFrameResult submitAndPresentClearFrame(const float clearColor[3]);
```

Implement it and make the existing combined method delegate:

```cpp
VulkanPresentationRuntime::SubmitFrameResult
VulkanPresentationRuntime::submitAndPresentClearFrame(const float clearColor[3]) {
    SubmitFrameResult submitResult{};
    if (!m_isFrameOpen) {
        submitResult.vulkanResult = VK_NOT_READY;
        return submitResult;
    }
    VkCommandBuffer commandBuffer = m_frameCommandBuffers[m_currentFrameSlot];
    recordClearCommandBuffer(commandBuffer, m_images[m_acquiredImageIndex], clearColor);
    return submitAndPresentFrame(commandBuffer);
}

VulkanPresentationRuntime::SubmitFrameResult VulkanPresentationRuntime::presentClearFrame(
    std::uint32_t framebufferWidth, std::uint32_t framebufferHeight,
    const float clearColor[3], BeginFrameResult& outBeginResult) {
    outBeginResult = beginFrame(framebufferWidth, framebufferHeight);
    if (outBeginResult.status != FrameStatus::Success
        && outBeginResult.status != FrameStatus::Suboptimal) {
        SubmitFrameResult submitResult{};
        submitResult.status = outBeginResult.status;
        submitResult.vulkanResult = outBeginResult.vulkanResult;
        return submitResult;
    }
    return submitAndPresentClearFrame(clearColor);
}
```

- [ ] **Step 4: Add the additive C ABI entry point**

Declare `barriEwwSubmitAndPresentClearFrameVersion1` after the ordinary submit entry
with complete §2 boundary documentation. Implement it with this behavior:

```cpp
extern "C" barrieww::NativePresentationRuntimeOperationResult
barriEwwSubmitAndPresentClearFrameVersion1(
    std::uint64_t runtimeAddress, float clearRed, float clearGreen, float clearBlue,
    barrieww::NativePresentationSubmitFrameResultVersion1* submitResult) noexcept {
    using barrieww::NativePresentationRuntimeOperationResult;
    if (submitResult == nullptr) {
        return NativePresentationRuntimeOperationResult::InvalidArgument;
    }
    *submitResult = {};
    if (runtimeAddress == 0u) {
        return NativePresentationRuntimeOperationResult::InvalidArgument;
    }
    try {
        auto* runtime =
            reinterpret_cast<barrieww::VulkanPresentationRuntime*>(runtimeAddress);
        if (!runtime->isFrameOpen()) {
            return NativePresentationRuntimeOperationResult::InvalidArgument;
        }
        const float clearColor[3] = {clearRed, clearGreen, clearBlue};
        const auto frameResult = runtime->submitAndPresentClearFrame(clearColor);
        submitResult->frameStatusValue = static_cast<std::uint32_t>(frameResult.status);
        submitResult->vulkanResult = frameResult.vulkanResult;
        return NativePresentationRuntimeOperationResult::Success;
    } catch (...) {
        *submitResult = {};
        return NativePresentationRuntimeOperationResult::InternalFailure;
    }
}
```

- [ ] **Step 5: Run focused and full Native verification**

Run:

```powershell
cmake --build Native/build --target BarriEwwNativePresentationRuntimeTests --config Release
& "Native/build/tests/Release/BarriEwwNativePresentationRuntimeTests.exe"
cmake --build Native/build --target BarriEwwNativeTests --config Release
& "Native/build/tests/Release/BarriEwwNativeTests.exe"
```

Expected: all presentation tests pass; the full suite reports at least the existing
61 test cases and 3064 assertions plus the new assertions.

- [ ] **Step 6: Commit the Native additive interface**

```text
feature/presentation-clear-submission

改动点:
  - 拆分已开启帧的纯色提交并新增 Version 1 FFM 入口
影响范围:
  - Native presentation runtime 与 FFM shared library
相关文件:
  - Native/include/BarriEww/Vulkan/VulkanPresentationRuntime.hpp — 声明纯色提交
  - Native/src/Vulkan/VulkanPresentationRuntime.cpp — 复用既有清色与提交路径
  - Native/include/BarriEww/Interoperability/NativePresentationRuntimeBoundary.hpp — 声明 additive C ABI
  - Native/src/Interoperability/NativePresentationRuntimeBoundary.cpp — containment 新入口
  - Native/tests/NativePresentationRuntimeBoundaryTests.cpp — 精确断言拆分调用
改动思路:
  - 保留 standalone 组合方法并为 Minecraft acquire/present 分离提供终局调用形态
功能性变化:
  - 支持对已开启 frame 清色、提交并 present
非功能性变化:
  - Version 1 ABI 仅 additive 扩展，既有 symbol 行为不变
```

### Task 2: Promote and Extend the Core Presentation Binding

**Files:**
- Move: `Core/src/demo/java/barrieww/core/interoperability/*.java` listed in the file map
- Move: `Core/src/demoTest/java/barrieww/core/interoperability/*.java` listed in the file map
- Modify: `Core/src/main/java/barrieww/core/interoperability/NativePresentationRuntime.java`
- Modify: `Core/src/test/java/barrieww/core/interoperability/PresentationRuntimeBindingTests.java`
- Modify: `Core/src/test/java/barrieww/core/interoperability/NativePresentationRuntimeIntegrationTests.java`
- Modify: `Core/build.gradle.kts`

- [ ] **Step 1: Move the host-neutral types and tests without changing packages**

Use `apply_patch` move operations for these exact files:

```text
NativePresentationRuntime.java
NativePresentationRuntimeException.java
PresentationBeginFrame.java
PresentationBootstrapHandles.java
PresentationClearFrame.java
PresentationRuntimeBindingTests.java
NativePresentationRuntimeIntegrationTests.java
```

Do not move `VisibleClearDemo.java`; it remains in the `demo` source set and compiles
against Core main output.

- [ ] **Step 2: Add failing Core assertions for the new binding surface**

Add to `PresentationRuntimeBindingTests`:

```java
@Test
void clearSubmissionUsesTheVersionOneSymbolName() {
    assertEquals("barriEwwSubmitAndPresentClearFrameVersion1",
            NativePresentationRuntime.s_submitAndPresentClearFrameSymbolName);
}
```

Extend the real integration test with a separate test-only FFM downcall: open the
configured library with a confined Arena, resolve
`s_submitAndPresentClearFrameSymbolName`, invoke it with runtime address zero and an
8-byte submit-result segment, and assert operation result `1`. This duplicates no
production binding logic and keeps numeric mapping assertions out of Mod.

- [ ] **Step 3: Run Core tests and verify RED**

Run:

```powershell
& "Core/gradlew.bat" -p Core test --no-daemon
```

Expected: compilation fails because
`s_submitAndPresentClearFrameSymbolName` does not exist.

- [ ] **Step 4: Resolve once and reuse fixed frame storage**

Add the symbol, descriptor, handle, and Arena-owned segments:

```java
public static final String s_submitAndPresentClearFrameSymbolName =
        "barriEwwSubmitAndPresentClearFrameVersion1";

private static final FunctionDescriptor s_submitAndPresentClearDescriptor =
        FunctionDescriptor.of(ValueLayout.JAVA_INT, ValueLayout.JAVA_LONG,
                ValueLayout.JAVA_FLOAT, ValueLayout.JAVA_FLOAT,
                ValueLayout.JAVA_FLOAT, ValueLayout.ADDRESS);

private final MethodHandle m_submitAndPresentClearHandle;
private final MemorySegment m_reusableBeginResult;
private final MemorySegment m_reusablePriorMetrics;
private final MemorySegment m_reusableSubmitResult;
```

Allocate the three segments from `m_libraryArena` during successful creation. Add a
private invocation method used by both the metrics-bearing API and this status-only
API:

```java
public PresentationFrameStatus beginFrameStatus(int framebufferWidth,
                                                int framebufferHeight)
        throws NativePresentationRuntimeException {
    requireOpen();
    invokeBeginFrame(framebufferWidth, framebufferHeight,
            m_reusableBeginResult, m_reusablePriorMetrics);
    return PresentationFrameStatus.fromCode(
            m_reusableBeginResult.get(ValueLayout.JAVA_INT, 0));
}

public PresentationFrameStatus submitAndPresentClearFrame(
        float clearRed, float clearGreen, float clearBlue)
        throws NativePresentationRuntimeException {
    requireOpen();
    int operationResult;
    try {
        operationResult = (int) m_submitAndPresentClearHandle.invokeExact(
                m_runtimeAddress, clearRed, clearGreen, clearBlue,
                m_reusableSubmitResult);
    } catch (Throwable invocationFailure) {
        throw new NativePresentationRuntimeException(
                "Native clear submission invocation failed: " + invocationFailure,
                s_submitAndPresentClearFrameSymbolName, -1, 0);
    }
    if (operationResult != s_operationSuccess) {
        throw new NativePresentationRuntimeException(
                "Native clear submission reported operation result " + operationResult,
                s_submitAndPresentClearFrameSymbolName, operationResult,
                m_reusableSubmitResult.get(ValueLayout.JAVA_INT, 4));
    }
    return PresentationFrameStatus.fromCode(
            m_reusableSubmitResult.get(ValueLayout.JAVA_INT, 0));
}
```

Keep every FFM method's first documentation line as `@note ThreadSafety` and include
`@warning MemoryOwnership`. Preserve `presentClearFrame` behavior and demo metrics.

- [ ] **Step 5: Wire main-source integration testing**

Change `nativeIntegrationTest` to continue using `sourceSets.test` and move the tagged
test there. Keep `demoTest` only for demo-specific classes. No task may silently skip
when `barriewwNativeLibraryPath` is supplied.

- [ ] **Step 6: Run Core unit and real FFM tests**

Run:

```powershell
& "Core/gradlew.bat" -p Core clean test `
  nativeIntegrationTest demoTest `
  -PbarriewwNativeLibraryPath="D:/Repositories/ComputerGraphics/Barri-Eww/Native/build/ffm/BarriEwwNativeFfm.dll" `
  --no-daemon
```

Expected: all three Gradle tasks pass; absence of the exact library path is a
failure, not a skip.

- [ ] **Step 7: Commit the Core production binding**

```text
feature/core-presentation-binding

改动点:
  - 将 presentation FFM binding 提升为 Core 主产物并绑定纯色提交入口
影响范围:
  - Core main、visible demo、Java-to-Native integration tests
相关文件:
  - Core/src/main/java/barrieww/core/interoperability/NativePresentationRuntime.java — 提供可复用 frame storage 与新调用
  - Core/src/main/java/barrieww/core/interoperability/PresentationBootstrapHandles.java — 提升到主产物
  - Core/src/test/java/barrieww/core/interoperability/NativePresentationRuntimeIntegrationTests.java — 验证真实新 symbol
  - Core/build.gradle.kts — 调整 source set 测试归属
改动思路:
  - 让 Mod 只消费 Core API，并保持 symbol lookup 与 status mapping 在 Core
功能性变化:
  - Core 主产物可创建并驱动 Native presentation runtime
非功能性变化:
  - Mod 所用稳态路径复用固定 FFM segment，不产生逐帧 Arena
```

### Task 3: Create the Independent Fabric Mod Build and Native Loader

**Files:**
- Create: all Mod build/resource/initializer/extractor files in the file map
- Delete: `Mod/.gitkeep`
- Test: `Mod/src/test/java/barrieww/mod/NativeLibraryExtractorTests.java`

- [ ] **Step 1: Create the Loom build and wrapper**

Copy Core's four wrapper files without modifying their bytes. Create
`Mod/settings.gradle.kts` with Fabric, Maven Central, and plugin portal repositories.
Create `gradle.properties` with:

```properties
org.gradle.jvmargs=-Xmx2G
org.gradle.parallel=true
fabric.loom.disableObfuscation=true
minecraftVersion=26.2
fabricLoaderVersion=0.19.3
fabricLoomVersion=1.17.17
fabricApiVersion=0.153.0+26.2
modVersion=0.1.0
```

In `build.gradle.kts`, apply fabric-loom, Java library, and JaCoCo; use Java 25;
depend on the exact Core jar path from `barriewwCoreLibraryPath` with fallback
`../Core/build/libs/BarriEwwCore-0.1.0.jar`; and fail `assemble` when either Core or
`barriewwNativeLibraryPath` is absent. Embed the Native file at:

```text
barrieww/native/windows-x86_64/BarriEwwNativeFfm.dll
barrieww/native/linux-x86_64/libBarriEwwNativeFfm.so
```

Configure JUnit 5, JaCoCo line coverage `0.90`, and client run JVM argument
`--enable-native-access=ALL-UNNAMED`.

- [ ] **Step 2: Add metadata and a failing extraction test**

Create `fabric.mod.json` with client-only environment, client entry point
`barrieww.mod.BarriEwwClientInitializer`, mixin file `barrieww.mixins.json`, and exact
Minecraft/Java dependencies. Add a test resource containing bytes `42 45 57 57` and
write:

```java
@Test
void repeatedExtractionReusesTheContentHashPath() throws Exception {
    Path firstPath = NativeLibraryExtractor.extract(
            "/barrieww/native-test/BarriEwwNativeFfm.dll");
    Path secondPath = NativeLibraryExtractor.extract(
            "/barrieww/native-test/BarriEwwNativeFfm.dll");
    assertEquals(firstPath, secondPath);
    assertArrayEquals(new byte[] {0x42, 0x45, 0x57, 0x57},
            Files.readAllBytes(firstPath));
    assertTrue(firstPath.isAbsolute());
}
```

- [ ] **Step 3: Run the Mod test and verify RED**

Run:

```powershell
& "Mod/gradlew.bat" -p Mod test `
  -PbarriewwCoreLibraryPath="D:/Repositories/ComputerGraphics/Barri-Eww/Core/build/libs/BarriEwwCore-0.1.0.jar" `
  --no-daemon
```

Expected: compilation fails because `NativeLibraryExtractor` does not exist.

- [ ] **Step 4: Implement hash-based extraction and the client initializer**

`NativeLibraryExtractor.extract` must read the resource once, compute SHA-256 with
`MessageDigest`, create `${java.io.tmpdir}/barrieww-native/<hash>/`, write through a
temporary sibling, atomically move when possible, register `deleteOnExit`, and return
the normalized absolute file. Reject a missing resource with `IOException`.

`BarriEwwClientInitializer` extracts the current platform resource in
`onInitializeClient`, stores the absolute path in an immutable static field published
before any surface configure, and logs one initialization line. Unsupported operating
systems or architectures leave presentation unavailable and log one warning; they do
not crash a non-Vulkan game startup.

- [ ] **Step 5: Run Mod tests and remapped assembly**

Run:

```powershell
& "Mod/gradlew.bat" -p Mod clean test remapJar `
  -PbarriewwCoreLibraryPath="D:/Repositories/ComputerGraphics/Barri-Eww/Core/build/libs/BarriEwwCore-0.1.0.jar" `
  -PbarriewwNativeLibraryPath="D:/Repositories/ComputerGraphics/Barri-Eww/Native/build/ffm/BarriEwwNativeFfm.dll" `
  --no-daemon
```

Expected: tests pass and the remapped jar contains the Core classes plus the Native
resource at the Windows path.

- [ ] **Step 6: Commit the loadable Mod skeleton**

```text
feature/mod-skeleton

改动点:
  - 建立独立 Fabric Mod 构建并嵌入、解压 Core 与 Native 产物
影响范围:
  - Mod build、client initialization、artifact packaging
相关文件:
  - Mod/build.gradle.kts — 定义 Loom、Java 25、测试与产物输入
  - Mod/src/main/resources/fabric.mod.json — 注册 client mod
  - Mod/src/main/java/barrieww/mod/NativeLibraryExtractor.java — 解压 hash 命名 native library
  - Mod/src/main/java/barrieww/mod/BarriEwwClientInitializer.java — 初始化 Mod 慢路径
  - Mod/src/test/java/barrieww/mod/NativeLibraryExtractorTests.java — 验证提取复用
改动思路:
  - Mod 只加载 Core 与 Native 二进制，不复制 FFM binding 责任
功能性变化:
  - Fabric 26.2 client 可加载 Barri-Eww Mod
非功能性变化:
  - 构建缺失或平台不支持时给出确定性失败或 readiness 关闭
```

### Task 4: Negotiate BDA and Extract Borrowed Vulkan Handles

**Files:**
- Create: capability state, backend mixin, accessor, and generation-input files from the file map
- Modify: `Mod/src/main/resources/barrieww.mixins.json`
- Test: `Mod/src/test/java/barrieww/mod/VulkanDeviceCapabilityStateTests.java`

- [ ] **Step 1: Write failing capability state tests**

```java
@Test
void capabilityAppliesOnlyToTheRecordedLogicalDevice() {
    VulkanDeviceCapabilityState state = new VulkanDeviceCapabilityState();
    state.recordNegotiatedDevice(41L, true);
    assertTrue(state.supportsPresentationTakeover(41L));
    assertFalse(state.supportsPresentationTakeover(42L));
    state.recordNegotiatedDevice(43L, false);
    assertFalse(state.supportsPresentationTakeover(43L));
}
```

- [ ] **Step 2: Run the focused test and verify RED**

Run `Mod/gradlew.bat -p Mod test --tests "*VulkanDeviceCapabilityStateTests"` with the
Core property from Task 3. Expected: compilation fails because the state class is absent.

- [ ] **Step 3: Implement device creation negotiation**

Create one `VulkanFeature` for
`VulkanBackend.VK12_FEATURES_STRUCT`, name `bufferDeviceAddress`, and offset
`VkPhysicalDeviceVulkan12Features.BUFFERDEVICEADDRESS`. In a `@ModifyArgs` matching
the verified MC 26.2 `createDevice` descriptor:

1. query `VkPhysicalDeviceFeatures2` with the feature struct chained;
2. copy the incoming `Set<VulkanFeature>`;
3. add the feature only when supported;
4. replace argument 2 with the copied set;
5. record the result after the logical `VkDevice` is available at the existing
   `createVma` injection seam.

Dynamic rendering must only be verified in the incoming feature set because Minecraft
already requires it. Do not add a second runtime mode.

- [ ] **Step 4: Implement lazy handle flattening**

`GpuDeviceAccessor` exposes only:

```java
@Accessor("backend")
GpuDeviceBackend barrieww$getBackend();
```

`MinecraftPresentationGenerationInputs.create` returns `Optional.empty()` unless the
RenderSystem backend is the same `VulkanDevice` owned by the surface, the capability
state matches its logical device address, graphics/present queue addresses and family
indices are identical, all handles are nonzero, and the main target color texture:

```java
texture.getFormat().hasColorAspect()
        && texture.getWidth(0) == framebufferWidth
        && texture.getHeight(0) == framebufferHeight
        && (texture.usage() & GpuTexture.USAGE_COPY_SRC) != 0
        && (texture.usage() & GpuTexture.USAGE_TEXTURE_BINDING) != 0
```

On success return primitive handles in `PresentationBootstrapHandles` and no retained
Minecraft object.

- [ ] **Step 5: Build tests and launch the non-destructive probe**

Run Mod tests, then `runClient` with Core/Native artifact properties. Expected log:
one line with a nonzero borrowed `VkDevice` address and no takeover yet. Close the
client normally and confirm no crash.

- [ ] **Step 6: Commit capability negotiation and handle extraction**

```text
feature/mod-vulkan-bootstrap

改动点:
  - 在设备创建前协商 buffer device address 并惰性提取借用 Vulkan handles
影响范围:
  - Minecraft Vulkan device bring-up 与 Mod readiness
相关文件:
  - Mod/src/main/java/barrieww/mod/mixin/VulkanBackendMixin.java — 注入 capability negotiation
  - Mod/src/main/java/barrieww/mod/mixin/GpuDeviceAccessor.java — 访问 active backend
  - Mod/src/main/java/barrieww/mod/MinecraftPresentationGenerationInputs.java — 校验并扁平化 handles
  - Mod/src/main/java/barrieww/mod/VulkanDeviceCapabilityState.java — 记录匹配 logical device 的结果
改动思路:
  - 在 vkCreateDevice 唯一窗口追加 feature，之后只借用已创建对象
功能性变化:
  - Vulkan backend 可向 Core 提供 presentation bootstrap handles
非功能性变化:
  - 非 Vulkan 或 feature 缺失时 readiness 惰性关闭且不抛异常
```

### Task 5: Implement the Generation-Level Takeover Coordinator

**Files:**
- Create: the six policy/adapter files listed under Mod takeover policy
- Test: `Mod/src/test/java/barrieww/mod/PresentationTakeoverCoordinatorTests.java`

- [ ] **Step 1: Define fakeable interfaces and write RED state-machine tests**

Use this production interface:

```java
public interface PresentationRuntime extends AutoCloseable {
    PresentationFrameStatus beginFrameStatus(int framebufferWidth,
                                             int framebufferHeight)
            throws NativePresentationRuntimeException;
    PresentationFrameStatus submitAndPresentClearFrame(
            float clearRed, float clearGreen, float clearBlue)
            throws NativePresentationRuntimeException;
    @Override
    void close();
}
```

Write independent tests proving:

- unready inputs return `false` and never call the factory;
- ready initial configure returns `true` and creates exactly once;
- ready resize closes the old runtime before creating the replacement;
- replacement creation failure returns `false` so vanilla configure continues;
- successful acquire followed by present calls begin before clear submit;
- blit interception is permitted only while taken over;
- recreate/suboptimal status requests reconfiguration;
- checked frame failure permanently disables takeover for the next configure;
- close is idempotent and closes Native exactly once.

- [ ] **Step 2: Run focused tests and verify RED**

Run `Mod/gradlew.bat -p Mod test --tests "*PresentationTakeoverCoordinatorTests"`.
Expected: compilation fails because the coordinator and interfaces do not exist.

- [ ] **Step 3: Implement the minimal coordinator**

Use fixed fields and no collections:

```java
private static final float s_clearRed = 0.08f;
private static final float s_clearGreen = 0.72f;
private static final float s_clearBlue = 0.93f;
private static final int s_framesInFlightCount = 2;

private final PresentationRuntimeFactory m_runtimeFactory;
private PresentationRuntime m_runtime;
private boolean m_isFrameOpen;
private boolean m_requiresReconfiguration;
private boolean m_isPermanentlyDisabled;
```

`configure` closes the old runtime first, rejects any false readiness bit or permanent
disable, creates the replacement, and only then returns `true`. `beginFrame` and
`present` catch checked Core exceptions, close runtime, set reconfiguration plus
permanent disable, and never throw through the Minecraft backend hook. `present`
submits only when begin returned `SUCCESS` or `SUBOPTIMAL`. All successful methods are
allocation-free and silent.

- [ ] **Step 4: Implement the Core adapter and production factory**

`CorePresentationRuntime` delegates exactly to Core and owns its close. The factory
calls:

```java
NativePresentationRuntime.create(nativeLibraryPath, bootstrapHandles,
        framebufferWidth, framebufferHeight, framesInFlightCount)
```

No symbol name, operation result integer, or FFM type appears in coordinator code.

- [ ] **Step 5: Run focused tests and JaCoCo verification**

Run:

```powershell
& "Mod/gradlew.bat" -p Mod test jacocoTestCoverageVerification `
  -PbarriewwCoreLibraryPath="D:/Repositories/ComputerGraphics/Barri-Eww/Core/build/libs/BarriEwwCore-0.1.0.jar" `
  --no-daemon
```

Expected: all coordinator tests pass and Mod line coverage is at least 90%.

- [ ] **Step 6: Commit the tested takeover policy**

```text
feature/presentation-takeover-coordinator

改动点:
  - 实现 generation readiness、frame 状态、resize 与延迟回退协调器
影响范围:
  - Mod presentation policy 与 Core runtime adaptation
相关文件:
  - Mod/src/main/java/barrieww/mod/PresentationTakeoverCoordinator.java — 统一状态机
  - Mod/src/main/java/barrieww/mod/CorePresentationRuntime.java — 适配 Core binding
  - Mod/src/main/java/barrieww/mod/PresentationRuntimeFactory.java — 隔离 generation 创建
  - Mod/src/test/java/barrieww/mod/PresentationTakeoverCoordinatorTests.java — 覆盖 readiness 与回退
改动思路:
  - 将版本敏感 Mixin 限制为接缝，把策略放入可单测普通 Java 类
功能性变化:
  - 可在完整 readiness 后一次性接管一个 presentation generation
非功能性变化:
  - 稳态 frame 路径无 allocation、lookup 与日志
```

### Task 6: Hook VulkanGpuSurface, Close ADR D2, and Verify End to End

**Files:**
- Create: `Mod/src/main/java/barrieww/mod/mixin/VulkanGpuSurfaceMixin.java`
- Modify: `Mod/src/main/resources/barrieww.mixins.json`
- Modify: `build.sh`
- Modify: `build.bat`
- Modify: `vibe-docs/adr/ADR-0006-NativeOwnedMinecraftPresentation.md`

- [ ] **Step 1: Add exact backend-only Mixin hooks**

Shadow the final `VulkanDevice device`, final `long surface`, and mutable
`swapchainSuboptimal`. Create one coordinator at constructor TAIL. Implement:

```java
@Inject(method = "configure", at = @At("HEAD"), cancellable = true)
private void barrieww$configure(GpuSurface.Configuration configuration,
                                CallbackInfo callbackInformation) {
    Optional<PresentationGenerationInputs> generationInputs =
            MinecraftPresentationGenerationInputs.create(
                    this.device, this.surface, configuration.width(),
                    configuration.height());
    if (generationInputs.isPresent()
            && this.barrieww$m_presentationTakeoverCoordinator.configure(
                    generationInputs.get())) {
        callbackInformation.cancel();
    }
}

@Inject(method = "acquireNextTexture", at = @At("HEAD"), cancellable = true)
private void barrieww$acquireNextTexture(CallbackInfo callbackInformation) {
    if (this.barrieww$m_presentationTakeoverCoordinator.isTakenOver()) {
        this.barrieww$m_presentationTakeoverCoordinator.beginFrame();
        this.swapchainSuboptimal =
                this.barrieww$m_presentationTakeoverCoordinator
                        .requiresReconfiguration();
        callbackInformation.cancel();
    }
}

@Inject(method = "blitFromTexture", at = @At("HEAD"), cancellable = true)
private void barrieww$blitFromTexture(CommandEncoderBackend commandEncoder,
                                      GpuTextureView textureView,
                                      CallbackInfo callbackInformation) {
    if (this.barrieww$m_presentationTakeoverCoordinator.isTakenOver()) {
        callbackInformation.cancel();
    }
}

@Inject(method = "present", at = @At("HEAD"), cancellable = true)
private void barrieww$present(CallbackInfo callbackInformation) {
    if (this.barrieww$m_presentationTakeoverCoordinator.isTakenOver()) {
        this.barrieww$m_presentationTakeoverCoordinator.presentFrame();
        this.swapchainSuboptimal =
                this.barrieww$m_presentationTakeoverCoordinator
                        .requiresReconfiguration();
        callbackInformation.cancel();
    }
}
```

At `close` HEAD, close the coordinator without cancelling vanilla. Do not inject into
`GpuSurface`, `Minecraft.renderFrame`, or `VulkanCommandEncoder`.

- [ ] **Step 2: Pass exact artifacts from both root scripts**

After Core assembly, derive the Core jar and Native shared-library paths already
selected by the root variant. Add both properties to the Mod Gradle invocation:

```text
-PbarriewwCoreLibraryPath=<absolute Core jar>
-PbarriewwNativeLibraryPath=<absolute matching Native shared library>
```

Keep `build.sh` and `build.bat` CLI, validation, ordering, and failure propagation
exactly symmetric.

- [ ] **Step 3: Update ADR-0006 D2 with implementation evidence**

Change D2 from open implementation detail to decided implementation detail. Record:

1. all three frame hooks are on `VulkanGpuSurface` backend methods;
2. outer `GpuSurface.acquireNextTexture` sets `hasImageAcquired` after the cancelled
   backend returns;
3. outer `GpuSurface.blitFromTexture` sets `hasBlittedTexture`, satisfying its own
   `present` precondition;
4. both `Minecraft.renderFrame` `isAcquired()` checks therefore remain true;
5. configure is cancelled only after replacement Native generation creation succeeds.

Do not alter D7 or add D9 steps 2-4.

- [ ] **Step 4: Run all automated regression gates**

Run:

```powershell
cmake --build Native/build --target BarriEwwNativeTests --config Release
& "Native/build/tests/Release/BarriEwwNativeTests.exe"
& "Core/gradlew.bat" -p Core clean test nativeIntegrationTest `
  -PbarriewwNativeLibraryPath="D:/Repositories/ComputerGraphics/Barri-Eww/Native/build/ffm/BarriEwwNativeFfm.dll" `
  --no-daemon
& "Mod/gradlew.bat" -p Mod clean test jacocoTestCoverageVerification remapJar `
  -PbarriewwCoreLibraryPath="D:/Repositories/ComputerGraphics/Barri-Eww/Core/build/libs/BarriEwwCore-0.1.0.jar" `
  -PbarriewwNativeLibraryPath="D:/Repositories/ComputerGraphics/Barri-Eww/Native/build/ffm/BarriEwwNativeFfm.dll" `
  --no-daemon
```

Expected: Native reports at least 61 cases/3064 assertions plus new cases; Core and
Mod report no failed tests; Mod coverage is at least 90%; remapped jar succeeds.

- [ ] **Step 5: Run the visible D9 step 1 acceptance**

Start `runClient` with the same two artifact properties and native access enabled.
Verify the window is entirely RGB `(0.08, 0.72, 0.93)` within swapchain color-space
tolerance. Use Windows window controls to resize repeatedly, minimize for five seconds,
restore, alt-tab twice, and close from the title screen. Acceptance requires:

- pure color after every restore/resize;
- no black/frozen frame persisting after a generation recreation;
- no crash or Vulkan validation error;
- one clean Native runtime destruction before surface/device teardown.

If any observed method order or state differs from the verified MC 26.2 facts, stop
without adding an alternate hook and report the exact mismatch to the owner.

- [ ] **Step 6: Commit backend takeover and D2 evidence**

```text
feature/mod-presentation-takeover

改动点:
  - 在 VulkanGpuSurface 后端层接管 configure、acquire、blit、present 与 close
  - 固化 ADR-0006 D2 的实测屏蔽手法
影响范围:
  - Mod Minecraft 26.2 presentation、根构建入口、ADR-0006
相关文件:
  - Mod/src/main/java/barrieww/mod/mixin/VulkanGpuSurfaceMixin.java — 驱动三个后端接管点
  - Mod/src/main/resources/barrieww.mixins.json — 注册 client mixins
  - build.sh — 传递 POSIX 构建产物路径
  - build.bat — 传递 Windows 构建产物路径
  - vibe-docs/adr/ADR-0006-NativeOwnedMinecraftPresentation.md — 记录 D2 实现结论
改动思路:
  - 保留 GpuSurface 外层状态机，让其自然满足两处 isAcquired 与 blit 前置条件
功能性变化:
  - Minecraft 窗口由 Native swapchain 持续输出固定纯色
非功能性变化:
  - resize、alt-tab、延迟回退与 teardown 具有明确 generation 语义
```

### Task 7: Final Verification and Owner Report

**Files:**
- No production file changes expected

- [ ] **Step 1: Verify repository state and commit boundaries**

Run `git status --short --branch`, `git diff dev...HEAD --check`, and
`git log --oneline dev..HEAD`. Expected: only intended files changed, no whitespace
errors, and design plus six focused implementation commits are present.

- [ ] **Step 2: Run the owner-requested exact regression commands once more**

Run the exact Release Native build/executable and `Core/gradlew test` commands from
the handoff, then Mod `test remapJar`. Record the exact Native case/assertion totals
and Java/Mod test totals from fresh output.

- [ ] **Step 3: Prepare the report without pushing or merging**

Report:

- what changed and why for each increment;
- Native case/assertion totals and Core/Mod test totals;
- visible pure-color, resize, alt-tab, and clean-exit observations;
- branch `feature/mod-skeleton-presentation-takeover`;
- every commit hash in order.

Do not push, merge, or open an MR without a new explicit owner instruction.
