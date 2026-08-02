package barrieww.core.interoperability;

import static java.lang.foreign.MemoryLayout.PathElement.groupElement;

import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.Linker;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.StructLayout;
import java.lang.foreign.SymbolLookup;
import java.lang.foreign.ValueLayout;
import java.lang.invoke.MethodHandle;
import java.nio.file.Path;
import java.util.Objects;
import java.util.Optional;

/**
 * @note ThreadSafety: Thread-confined. The opening thread owns the confined library Arena
 *       and must perform every frame and close operation; concurrent use is unsupported.
 * Opens the non-critical Version 1 Native presentation runtime, borrowing the Java-owned
 * Vulkan bootstrap handles and driving a Native-owned swapchain frame loop (ADR-0004).
 * @warning MemoryOwnership: This instance owns and closes the confined library Arena and the
 * Native runtime. Callers own every Vulkan bootstrap handle and keep it valid until close.
 */
public final class NativePresentationRuntime implements AutoCloseable {
    public static final String s_createSymbolName = "barriEwwCreatePresentationRuntimeVersion1";
    public static final String s_destroySymbolName =
            "barriEwwDestroyPresentationRuntimeVersion1";
    public static final String s_beginFrameSymbolName =
            "barriEwwBeginPresentationFrameVersion1";
    public static final String s_submitFrameSymbolName =
            "barriEwwSubmitPresentationFrameVersion1";
    public static final String s_presentClearFrameSymbolName =
            "barriEwwPresentClearFrameVersion1";
    public static final String s_submitAndPresentClearFrameSymbolName =
            "barriEwwSubmitAndPresentClearFrameVersion1";
    public static final String s_createHostImageSymbolName =
            "barriEwwCreateHostImagePresentationRuntimeVersion1";
    public static final String s_detachHostImageResourcesSymbolName =
            "barriEwwDetachHostImagePresentationResourcesVersion1";
    public static final String s_submitAndPresentHostImageFrameSymbolName =
            "barriEwwSubmitAndPresentHostImageFrameVersion1";

    private static final int s_operationSuccess = 0;
    private static final long s_createInfoByteSize = 72;
    private static final long s_createResultByteSize = 32;
    private static final long s_beginResultByteSize = 40;
    private static final long s_submitResultByteSize = 8;

    static final StructLayout s_hostImageCreateInfoLayout = MemoryLayout.structLayout(
            ValueLayout.JAVA_LONG.withName("instanceHandle"),
            ValueLayout.JAVA_LONG.withName("physicalDeviceHandle"),
            ValueLayout.JAVA_LONG.withName("logicalDeviceHandle"),
            ValueLayout.JAVA_LONG.withName("surfaceHandle"),
            ValueLayout.JAVA_LONG.withName("graphicsQueueHandle"),
            ValueLayout.JAVA_LONG.withName("presentQueueHandle"),
            ValueLayout.JAVA_INT.withName("graphicsQueueFamilyIndex"),
            ValueLayout.JAVA_INT.withName("presentQueueFamilyIndex"),
            ValueLayout.JAVA_INT.withName("framebufferWidth"),
            ValueLayout.JAVA_INT.withName("framebufferHeight"),
            ValueLayout.JAVA_INT.withName("framesInFlightCount"),
            ValueLayout.JAVA_INT.withName("reservedFlags"),
            ValueLayout.JAVA_LONG.withName("hostImageHandle"),
            ValueLayout.JAVA_INT.withName("hostImageFormatValue"),
            ValueLayout.JAVA_INT.withName("requestedSurfaceFormatValue"),
            ValueLayout.JAVA_INT.withName("hostImageWidth"),
            ValueLayout.JAVA_INT.withName("hostImageHeight"));
    static final long s_instanceHandleOffset = s_hostImageCreateInfoLayout.byteOffset(
            groupElement("instanceHandle"));
    static final long s_physicalDeviceHandleOffset = s_hostImageCreateInfoLayout.byteOffset(
            groupElement("physicalDeviceHandle"));
    static final long s_logicalDeviceHandleOffset = s_hostImageCreateInfoLayout.byteOffset(
            groupElement("logicalDeviceHandle"));
    static final long s_surfaceHandleOffset = s_hostImageCreateInfoLayout.byteOffset(
            groupElement("surfaceHandle"));
    static final long s_graphicsQueueHandleOffset = s_hostImageCreateInfoLayout.byteOffset(
            groupElement("graphicsQueueHandle"));
    static final long s_presentQueueHandleOffset = s_hostImageCreateInfoLayout.byteOffset(
            groupElement("presentQueueHandle"));
    static final long s_graphicsQueueFamilyIndexOffset = s_hostImageCreateInfoLayout.byteOffset(
            groupElement("graphicsQueueFamilyIndex"));
    static final long s_presentQueueFamilyIndexOffset = s_hostImageCreateInfoLayout.byteOffset(
            groupElement("presentQueueFamilyIndex"));
    static final long s_framebufferWidthOffset = s_hostImageCreateInfoLayout.byteOffset(
            groupElement("framebufferWidth"));
    static final long s_framebufferHeightOffset = s_hostImageCreateInfoLayout.byteOffset(
            groupElement("framebufferHeight"));
    static final long s_framesInFlightCountOffset = s_hostImageCreateInfoLayout.byteOffset(
            groupElement("framesInFlightCount"));
    static final long s_reservedFlagsOffset = s_hostImageCreateInfoLayout.byteOffset(
            groupElement("reservedFlags"));
    static final long s_hostImageHandleOffset = s_hostImageCreateInfoLayout.byteOffset(
            groupElement("hostImageHandle"));
    static final long s_hostImageFormatValueOffset = s_hostImageCreateInfoLayout.byteOffset(
            groupElement("hostImageFormatValue"));
    static final long s_requestedSurfaceFormatValueOffset =
            s_hostImageCreateInfoLayout.byteOffset(groupElement("requestedSurfaceFormatValue"));
    static final long s_hostImageWidthOffset = s_hostImageCreateInfoLayout.byteOffset(
            groupElement("hostImageWidth"));
    static final long s_hostImageHeightOffset = s_hostImageCreateInfoLayout.byteOffset(
            groupElement("hostImageHeight"));

    static final StructLayout s_detachResultLayout = MemoryLayout.structLayout(
            ValueLayout.JAVA_INT.withName("vulkanResult"),
            ValueLayout.JAVA_INT.withName("reserved"));
    static final long s_detachVulkanResultOffset = s_detachResultLayout.byteOffset(
            groupElement("vulkanResult"));
    static final long s_detachReservedOffset = s_detachResultLayout.byteOffset(
            groupElement("reserved"));

