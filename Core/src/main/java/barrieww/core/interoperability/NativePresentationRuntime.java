package barrieww.core.interoperability;

import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.Linker;
import java.lang.foreign.MemorySegment;
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

    private static final int s_operationSuccess = 0;
    private static final long s_createInfoByteSize = 72;
    private static final long s_createResultByteSize = 32;
    private static final long s_beginResultByteSize = 40;
    private static final long s_submitResultByteSize = 8;

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

    private final Arena m_libraryArena;
    private final MethodHandle m_destroyHandle;
    private final MethodHandle m_beginHandle;
    private final MethodHandle m_submitHandle;
    private final MethodHandle m_presentClearHandle;
    private final MethodHandle m_submitAndPresentClearFrameHandle;
    private final MemorySegment m_beginResult;
    private final MemorySegment m_priorMetrics;
    private final MemorySegment m_submitResult;
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
     * @param MemorySegment beginResult Reusable begin-frame output storage
     * @param MemorySegment priorMetrics Reusable prior-frame metrics output storage
     * @param MemorySegment submitResult Reusable submit-frame output storage
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
                                       MemorySegment beginResult, MemorySegment priorMetrics,
                                       MemorySegment submitResult, long runtimeAddress,
                                       int selectedFormatValue, int selectedPresentModeValue,
                                       int selectedSharingModeValue, int swapchainImageCount) {
        m_libraryArena = libraryArena;
        m_destroyHandle = destroyHandle;
        m_beginHandle = beginHandle;
        m_submitHandle = submitHandle;
        m_presentClearHandle = presentClearHandle;
        m_submitAndPresentClearFrameHandle = submitAndPresentClearFrameHandle;
        m_beginResult = beginResult;
        m_priorMetrics = priorMetrics;
        m_submitResult = submitResult;
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
            Linker linker = Linker.nativeLinker();
            createHandle = linker.downcallHandle(
                    findSymbol(symbolLookup, absoluteLibraryPath, s_createSymbolName),
                    s_createDescriptor);
            destroyHandle = linker.downcallHandle(
                    findSymbol(symbolLookup, absoluteLibraryPath, s_destroySymbolName),
                    s_destroyDescriptor);
            beginHandle = linker.downcallHandle(
                    findSymbol(symbolLookup, absoluteLibraryPath, s_beginFrameSymbolName),
                    s_beginDescriptor);
            submitHandle = linker.downcallHandle(
                    findSymbol(symbolLookup, absoluteLibraryPath, s_submitFrameSymbolName),
                    s_submitDescriptor);
            presentClearHandle = linker.downcallHandle(
                    findSymbol(symbolLookup, absoluteLibraryPath, s_presentClearFrameSymbolName),
                    s_presentClearDescriptor);
            submitAndPresentClearFrameHandle = linker.downcallHandle(
                    findSymbol(symbolLookup, absoluteLibraryPath,
                            s_submitAndPresentClearFrameSymbolName),
                    s_submitAndPresentClearFrameDescriptor);
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

        try (Arena creationArena = Arena.ofConfined()) {
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
            int operationResult;
            try {
                operationResult = (int) createHandle.invokeExact(createInfo, createResult);
            } catch (Throwable invocationFailure) {
                libraryArena.close();
                throw new NativePresentationRuntimeException(
                        "Native presentation runtime creation invocation failed: "
                                + invocationFailure, s_createSymbolName, -1, 0);
            }
            if (operationResult != s_operationSuccess) {
                int vulkanResult = createResult.get(ValueLayout.JAVA_INT, 8);
                libraryArena.close();
                throw new NativePresentationRuntimeException(
                        "Native presentation runtime creation failed with operation result "
                                + operationResult, s_createSymbolName, operationResult,
                        vulkanResult);
            }
            long runtimeAddress = createResult.get(ValueLayout.JAVA_LONG, 0);
            return new NativePresentationRuntime(libraryArena, destroyHandle, beginHandle,
                    submitHandle, presentClearHandle, submitAndPresentClearFrameHandle,
                    beginResult, priorMetrics, submitResult, runtimeAddress,
                    createResult.get(ValueLayout.JAVA_INT, 12),
                    createResult.get(ValueLayout.JAVA_INT, 16),
                    createResult.get(ValueLayout.JAVA_INT, 20),
                    createResult.get(ValueLayout.JAVA_INT, 24));
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
                symbolName, -1, 0);
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
