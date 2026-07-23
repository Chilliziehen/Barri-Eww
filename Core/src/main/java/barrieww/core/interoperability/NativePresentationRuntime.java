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

    private final Arena m_libraryArena;
    private final MethodHandle m_destroyHandle;
    private final MethodHandle m_beginHandle;
    private final MethodHandle m_submitHandle;
    private final long m_runtimeAddress;
    private final int m_selectedFormatValue;
    private final int m_selectedPresentModeValue;
    private final int m_selectedSharingModeValue;
    private final int m_swapchainImageCount;
    private boolean m_isClosed;

    private NativePresentationRuntime(Arena libraryArena, MethodHandle destroyHandle,
                                      MethodHandle beginHandle, MethodHandle submitHandle,
                                      long runtimeAddress, int selectedFormatValue,
                                      int selectedPresentModeValue,
                                      int selectedSharingModeValue,
                                      int swapchainImageCount) {
        m_libraryArena = libraryArena;
        m_destroyHandle = destroyHandle;
        m_beginHandle = beginHandle;
        m_submitHandle = submitHandle;
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
        try {
            SymbolLookup symbolLookup =
                    SymbolLookup.libraryLookup(absoluteLibraryPath, libraryArena);
            Linker linker = Linker.nativeLinker();
            createHandle = linker.downcallHandle(
                    findSymbol(symbolLookup, s_createSymbolName), s_createDescriptor);
            destroyHandle = linker.downcallHandle(
                    findSymbol(symbolLookup, s_destroySymbolName), s_destroyDescriptor);
            beginHandle = linker.downcallHandle(
                    findSymbol(symbolLookup, s_beginFrameSymbolName), s_beginDescriptor);
            submitHandle = linker.downcallHandle(
                    findSymbol(symbolLookup, s_submitFrameSymbolName), s_submitDescriptor);
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
                    submitHandle, runtimeAddress,
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
     */
    public PresentationBeginFrame beginFrame(int framebufferWidth, int framebufferHeight)
            throws NativePresentationRuntimeException {
        requireOpen();
        try (Arena frameArena = Arena.ofConfined()) {
            MemorySegment beginResult = frameArena.allocate(s_beginResultByteSize, 8);
            MemorySegment priorMetrics =
                    frameArena.allocate(PresentationFrameMetrics.s_byteSize, 8);
            int operationResult;
            try {
                operationResult = (int) m_beginHandle.invokeExact(m_runtimeAddress,
                        framebufferWidth, framebufferHeight, beginResult, priorMetrics);
            } catch (Throwable invocationFailure) {
                throw new NativePresentationRuntimeException(
                        "Native beginFrame invocation failed: " + invocationFailure,
                        s_beginFrameSymbolName, -1, 0);
            }
            if (operationResult != s_operationSuccess) {
                throw new NativePresentationRuntimeException(
                        "Native beginFrame reported operation result " + operationResult,
                        s_beginFrameSymbolName, operationResult, 0);
            }
            PresentationFrameStatus status = PresentationFrameStatus.fromCode(
                    beginResult.get(ValueLayout.JAVA_INT, 0));
            int priorMetricsValid = beginResult.get(ValueLayout.JAVA_INT, 12);
            Optional<PresentationFrameMetrics> priorMetricsValue = priorMetricsValid != 0
                    ? Optional.of(PresentationFrameMetrics.decode(priorMetrics))
                    : Optional.empty();
            return new PresentationBeginFrame(status,
                    beginResult.get(ValueLayout.JAVA_INT, 4),
                    beginResult.get(ValueLayout.JAVA_INT, 8),
                    beginResult.get(ValueLayout.JAVA_LONG, 16),
                    beginResult.get(ValueLayout.JAVA_LONG, 24),
                    priorMetricsValue);
        }
    }

    /**
     * @note ThreadSafety: Thread-confined; call serially on the render thread.
     * Submits the open frame's prerecorded command buffer and presents it.
     *
     * @param long commandBufferHandle Prerecorded primary command buffer value, or 0 for none
     * @return PresentationFrameStatus The frame status after submit and present
     * @throws NativePresentationRuntimeException When the boundary reports an invalid operation
     */
    public PresentationFrameStatus submitAndPresentFrame(long commandBufferHandle)
            throws NativePresentationRuntimeException {
        requireOpen();
        try (Arena frameArena = Arena.ofConfined()) {
            MemorySegment submitResult = frameArena.allocate(s_submitResultByteSize, 8);
            int operationResult;
            try {
                operationResult = (int) m_submitHandle.invokeExact(m_runtimeAddress,
                        commandBufferHandle, submitResult);
            } catch (Throwable invocationFailure) {
                throw new NativePresentationRuntimeException(
                        "Native submitFrame invocation failed: " + invocationFailure,
                        s_submitFrameSymbolName, -1, 0);
            }
            if (operationResult != s_operationSuccess) {
                throw new NativePresentationRuntimeException(
                        "Native submitFrame reported operation result " + operationResult,
                        s_submitFrameSymbolName, operationResult, 0);
            }
            return PresentationFrameStatus.fromCode(
                    submitResult.get(ValueLayout.JAVA_INT, 0));
        }
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
     * Destroys the Native runtime and closes the library Arena.
     * @warning MemoryOwnership: Releases only Native-owned objects; callers retain their
     *          Vulkan bootstrap handles.
     */
    @Override
    public void close() {
        if (m_isClosed) {
            return;
        }
        m_isClosed = true;
        try {
            int ignoredResult = (int) m_destroyHandle.invokeExact(m_runtimeAddress);
        } catch (Throwable destructionFailure) {
            // Destruction is best-effort on close; the Arena is released regardless.
        }
        if (m_libraryArena.scope().isAlive()) {
            m_libraryArena.close();
        }
    }

    private void requireOpen() {
        if (m_isClosed) {
            throw new IllegalStateException("Native presentation runtime is closed");
        }
    }

    private static MemorySegment findSymbol(SymbolLookup symbolLookup, String symbolName) {
        return symbolLookup.find(symbolName).orElseThrow(() ->
                new IllegalArgumentException("Native symbol is absent: " + symbolName));
    }
}