    private static final FunctionDescriptor s_createDescriptor = FunctionDescriptor.of(
            ValueLayout.JAVA_INT, ValueLayout.ADDRESS, ValueLayout.ADDRESS);
    private static final FunctionDescriptor s_destroyDescriptor = FunctionDescriptor.of(
            ValueLayout.JAVA_INT, ValueLayout.JAVA_LONG);
    private static final FunctionDescriptor s_beginDescriptor = FunctionDescriptor.of(
            ValueLayout.JAVA_INT, ValueLayout.JAVA_LONG, ValueLayout.JAVA_INT,
            ValueLayout.JAVA_INT, ValueLayout.ADDRESS, ValueLayout.ADDRESS);
    private static final FunctionDescriptor s_submitDescriptor = FunctionDescriptor.of(
            ValueLayout.JAVA_INT, ValueLayout.JAVA_LONG, ValueLayout.JAVA_LONG,
            ValueLayout.ADDRESS);
    private static final FunctionDescriptor s_presentClearDescriptor = FunctionDescriptor.of(
            ValueLayout.JAVA_INT, ValueLayout.JAVA_LONG, ValueLayout.JAVA_INT,
            ValueLayout.JAVA_INT, ValueLayout.JAVA_FLOAT, ValueLayout.JAVA_FLOAT,
            ValueLayout.JAVA_FLOAT, ValueLayout.ADDRESS, ValueLayout.ADDRESS,
            ValueLayout.ADDRESS);
    private static final FunctionDescriptor s_submitAndPresentClearFrameDescriptor =
            FunctionDescriptor.of(ValueLayout.JAVA_INT, ValueLayout.JAVA_LONG,
                    ValueLayout.JAVA_FLOAT, ValueLayout.JAVA_FLOAT, ValueLayout.JAVA_FLOAT,
                    ValueLayout.ADDRESS);
    static final FunctionDescriptor s_createHostImageDescriptor = FunctionDescriptor.of(
            ValueLayout.JAVA_INT, ValueLayout.ADDRESS, ValueLayout.ADDRESS);
    static final FunctionDescriptor s_detachHostImageResourcesDescriptor = FunctionDescriptor.of(
            ValueLayout.JAVA_INT, ValueLayout.JAVA_LONG, ValueLayout.ADDRESS);
    static final FunctionDescriptor s_submitAndPresentHostImageFrameDescriptor =
            FunctionDescriptor.of(ValueLayout.JAVA_INT, ValueLayout.JAVA_LONG,
                    ValueLayout.ADDRESS);

    private final Arena m_libraryArena;
    private final MethodHandle m_destroyHandle;
    private final MethodHandle m_beginHandle;
    private final MethodHandle m_submitHandle;
    private final MethodHandle m_presentClearHandle;
    private final MethodHandle m_submitAndPresentClearFrameHandle;
    private final MethodHandle m_detachHostImageResourcesHandle;
    private final MethodHandle m_submitAndPresentHostImageFrameHandle;
    private final MemorySegment m_beginResult;
    private final MemorySegment m_priorMetrics;
    private final MemorySegment m_submitResult;
    private final MemorySegment m_detachResult;
    private final long m_runtimeAddress;
    private final int m_selectedFormatValue;
    private final int m_selectedPresentModeValue;
    private final int m_selectedSharingModeValue;
    private final int m_swapchainImageCount;
    private boolean m_isClosed;

    /**
     * @note ThreadSafety: Thread-confined; construction and all subsequent access occur on the
     *       opening presentation thread.
     * Creates the Java owner for one successfully created Native presentation runtime.
     *
     * @param Arena libraryArena Confined owner of the library lookup and reusable output storage
     * @param MethodHandle destroyHandle Non-critical runtime-destroy downcall handle
     * @param MethodHandle beginHandle Non-critical begin-frame downcall handle
     * @param MethodHandle submitHandle Non-critical submit-frame downcall handle
     * @param MethodHandle presentClearHandle Non-critical combined clear-frame downcall handle
     * @param MethodHandle submitAndPresentClearFrameHandle Non-critical clear-submit downcall
     *        handle
     * @param MethodHandle detachHostImageResourcesHandle Optional non-critical host detach handle
     * @param MethodHandle submitAndPresentHostImageFrameHandle Optional non-critical host submit
     *        handle
     * @param MemorySegment beginResult Reusable begin-frame output storage
     * @param MemorySegment priorMetrics Reusable prior-frame metrics output storage
     * @param MemorySegment submitResult Reusable submit-frame output storage
     * @param MemorySegment detachResult Optional reusable host detach output storage
     * @param long runtimeAddress Native-owned opaque presentation runtime address
     * @param int selectedFormatValue Selected neutral swapchain format value
     * @param int selectedPresentModeValue Selected Vulkan present-mode value
     * @param int selectedSharingModeValue Selected Vulkan sharing-mode value
     * @param int swapchainImageCount Number of images in the created swapchain
     * @warning MemoryOwnership: This instance owns libraryArena, all MethodHandles tied to its
     *          lookup, and all reusable MemorySegments. It owns runtimeAddress and destroys that
     *          Native runtime before closing libraryArena. The create caller retains every
     *          borrowed Vulkan bootstrap handle and must keep them alive until close returns.
     */
    private NativePresentationRuntime(Arena libraryArena, MethodHandle destroyHandle,
                                       MethodHandle beginHandle, MethodHandle submitHandle,
                                        MethodHandle presentClearHandle,
                                        MethodHandle submitAndPresentClearFrameHandle,
                                        MethodHandle detachHostImageResourcesHandle,
                                        MethodHandle submitAndPresentHostImageFrameHandle,
                                        MemorySegment beginResult, MemorySegment priorMetrics,
                                        MemorySegment submitResult, MemorySegment detachResult,
                                        long runtimeAddress,
                                       int selectedFormatValue, int selectedPresentModeValue,
                                       int selectedSharingModeValue, int swapchainImageCount) {
        m_libraryArena = libraryArena;
        m_destroyHandle = destroyHandle;
        m_beginHandle = beginHandle;
        m_submitHandle = submitHandle;
        m_presentClearHandle = presentClearHandle;
        m_submitAndPresentClearFrameHandle = submitAndPresentClearFrameHandle;
        m_detachHostImageResourcesHandle = detachHostImageResourcesHandle;
        m_submitAndPresentHostImageFrameHandle = submitAndPresentHostImageFrameHandle;
        m_beginResult = beginResult;
        m_priorMetrics = priorMetrics;
        m_submitResult = submitResult;
        m_detachResult = detachResult;
        m_runtimeAddress = runtimeAddress;
        m_selectedFormatValue = selectedFormatValue;
        m_selectedPresentModeValue = selectedPresentModeValue;
        m_selectedSharingModeValue = selectedSharingModeValue;
        m_swapchainImageCount = swapchainImageCount;
    }

    /**
     * @note ThreadSafety: The returned runtime is confined to the calling thread.
     * Opens the Native FFM shared library and creates a Native-owned swapchain runtime around
     * the borrowed Vulkan bootstrap handles.
     *
     * @param Path nativeLibraryPath Absolute path to BarriEwwNativeFfm
     * @param PresentationBootstrapHandles bootstrapHandles Borrowed Vulkan handles
     * @param int framebufferWidth Initial framebuffer width in pixels
     * @param int framebufferHeight Initial framebuffer height in pixels
     * @param int framesInFlightCount Number of in-flight frame slots
     * @return NativePresentationRuntime The open thread-confined runtime
     * @throws IllegalArgumentException When framebuffer dimensions or framesInFlightCount are not
     *         positive
     * @throws NativeLibraryLoadingException When the library or a symbol cannot be resolved
     * @throws NativePresentationRuntimeException When the Native runtime cannot be created
     * @warning MemoryOwnership: The returned runtime owns its confined library Arena and Native
     *          runtime; Native borrows the caller-owned Vulkan handles until close returns.
     */
    public static NativePresentationRuntime create(Path nativeLibraryPath,
                                                   PresentationBootstrapHandles bootstrapHandles,
                                                   int framebufferWidth, int framebufferHeight,
                                                   int framesInFlightCount)
            throws NativeLibraryLoadingException, NativePresentationRuntimeException {
        Objects.requireNonNull(nativeLibraryPath, "nativeLibraryPath");
        Objects.requireNonNull(bootstrapHandles, "bootstrapHandles");
        validateCreateDimensions(framebufferWidth, framebufferHeight, framesInFlightCount);
        Path absoluteLibraryPath = nativeLibraryPath.toAbsolutePath().normalize();
        if (!nativeLibraryPath.isAbsolute()) {
            throw new NativeLibraryLoadingException(
                    "Native library path must be absolute: " + nativeLibraryPath,
                    absoluteLibraryPath, s_createSymbolName, null);
        }

        Arena libraryArena = Arena.ofConfined();
        MethodHandle createHandle;
        MethodHandle destroyHandle;
        MethodHandle beginHandle;
        MethodHandle submitHandle;
        MethodHandle presentClearHandle;
        MethodHandle submitAndPresentClearFrameHandle;
        MemorySegment beginResult;
        MemorySegment priorMetrics;
        MemorySegment submitResult;
        try {
            SymbolLookup symbolLookup =
                    SymbolLookup.libraryLookup(absoluteLibraryPath, libraryArena);
            MemorySegment[] symbols = resolveSymbolAddresses(
                    symbolLookup, absoluteLibraryPath, false);
            Linker linker = Linker.nativeLinker();
            MethodHandle[] downcallHandles = createOrdinaryDowncallHandles(
                    symbols, linker::downcallHandle);
            createHandle = downcallHandles[0];
            destroyHandle = downcallHandles[1];
            beginHandle = downcallHandles[2];
            submitHandle = downcallHandles[3];
            presentClearHandle = downcallHandles[4];
            submitAndPresentClearFrameHandle = downcallHandles[5];
            beginResult = libraryArena.allocate(s_beginResultByteSize, 8);
            priorMetrics = libraryArena.allocate(PresentationFrameMetrics.s_byteSize, 8);
            submitResult = libraryArena.allocate(s_submitResultByteSize, 4);
        } catch (NativeLibraryLoadingException loadingFailure) {
            libraryArena.close();
            throw loadingFailure;
        } catch (Throwable loadingFailure) {
            libraryArena.close();
            throw new NativeLibraryLoadingException(
                    "Failed to open Native presentation runtime at " + absoluteLibraryPath,
                    absoluteLibraryPath, s_createSymbolName, loadingFailure);
        }

        Arena creationArena = Arena.ofConfined();
        try {
            MemorySegment createInfo = creationArena.allocate(s_createInfoByteSize, 8);
            createInfo.set(ValueLayout.JAVA_LONG, 0, bootstrapHandles.instanceHandle());
            createInfo.set(ValueLayout.JAVA_LONG, 8, bootstrapHandles.physicalDeviceHandle());
            createInfo.set(ValueLayout.JAVA_LONG, 16, bootstrapHandles.logicalDeviceHandle());
            createInfo.set(ValueLayout.JAVA_LONG, 24, bootstrapHandles.surfaceHandle());
            createInfo.set(ValueLayout.JAVA_LONG, 32, bootstrapHandles.graphicsQueueHandle());
            createInfo.set(ValueLayout.JAVA_LONG, 40, bootstrapHandles.presentQueueHandle());
            createInfo.set(ValueLayout.JAVA_INT, 48,
                    bootstrapHandles.graphicsQueueFamilyIndex());
            createInfo.set(ValueLayout.JAVA_INT, 52,
                    bootstrapHandles.presentQueueFamilyIndex());
            createInfo.set(ValueLayout.JAVA_INT, 56, framebufferWidth);
            createInfo.set(ValueLayout.JAVA_INT, 60, framebufferHeight);
            createInfo.set(ValueLayout.JAVA_INT, 64, framesInFlightCount);
            createInfo.set(ValueLayout.JAVA_INT, 68, 0);

            MemorySegment createResult = creationArena.allocate(s_createResultByteSize, 8);
            PresentationRuntimeOwnerFactory ownerFactory =
                    () -> new NativePresentationRuntime(libraryArena, destroyHandle, beginHandle,
                            submitHandle, presentClearHandle,
                            submitAndPresentClearFrameHandle, null, null, beginResult,
                            priorMetrics, submitResult, null,
                            createResult.get(ValueLayout.JAVA_LONG, 0),
                            createResult.get(ValueLayout.JAVA_INT, 12),
                            createResult.get(ValueLayout.JAVA_INT, 16),
                            createResult.get(ValueLayout.JAVA_INT, 20),
                            createResult.get(ValueLayout.JAVA_INT, 24));
            return invokeCreateAndCompleteRuntimeOwnershipTransfer(
                    createHandle, createInfo, createResult, libraryArena, creationArena,
                    destroyHandle, ownerFactory, s_createSymbolName);
        } catch (NativePresentationRuntimeException creationFailure) {
            closeArenaSuppressing(creationArena, creationFailure);
            closeArenaSuppressing(libraryArena, creationFailure);
            throw creationFailure;
        } catch (Throwable creationFailure) {
            closeArenaSuppressing(creationArena, creationFailure);
            closeArenaSuppressing(libraryArena, creationFailure);
            throw invocationException("createPresentationRuntime", s_createSymbolName,
                    creationFailure);
        }
    }

    /**
     * @note ThreadSafety: The returned runtime is confined to the calling thread.
     * Opens the Native FFM shared library and creates a host-image presentation runtime that
     * borrows the validated host image and Vulkan bootstrap handles.
     *
     * @param Path nativeLibraryPath Absolute path to BarriEwwNativeFfm
     * @param PresentationBootstrapHandles bootstrapHandles Borrowed Vulkan handles
     * @param int framebufferWidth Initial framebuffer width in pixels
     * @param int framebufferHeight Initial framebuffer height in pixels
     * @param int framesInFlightCount Number of in-flight frame slots
     * @param HostImagePresentationBinding hostImageBinding Borrowed host image description
     * @return NativePresentationRuntime The open thread-confined host-image runtime
     * @throws IllegalArgumentException When framebuffer dimensions or framesInFlightCount are not
     *         positive, or the host image dimensions differ from the framebuffer
     * @throws NativeLibraryLoadingException When the library or a required symbol cannot resolve
     * @throws NativePresentationRuntimeException When Native runtime creation fails
     * @warning MemoryOwnership: The returned runtime owns its confined library Arena, Native
     *          runtime and Native host presentation resources. Native borrows all caller-owned
     *          Vulkan handles and the host image until detach or close returns.
     */
    public static NativePresentationRuntime createHostImagePresentation(
            Path nativeLibraryPath, PresentationBootstrapHandles bootstrapHandles,
            int framebufferWidth, int framebufferHeight, int framesInFlightCount,
            HostImagePresentationBinding hostImageBinding)
            throws NativeLibraryLoadingException, NativePresentationRuntimeException {
        Objects.requireNonNull(nativeLibraryPath, "nativeLibraryPath");
        Objects.requireNonNull(bootstrapHandles, "bootstrapHandles");
        Objects.requireNonNull(hostImageBinding, "hostImageBinding");
        validateCreateDimensions(framebufferWidth, framebufferHeight, framesInFlightCount);
        HostImagePresentationBinding validatedBinding = HostImagePresentationBinding.create(
                hostImageBinding.hostImageHandle(), hostImageBinding.hostImageFormat(),
                hostImageBinding.requestedSurfaceFormat(), hostImageBinding.width(),
                hostImageBinding.height(), framebufferWidth, framebufferHeight);
        Path absoluteLibraryPath = nativeLibraryPath.toAbsolutePath().normalize();
        if (!nativeLibraryPath.isAbsolute()) {
            throw new NativeLibraryLoadingException(
                    "Native library path must be absolute: " + nativeLibraryPath,
                    absoluteLibraryPath, s_createHostImageSymbolName, null);
        }

        Arena libraryArena = Arena.ofConfined();
        return createHostImagePresentationWithLibraryArena(
                absoluteLibraryPath, bootstrapHandles, framebufferWidth, framebufferHeight,
                framesInFlightCount, validatedBinding, libraryArena);
    }

    /**
     * @note ThreadSafety: Initialization-confined; the calling thread owns libraryArena and the
     *       returned runtime when creation succeeds.
     * Creates a host-image runtime with an observable caller-supplied Arena for ownership tests.
     * The path and binding must already be absolute and validated by the public factory.
     *
     * @param Path absoluteLibraryPath Absolute path to BarriEwwNativeFfm
     * @param PresentationBootstrapHandles bootstrapHandles Borrowed Vulkan handles
     * @param int framebufferWidth Exact framebuffer width in pixels
     * @param int framebufferHeight Exact framebuffer height in pixels
     * @param int framesInFlightCount Number of in-flight frame slots
     * @param HostImagePresentationBinding validatedBinding Validated host-image binding
     * @param Arena libraryArena Confined Arena transferred to this creation attempt
     * @return NativePresentationRuntime Open host-image runtime on success
     * @throws NativeLibraryLoadingException When library or symbol initialization fails
     * @throws NativePresentationRuntimeException When Native runtime creation fails
     * @warning MemoryOwnership: This method consumes libraryArena. The returned runtime owns it on
     *          success; every failure closes it. Native borrows bootstrap and host image handles.
     */
    static NativePresentationRuntime createHostImagePresentationWithLibraryArena(
            Path absoluteLibraryPath, PresentationBootstrapHandles bootstrapHandles,
            int framebufferWidth, int framebufferHeight, int framesInFlightCount,
            HostImagePresentationBinding validatedBinding, Arena libraryArena)
            throws NativeLibraryLoadingException, NativePresentationRuntimeException {
        MethodHandle destroyHandle;
        MethodHandle beginHandle;
        MethodHandle submitHandle;
        MethodHandle presentClearHandle;
        MethodHandle submitAndPresentClearFrameHandle;
        MethodHandle createHostImageHandle;
        MethodHandle detachHostImageResourcesHandle;
        MethodHandle submitAndPresentHostImageFrameHandle;
        MemorySegment beginResult;
        MemorySegment priorMetrics;
        MemorySegment submitResult;
        MemorySegment detachResult;
        try {
            SymbolLookup symbolLookup = SymbolLookup.libraryLookup(
                    absoluteLibraryPath, libraryArena);
            MemorySegment[] symbols = resolveSymbolAddresses(
                    symbolLookup, absoluteLibraryPath, true);
            Linker linker = Linker.nativeLinker();
            MethodHandle[] downcallHandles = createHostImageDowncallHandles(
                    symbols, linker::downcallHandle);
            destroyHandle = downcallHandles[0];
            beginHandle = downcallHandles[1];
            submitHandle = downcallHandles[2];
            presentClearHandle = downcallHandles[3];
            submitAndPresentClearFrameHandle = downcallHandles[4];
            createHostImageHandle = downcallHandles[5];
            detachHostImageResourcesHandle = downcallHandles[6];
            submitAndPresentHostImageFrameHandle = downcallHandles[7];
            beginResult = libraryArena.allocate(s_beginResultByteSize, 8);
            priorMetrics = libraryArena.allocate(PresentationFrameMetrics.s_byteSize, 8);
            submitResult = libraryArena.allocate(s_submitResultByteSize, 4);
            detachResult = libraryArena.allocate(s_detachResultLayout);
        } catch (NativeLibraryLoadingException loadingFailure) {
            libraryArena.close();
            throw loadingFailure;
        } catch (Throwable loadingFailure) {
            libraryArena.close();
            throw new NativeLibraryLoadingException(
                    "Failed to open Native host-image presentation runtime at "
                            + absoluteLibraryPath,
                    absoluteLibraryPath, s_createHostImageSymbolName, loadingFailure);
        }

        Arena creationArena = Arena.ofConfined();
        try {
            MemorySegment createInfo = creationArena.allocate(s_hostImageCreateInfoLayout);
            writeHostImageCreateInfo(createInfo, bootstrapHandles, framebufferWidth,
                    framebufferHeight, framesInFlightCount, validatedBinding);
            MemorySegment createResult = creationArena.allocate(s_createResultByteSize, 8);
            PresentationRuntimeOwnerFactory ownerFactory =
                    () -> new NativePresentationRuntime(libraryArena, destroyHandle, beginHandle,
                            submitHandle, presentClearHandle,
                            submitAndPresentClearFrameHandle, detachHostImageResourcesHandle,
                            submitAndPresentHostImageFrameHandle, beginResult, priorMetrics,
                            submitResult, detachResult,
                            createResult.get(ValueLayout.JAVA_LONG, 0),
                            createResult.get(ValueLayout.JAVA_INT, 12),
                            createResult.get(ValueLayout.JAVA_INT, 16),
                            createResult.get(ValueLayout.JAVA_INT, 20),
                            createResult.get(ValueLayout.JAVA_INT, 24));
            return invokeCreateAndCompleteRuntimeOwnershipTransfer(
                    createHostImageHandle, createInfo, createResult, libraryArena, creationArena,
                    destroyHandle, ownerFactory, s_createHostImageSymbolName);
        } catch (NativePresentationRuntimeException creationFailure) {
            closeArenaSuppressing(creationArena, creationFailure);
            closeArenaSuppressing(libraryArena, creationFailure);
            throw creationFailure;
        } catch (Throwable creationFailure) {
            closeArenaSuppressing(creationArena, creationFailure);
            closeArenaSuppressing(libraryArena, creationFailure);
            throw invocationException("createHostImagePresentationRuntime",
                    s_createHostImageSymbolName, creationFailure);
        }
    }

    /**
     * @note ThreadSafety: Thread-confined; call serially on the render thread.
     * Waits the next frame slot, acquires a swapchain image and returns the prior frame's
     * completed metrics for that slot.
     *
     * @param int framebufferWidth Current framebuffer width; 0 signals an unavailable surface
     * @param int framebufferHeight Current framebuffer height; 0 signals an unavailable surface
     * @return PresentationBeginFrame The frame status, identity and prior-frame metrics
     * @throws NativePresentationRuntimeException When the boundary reports an invalid operation
     * @warning MemoryOwnership: Native synchronously writes reusable output segments owned by
     *          this runtime's confined library Arena and retains no address.
     */
    public PresentationBeginFrame beginFrame(int framebufferWidth, int framebufferHeight)
            throws NativePresentationRuntimeException {
        requireOpen();
        PresentationFrameStatus status = invokeBeginFrame(framebufferWidth, framebufferHeight);
        int priorMetricsValid = m_beginResult.get(ValueLayout.JAVA_INT, 12);
        Optional<PresentationFrameMetrics> priorMetricsValue = priorMetricsValid != 0
                ? Optional.of(PresentationFrameMetrics.decode(m_priorMetrics))
                : Optional.empty();
        return new PresentationBeginFrame(status,
                m_beginResult.get(ValueLayout.JAVA_INT, 4),
                m_beginResult.get(ValueLayout.JAVA_INT, 8),
                m_beginResult.get(ValueLayout.JAVA_LONG, 16),
                m_beginResult.get(ValueLayout.JAVA_LONG, 24),
                priorMetricsValue);
    }

    /**
     * @note ThreadSafety: Thread-confined; call serially on the render thread.
     * Waits the next frame slot, acquires a swapchain image and returns only its status without
     * allocating a frame result, metrics value or temporary Arena.
     *
     * @param int framebufferWidth Current framebuffer width; 0 signals an unavailable surface
     * @param int framebufferHeight Current framebuffer height; 0 signals an unavailable surface
     * @return PresentationFrameStatus The begin-frame status singleton
     * @throws NativePresentationRuntimeException When the boundary reports an invalid operation
     * @warning MemoryOwnership: Native synchronously writes reusable output segments owned by
     *          this runtime's confined library Arena and retains no address.
     */
    public PresentationFrameStatus beginFrameStatus(int framebufferWidth, int framebufferHeight)
            throws NativePresentationRuntimeException {
        requireOpen();
        return invokeBeginFrame(framebufferWidth, framebufferHeight);
    }

    /**
     * @note ThreadSafety: Thread-confined; call serially on the render thread.
     * Submits the open frame's prerecorded command buffer and presents it.
     *
     * @param long commandBufferHandle Prerecorded primary command buffer value, or 0 for none
     * @return PresentationFrameStatus The frame status after submit and present
     * @throws NativePresentationRuntimeException When the boundary reports an invalid operation
     * @warning MemoryOwnership: Native synchronously writes the reusable submit output owned by
     *          this runtime's confined library Arena and retains no address.
     */
    public PresentationFrameStatus submitAndPresentFrame(long commandBufferHandle)
            throws NativePresentationRuntimeException {
        requireOpen();
        int operationResult;
        try {
            operationResult = (int) m_submitHandle.invokeExact(m_runtimeAddress,
                    commandBufferHandle, m_submitResult);
        } catch (Throwable invocationFailure) {
            throw invocationException("submitFrame", s_submitFrameSymbolName,
                    invocationFailure);
        }
        requireSuccessfulOperation("submitFrame", s_submitFrameSymbolName, operationResult,
                m_submitResult.get(ValueLayout.JAVA_INT, 4));
        return PresentationFrameStatus.fromCode(m_submitResult.get(ValueLayout.JAVA_INT, 0));
    }

    /**
     * @note ThreadSafety: Thread-confined; call serially on the render thread.
     * Detaches borrowed host-image presentation resources. Native detach semantics are idempotent,
     * and this method allocates no per-call Java storage.
     *
     * @throws IllegalStateException When this is closed or is an ordinary presentation runtime
     * @throws NativePresentationRuntimeException When invocation or the detach operation fails
     * @warning MemoryOwnership: Native synchronously writes the runtime-owned reusable detach
     *          result and retains no address. A successful detach retires Native-owned resources
     *          that reference the caller-owned host image but never destroys that image.
     */
    public void detachHostImagePresentationResources()
            throws NativePresentationRuntimeException {
        requireOpen();
        requireHostImageRuntime();
        int operationResult;
        try {
            operationResult = (int) m_detachHostImageResourcesHandle.invokeExact(
                    m_runtimeAddress, m_detachResult);
        } catch (Throwable invocationFailure) {
            throw invocationException("detachHostImagePresentationResources",
                    s_detachHostImageResourcesSymbolName, invocationFailure);
        }
        requireSuccessfulOperation("detachHostImagePresentationResources",
                s_detachHostImageResourcesSymbolName, operationResult,
                m_detachResult.get(ValueLayout.JAVA_INT, s_detachVulkanResultOffset));
    }

    /**
     * @note ThreadSafety: Thread-confined; call serially on the render thread.
     * Submits and presents the open frame using its prerecorded host-image presentation command.
     * This valid host path performs one fixed-handle call and allocates no per-call Java storage.
     *
     * @return PresentationFrameStatus The frame status after submit and present
     * @throws IllegalStateException When this is closed or is an ordinary presentation runtime
     * @throws NativePresentationRuntimeException When invocation or the operation fails
     * @warning MemoryOwnership: Native synchronously writes the runtime-owned reusable submit
     *          result and retains no address. The host image remains caller-owned and borrowed.
     */
    public PresentationFrameStatus submitAndPresentHostImageFrame()
            throws NativePresentationRuntimeException {
        requireOpen();
        requireHostImageRuntime();
        int operationResult;
        try {
            operationResult = (int) m_submitAndPresentHostImageFrameHandle.invokeExact(
                    m_runtimeAddress, m_submitResult);
        } catch (Throwable invocationFailure) {
            throw invocationException("submitAndPresentHostImageFrame",
                    s_submitAndPresentHostImageFrameSymbolName, invocationFailure);
        }
        requireSuccessfulOperation("submitAndPresentHostImageFrame",
                s_submitAndPresentHostImageFrameSymbolName, operationResult,
                m_submitResult.get(ValueLayout.JAVA_INT, 4));
        return PresentationFrameStatus.fromCode(m_submitResult.get(ValueLayout.JAVA_INT, 0));
    }

    /**
     * @note ThreadSafety: Thread-confined; call serially on the render thread.
     * Clears, submits and presents the already-open frame and returns only its status without
     * allocating a frame result, optional value or temporary Arena.
     *
     * @param float clearRed Clear color red channel in [0, 1]
     * @param float clearGreen Clear color green channel in [0, 1]
     * @param float clearBlue Clear color blue channel in [0, 1]
     * @return PresentationFrameStatus The submit-and-present status singleton
     * @throws NativePresentationRuntimeException When the boundary reports an invalid operation
     * @warning MemoryOwnership: Native synchronously writes the reusable submit output owned by
     *          this runtime's confined library Arena and retains no address.
     */
    public PresentationFrameStatus submitAndPresentClearFrame(float clearRed, float clearGreen,
                                                              float clearBlue)
            throws NativePresentationRuntimeException {
        requireOpen();
        int operationResult;
        try {
            operationResult = (int) m_submitAndPresentClearFrameHandle.invokeExact(
                    m_runtimeAddress, clearRed, clearGreen, clearBlue, m_submitResult);
        } catch (Throwable invocationFailure) {
            throw invocationException("submitAndPresentClearFrame",
                    s_submitAndPresentClearFrameSymbolName, invocationFailure);
        }
        requireSuccessfulOperation("submitAndPresentClearFrame",
                s_submitAndPresentClearFrameSymbolName, operationResult,
                m_submitResult.get(ValueLayout.JAVA_INT, 4));
        return PresentationFrameStatus.fromCode(m_submitResult.get(ValueLayout.JAVA_INT, 0));
    }

    /**
     * @note ThreadSafety: Thread-confined; call serially on the render thread.
     * Begins a frame, clears the acquired swapchain image to the given color, submits and
     * presents it, and reports the prior frame's completed metrics (baseline visible frame).
     *
     * @param int framebufferWidth Current framebuffer width; 0 signals an unavailable surface
     * @param int framebufferHeight Current framebuffer height; 0 signals an unavailable surface
     * @param float clearRed Clear color red channel in [0, 1]
     * @param float clearGreen Clear color green channel in [0, 1]
     * @param float clearBlue Clear color blue channel in [0, 1]
     * @return PresentationClearFrame The begin/submit status, identity and prior-frame metrics
     * @throws NativePresentationRuntimeException When the boundary reports an invalid operation
     * @warning MemoryOwnership: Native synchronously writes reusable output segments owned by
     *          this runtime's confined library Arena and retains no address.
     */
    public PresentationClearFrame presentClearFrame(int framebufferWidth,
                                                    int framebufferHeight, float clearRed,
                                                    float clearGreen, float clearBlue)
            throws NativePresentationRuntimeException {
        requireOpen();
        int operationResult;
        try {
            operationResult = (int) m_presentClearHandle.invokeExact(m_runtimeAddress,
                    framebufferWidth, framebufferHeight, clearRed, clearGreen, clearBlue,
                    m_beginResult, m_priorMetrics, m_submitResult);
        } catch (Throwable invocationFailure) {
            throw invocationException("presentClearFrame", s_presentClearFrameSymbolName,
                    invocationFailure);
        }
        int submitVulkanResult = m_submitResult.get(ValueLayout.JAVA_INT, 4);
        int beginVulkanResult = m_beginResult.get(ValueLayout.JAVA_INT, 32);
        requireSuccessfulOperation("presentClearFrame", s_presentClearFrameSymbolName,
                operationResult, submitVulkanResult != 0 ? submitVulkanResult : beginVulkanResult);
        PresentationFrameStatus beginStatus = PresentationFrameStatus.fromCode(
                m_beginResult.get(ValueLayout.JAVA_INT, 0));
        PresentationFrameStatus submitStatus = PresentationFrameStatus.fromCode(
                m_submitResult.get(ValueLayout.JAVA_INT, 0));
        int priorMetricsValid = m_beginResult.get(ValueLayout.JAVA_INT, 12);
        Optional<PresentationFrameMetrics> priorMetricsValue = priorMetricsValid != 0
                ? Optional.of(PresentationFrameMetrics.decode(m_priorMetrics))
                : Optional.empty();
        return new PresentationClearFrame(beginStatus, submitStatus,
                m_beginResult.get(ValueLayout.JAVA_INT, 4),
                m_beginResult.get(ValueLayout.JAVA_INT, 8),
                m_beginResult.get(ValueLayout.JAVA_LONG, 16),
                m_beginResult.get(ValueLayout.JAVA_LONG, 24),
                priorMetricsValue);
    }

    /** The selected neutral swapchain format value. */
    public int selectedFormatValue() {
        return m_selectedFormatValue;
    }

    /** The selected Vulkan present-mode value. */
    public int selectedPresentModeValue() {
        return m_selectedPresentModeValue;
    }

    /** The selected Vulkan sharing-mode value. */
    public int selectedSharingModeValue() {
        return m_selectedSharingModeValue;
    }

    /** The number of swapchain images in the current generation. */
    public int swapchainImageCount() {
        return m_swapchainImageCount;
    }

    /**
     * @note ThreadSafety: Thread-confined; the opening thread closes this once.
     * Attempts Native destruction exactly once and closes the library Arena regardless of outcome.
     * @throws NativePresentationRuntimeException When destroy invocation or operation fails
     * @warning MemoryOwnership: The destroy boundary finalizes runtimeAddress ownership once
     * invoked; a returned failure or invocation Throwable must never retry that invalid or uncertain
     * address. This method marks the runtime closed and releases its Arena after the sole attempt.
     */
    @Override
    public void close() throws NativePresentationRuntimeException {
        if (m_isClosed) {
            return;
        }
        m_isClosed = true;
        try {
            int operationResult;
            try {
                operationResult = (int) m_destroyHandle.invokeExact(m_runtimeAddress);
            } catch (Throwable invocationFailure) {
                throw invocationException("destroyPresentationRuntime", s_destroySymbolName,
                        invocationFailure);
            }
            requireSuccessfulOperation("destroyPresentationRuntime", s_destroySymbolName,
                    operationResult, 0);
        } finally {
            if (m_libraryArena.scope().isAlive()) {
                m_libraryArena.close();
            }
        }
    }

    private void requireOpen() {
        if (m_isClosed) {
            throw new IllegalStateException("Native presentation runtime is closed");
        }
    }

    /** Rejects host-image operations on the compatibility-preserving ordinary runtime path. */
    private void requireHostImageRuntime() {
        if (m_detachHostImageResourcesHandle == null) {
            throw new IllegalStateException(
                    "Native presentation runtime was not created for host-image presentation");
        }
    }

    /**
     * @note ThreadSafety: Thread-confined; write only during host runtime creation.
     * Writes every fixed Version 1 host-image create-info field and explicitly clears reserved
     * flags.
     *
     * @param MemorySegment createInfo Writable 96-byte host-image create-info record
     * @param PresentationBootstrapHandles bootstrapHandles Borrowed Vulkan bootstrap handles
     * @param int framebufferWidth Exact framebuffer width in pixels
     * @param int framebufferHeight Exact framebuffer height in pixels
     * @param int framesInFlightCount Number of in-flight frame slots
     * @param HostImagePresentationBinding hostImageBinding Validated host-image binding
     * @warning MemoryOwnership: The caller owns createInfo and its Arena. This method writes it
     *          synchronously and retains neither the segment nor any borrowed handle.
     */
    static void writeHostImageCreateInfo(
            MemorySegment createInfo, PresentationBootstrapHandles bootstrapHandles,
            int framebufferWidth, int framebufferHeight, int framesInFlightCount,
            HostImagePresentationBinding hostImageBinding) {
        createInfo.set(ValueLayout.JAVA_LONG, s_instanceHandleOffset,
                bootstrapHandles.instanceHandle());
        createInfo.set(ValueLayout.JAVA_LONG, s_physicalDeviceHandleOffset,
                bootstrapHandles.physicalDeviceHandle());
        createInfo.set(ValueLayout.JAVA_LONG, s_logicalDeviceHandleOffset,
                bootstrapHandles.logicalDeviceHandle());
        createInfo.set(ValueLayout.JAVA_LONG, s_surfaceHandleOffset,
                bootstrapHandles.surfaceHandle());
        createInfo.set(ValueLayout.JAVA_LONG, s_graphicsQueueHandleOffset,
                bootstrapHandles.graphicsQueueHandle());
        createInfo.set(ValueLayout.JAVA_LONG, s_presentQueueHandleOffset,
                bootstrapHandles.presentQueueHandle());
        createInfo.set(ValueLayout.JAVA_INT, s_graphicsQueueFamilyIndexOffset,
                bootstrapHandles.graphicsQueueFamilyIndex());
        createInfo.set(ValueLayout.JAVA_INT, s_presentQueueFamilyIndexOffset,
                bootstrapHandles.presentQueueFamilyIndex());
        createInfo.set(ValueLayout.JAVA_INT, s_framebufferWidthOffset, framebufferWidth);
        createInfo.set(ValueLayout.JAVA_INT, s_framebufferHeightOffset, framebufferHeight);
        createInfo.set(ValueLayout.JAVA_INT, s_framesInFlightCountOffset, framesInFlightCount);
        createInfo.set(ValueLayout.JAVA_INT, s_reservedFlagsOffset, 0);
        createInfo.set(ValueLayout.JAVA_LONG, s_hostImageHandleOffset,
                hostImageBinding.hostImageHandle());
        createInfo.set(ValueLayout.JAVA_INT, s_hostImageFormatValueOffset,
                hostImageBinding.hostImageFormat().rawValue());
        createInfo.set(ValueLayout.JAVA_INT, s_requestedSurfaceFormatValueOffset,
                hostImageBinding.requestedSurfaceFormat().rawValue());
        createInfo.set(ValueLayout.JAVA_INT, s_hostImageWidthOffset, hostImageBinding.width());
        createInfo.set(ValueLayout.JAVA_INT, s_hostImageHeightOffset, hostImageBinding.height());
    }

    /**
     * @note ThreadSafety: Thread-confined; call serially on the render thread.
     * Invokes begin-frame into runtime-lifetime reusable storage and translates operation
     * failures while leaving value-shape selection to the public caller.
     *
     * @param int framebufferWidth Current framebuffer width; 0 signals an unavailable surface
     * @param int framebufferHeight Current framebuffer height; 0 signals an unavailable surface
     * @return PresentationFrameStatus The decoded begin-frame status singleton
     * @throws NativePresentationRuntimeException When invocation or the operation fails
     * @warning MemoryOwnership: Native synchronously writes runtime-owned reusable segments and
     *          retains no address; the confined library Arena releases them during close.
     */
    private PresentationFrameStatus invokeBeginFrame(int framebufferWidth, int framebufferHeight)
            throws NativePresentationRuntimeException {
        int operationResult;
        try {
            operationResult = (int) m_beginHandle.invokeExact(m_runtimeAddress,
                    framebufferWidth, framebufferHeight, m_beginResult, m_priorMetrics);
        } catch (Throwable invocationFailure) {
            throw invocationException("beginFrame", s_beginFrameSymbolName, invocationFailure);
        }
        requireSuccessfulOperation("beginFrame", s_beginFrameSymbolName, operationResult,
                m_beginResult.get(ValueLayout.JAVA_INT, 32));
        return PresentationFrameStatus.fromCode(m_beginResult.get(ValueLayout.JAVA_INT, 0));
    }

    /**
     * @note ThreadSafety: Initialization-confined; invoke once with an owner factory prepared
     *       before the Native create call.
     * Invokes one non-critical create downcall and immediately enters guarded ownership handoff
     * when Native reports success.
     *
     * @param MethodHandle createHandle Non-critical presentation create downcall handle
     * @param MemorySegment createInfo Read-only presentation create-info segment
     * @param MemorySegment createResult Writable presentation create-result segment
     * @param Arena libraryArena Confined library Arena transferred to the resulting owner
     * @param Arena creationArena Confined temporary create-record Arena
     * @param MethodHandle destroyHandle Non-critical compensating destroy handle
     * @param PresentationRuntimeOwnerFactory ownerFactory Owner factory prepared before invocation
     * @param String createSymbolName Exact Version 1 create symbol
     * @return NativePresentationRuntime Fully constructed Java owner
     * @throws NativePresentationRuntimeException When create invocation, operation or handoff fails
     * @warning MemoryOwnership: Before Native success this method owns both Arena cleanup paths.
     *          After success completeRuntimeOwnershipTransfer consumes runtimeAddress exactly once
     *          on failure or transfers it to the returned owner.
     */
    static NativePresentationRuntime invokeCreateAndCompleteRuntimeOwnershipTransfer(
            MethodHandle createHandle, MemorySegment createInfo, MemorySegment createResult,
            Arena libraryArena, Arena creationArena, MethodHandle destroyHandle,
            PresentationRuntimeOwnerFactory ownerFactory, String createSymbolName)
            throws NativePresentationRuntimeException {
        try {
            int operationResult = invokePresentationCreate(
                    createHandle, createInfo, createResult, createSymbolName);
            if (operationResult != s_operationSuccess) {
                int vulkanResult = createResult.get(ValueLayout.JAVA_INT, 8);
                throw new NativePresentationRuntimeException(
                        "Native presentation runtime creation failed with operation result "
                                + operationResult,
                        createSymbolName, operationResult, vulkanResult);
            }
            long runtimeAddress = createResult.get(ValueLayout.JAVA_LONG, 0);
            return completeRuntimeOwnershipTransfer(libraryArena, creationArena, destroyHandle,
                    runtimeAddress, ownerFactory, createSymbolName);
        } catch (NativePresentationRuntimeException creationFailure) {
            closeArenaSuppressing(creationArena, creationFailure);
            closeArenaSuppressing(libraryArena, creationFailure);
            throw creationFailure;
        } catch (Throwable creationFailure) {
            closeArenaSuppressing(creationArena, creationFailure);
            closeArenaSuppressing(libraryArena, creationFailure);
            throw invocationException("createPresentationRuntime", createSymbolName,
                    creationFailure);
        }
    }

    /**
     * @note ThreadSafety: Initialization-confined; invoke once after Native create succeeds.
     * Transfers a newly created Native runtime address to a fully constructed Java owner. If Java
     * owner construction fails, consumes the address through exactly one destroy attempt before
     * closing the library Arena.
     *
     * @param Arena libraryArena Confined library Arena to transfer or close
     * @param Arena creationArena Confined create-record Arena to close before committing handoff
     * @param MethodHandle destroyHandle Non-critical Native runtime destroy handle
     * @param long runtimeAddress Newly transferred nonzero Native runtime address
     * @param PresentationRuntimeOwnerFactory ownerFactory Java owner constructor
     * @param String createSymbolName Native create symbol responsible for this ownership transfer
     * @return NativePresentationRuntime Fully constructed Java owner
     * @throws NativePresentationRuntimeException When Java owner construction fails
     * @warning MemoryOwnership: Native ownership transfers to the returned owner only when this
     *          method returns successfully. On failure this method consumes runtimeAddress exactly
     *          once and closes libraryArena regardless of destroy outcome.
     */
    static NativePresentationRuntime completeRuntimeOwnershipTransfer(
            Arena libraryArena, Arena creationArena, MethodHandle destroyHandle,
            long runtimeAddress,
            PresentationRuntimeOwnerFactory ownerFactory, String createSymbolName)
            throws NativePresentationRuntimeException {
        try {
            NativePresentationRuntime runtime = Objects.requireNonNull(
                    ownerFactory.createRuntime(),
                    "ownerFactory returned null");
            creationArena.close();
            return runtime;
        } catch (Throwable initializationFailure) {
            try {
                int destroyOperationResult = (int) destroyHandle.invokeExact(runtimeAddress);
                if (destroyOperationResult != s_operationSuccess) {
                    initializationFailure.addSuppressed(new NativePresentationRuntimeException(
                            "Native runtime handoff destroy reported operation result "
                                    + destroyOperationResult,
                            s_destroySymbolName, destroyOperationResult, 0));
                }
            } catch (Throwable destroyFailure) {
                if (destroyFailure != initializationFailure) {
                    initializationFailure.addSuppressed(destroyFailure);
                }
            }
            closeArenaSuppressing(creationArena, initializationFailure);
            closeArenaSuppressing(libraryArena, initializationFailure);
            throw invocationException("initializePresentationRuntimeOwner", createSymbolName,
                    initializationFailure);
        }
    }

    /**
     * @note ThreadSafety: Initialization-confined; invoke once on the library Arena owner thread.
     * Invokes a non-critical presentation create downcall while retaining any Java-side failure as
     * the checked exception cause.
     *
     * @param MethodHandle createHandle Non-critical presentation create downcall handle
     * @param MemorySegment createInfo Read-only presentation create-info segment
     * @param MemorySegment createResult Writable presentation create result segment
     * @param String createSymbolName Exact Version 1 create symbol
     * @return int Stable Native operation result
     * @throws NativePresentationRuntimeException When the FFM invocation throws
     * @warning MemoryOwnership: The caller owns both segments and their creation Arena. Native
     *          borrows them synchronously and retains neither address.
     */
    static int invokePresentationCreate(MethodHandle createHandle, MemorySegment createInfo,
                                        MemorySegment createResult, String createSymbolName)
            throws NativePresentationRuntimeException {
        try {
            return (int) createHandle.invokeExact(createInfo, createResult);
        } catch (Throwable invocationFailure) {
            throw invocationException("createPresentationRuntime", createSymbolName,
                    invocationFailure);
        }
    }

    /**
     * Validates signed Java creation dimensions before they can be packed as Native uint32 values.
     *
     * @param int framebufferWidth Positive initial framebuffer width
     * @param int framebufferHeight Positive initial framebuffer height
     * @param int framesInFlightCount Positive number of in-flight frame slots
     * @throws IllegalArgumentException When any value is zero or negative
     */
    private static void validateCreateDimensions(int framebufferWidth, int framebufferHeight,
                                                 int framesInFlightCount) {
        if (framebufferWidth <= 0 || framebufferHeight <= 0 || framesInFlightCount <= 0) {
            throw new IllegalArgumentException(
                    "Framebuffer dimensions and framesInFlightCount must be positive");
        }
    }

    /**
     * Closes one Arena without replacing an already active primary failure.
     *
     * @param Arena arena Arena to close when still alive
     * @param Throwable primaryFailure Failure that must remain primary
     */
    private static void closeArenaSuppressing(Arena arena, Throwable primaryFailure) {
        try {
            if (arena.scope().isAlive()) {
                arena.close();
            }
        } catch (Throwable arenaCloseFailure) {
            if (arenaCloseFailure != primaryFailure) {
                primaryFailure.addSuppressed(arenaCloseFailure);
            }
        }
    }

    /**
     * Creates a checked failure for an exception contained on the Java side of a downcall.
     *
     * @param String operationName Semantic operation name
     * @param String symbolName Invoked Version 1 native symbol
     * @param Throwable invocationFailure Java-side downcall failure
     * @return NativePresentationRuntimeException Checked boundary failure
     */
    private static NativePresentationRuntimeException invocationException(
            String operationName, String symbolName, Throwable invocationFailure) {
        return new NativePresentationRuntimeException(
                "Native " + operationName + " invocation failed: " + invocationFailure,
                symbolName, -1, 0, invocationFailure);
    }

    /**
     * Converts a non-success operation result into a checked presentation failure.
     *
     * @param String operationName Semantic operation name
     * @param String symbolName Invoked Version 1 native symbol
     * @param int operationResult Stable native operation result
     * @param int vulkanResult Raw Vulkan result when available, otherwise 0
     * @throws NativePresentationRuntimeException When operationResult is not success
     */
    private static void requireSuccessfulOperation(String operationName, String symbolName,
                                                   int operationResult, int vulkanResult)
            throws NativePresentationRuntimeException {
        if (operationResult != s_operationSuccess) {
            throw new NativePresentationRuntimeException(
                    "Native " + operationName + " reported operation result " + operationResult,
                    symbolName, operationResult, vulkanResult);
        }
    }

    /**
     * @note ThreadSafety: Initialization-confined; invoke on the lookup Arena owner thread.
     * Creates every ordinary runtime downcall handle as explicitly non-critical by supplying no
     * linker options.
     *
     * @param MemorySegment[] symbolAddresses Fixed ordinary symbol addresses
     * @param DowncallHandleFactory downcallHandleFactory Production or observing handle factory
     * @return MethodHandle[] Ordinary handles in fixed runtime field order
     * @warning MemoryOwnership: Returned handles borrow symbolAddresses and share their lookup
     *          Arena lifetime. Neither this method nor the factory closes that Arena.
     */
    static MethodHandle[] createOrdinaryDowncallHandles(
            MemorySegment[] symbolAddresses, DowncallHandleFactory downcallHandleFactory) {
        return new MethodHandle[] {
                downcallHandleFactory.createDowncallHandle(
                        symbolAddresses[0], s_createDescriptor),
                downcallHandleFactory.createDowncallHandle(
                        symbolAddresses[1], s_destroyDescriptor),
                downcallHandleFactory.createDowncallHandle(
                        symbolAddresses[2], s_beginDescriptor),
                downcallHandleFactory.createDowncallHandle(
                        symbolAddresses[3], s_submitDescriptor),
                downcallHandleFactory.createDowncallHandle(
                        symbolAddresses[4], s_presentClearDescriptor),
                downcallHandleFactory.createDowncallHandle(
                        symbolAddresses[5], s_submitAndPresentClearFrameDescriptor)
        };
    }

    /**
     * @note ThreadSafety: Initialization-confined; invoke on the lookup Arena owner thread.
     * Creates the host runtime handles, including all three additive host-image downcalls, as
     * explicitly non-critical by supplying no linker options.
     *
     * @param MemorySegment[] symbolAddresses Fixed ordinary and host-image symbol addresses
     * @param DowncallHandleFactory downcallHandleFactory Production or observing handle factory
     * @return MethodHandle[] Host runtime handles in fixed runtime field order
     * @warning MemoryOwnership: Returned handles borrow symbolAddresses and share their lookup
     *          Arena lifetime. Neither this method nor the factory closes that Arena.
     */
    static MethodHandle[] createHostImageDowncallHandles(
            MemorySegment[] symbolAddresses, DowncallHandleFactory downcallHandleFactory) {
        return new MethodHandle[] {
                downcallHandleFactory.createDowncallHandle(
                        symbolAddresses[1], s_destroyDescriptor),
                downcallHandleFactory.createDowncallHandle(
                        symbolAddresses[2], s_beginDescriptor),
                downcallHandleFactory.createDowncallHandle(
                        symbolAddresses[3], s_submitDescriptor),
                downcallHandleFactory.createDowncallHandle(
                        symbolAddresses[4], s_presentClearDescriptor),
                downcallHandleFactory.createDowncallHandle(
                        symbolAddresses[5], s_submitAndPresentClearFrameDescriptor),
                downcallHandleFactory.createDowncallHandle(
                        symbolAddresses[6], s_createHostImageDescriptor),
                downcallHandleFactory.createDowncallHandle(
                        symbolAddresses[7], s_detachHostImageResourcesDescriptor),
                downcallHandleFactory.createDowncallHandle(
                        symbolAddresses[8], s_submitAndPresentHostImageFrameDescriptor)
        };
    }

    /**
     * @note ThreadSafety: Thread-confined; resolve only during construction on the lookup owner.
     * Resolves the fixed ordinary symbol set and, only when requested by the host factory, the
     * three additive host-image symbols.
     *
     * @param SymbolLookup symbolLookup Library lookup bound to a confined Arena
     * @param Path nativeLibraryPath Absolute path owning the symbol lookup
     * @param boolean shouldIncludeHostImageSymbols Whether to require all additive host symbols
     * @return MemorySegment[] Borrowed symbol addresses in fixed descriptor order
     * @throws NativeLibraryLoadingException When any required exact symbol is absent
     * @warning MemoryOwnership: Every returned address is owned by the Arena backing symbolLookup.
     *          The caller must create and retain handles only within that Arena's lifetime.
     */
    static MemorySegment[] resolveSymbolAddresses(
            SymbolLookup symbolLookup, Path nativeLibraryPath,
            boolean shouldIncludeHostImageSymbols) throws NativeLibraryLoadingException {
        int symbolCount = shouldIncludeHostImageSymbols ? 9 : 6;
        MemorySegment[] symbolAddresses = new MemorySegment[symbolCount];
        symbolAddresses[0] = findSymbol(symbolLookup, nativeLibraryPath, s_createSymbolName);
        symbolAddresses[1] = findSymbol(symbolLookup, nativeLibraryPath, s_destroySymbolName);
        symbolAddresses[2] = findSymbol(symbolLookup, nativeLibraryPath, s_beginFrameSymbolName);
        symbolAddresses[3] = findSymbol(symbolLookup, nativeLibraryPath, s_submitFrameSymbolName);
        symbolAddresses[4] = findSymbol(
                symbolLookup, nativeLibraryPath, s_presentClearFrameSymbolName);
        symbolAddresses[5] = findSymbol(
                symbolLookup, nativeLibraryPath, s_submitAndPresentClearFrameSymbolName);
        if (shouldIncludeHostImageSymbols) {
            symbolAddresses[6] = findSymbol(
                    symbolLookup, nativeLibraryPath, s_createHostImageSymbolName);
            symbolAddresses[7] = findSymbol(
                    symbolLookup, nativeLibraryPath, s_detachHostImageResourcesSymbolName);
            symbolAddresses[8] = findSymbol(
                    symbolLookup, nativeLibraryPath,
                    s_submitAndPresentHostImageFrameSymbolName);
        }
        return symbolAddresses;
    }

    /**
     * @note ThreadSafety: Thread-confined; resolve only during creation on the thread owning the
     *       lookup Arena.
     * Resolves one required Version 1 symbol and fails when the exact symbol is absent.
     *
     * @param SymbolLookup symbolLookup Library lookup bound to the runtime's confined Arena
     * @param Path nativeLibraryPath Absolute path owning the symbol lookup
     * @param String symbolName Exact required Native symbol name
     * @return MemorySegment Borrowed symbol address segment
     * @throws NativeLibraryLoadingException When lookup fails or the exact symbol is absent
     * @warning MemoryOwnership: The returned segment is owned by the Arena backing symbolLookup;
     *          callers must not retain it beyond that Arena or close it independently.
     */
    static MemorySegment findSymbol(SymbolLookup symbolLookup, Path nativeLibraryPath,
                                    String symbolName) throws NativeLibraryLoadingException {
        Optional<MemorySegment> symbol;
        try {
            symbol = symbolLookup.find(symbolName);
        } catch (Throwable lookupFailure) {
            throw new NativeLibraryLoadingException(
                    "Failed to resolve Native symbol " + symbolName + " from "
                            + nativeLibraryPath,
                    nativeLibraryPath, symbolName, lookupFailure);
        }
        if (symbol.isEmpty()) {
            throw new NativeLibraryLoadingException(
                    "Native symbol is absent: " + symbolName + " in " + nativeLibraryPath,
                    nativeLibraryPath, symbolName, null);
        }
        return symbol.orElseThrow();
    }
}
