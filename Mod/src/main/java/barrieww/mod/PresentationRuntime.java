package barrieww.mod;

import barrieww.core.interoperability.NativePresentationRuntimeException;
import barrieww.core.interoperability.PresentationFrameStatus;

/**
 * @note ThreadSafety: Thread-confined. Creation, frame operations and close must occur serially
 * on the render thread.
 * Defines the host-neutral allocation-free frame operations required by presentation takeover.
 * @warning MemoryOwnership: An implementation exclusively owns its Native presentation runtime;
 * callers must close it before invalidating any borrowed Vulkan bootstrap handle.
 */
public interface PresentationRuntime extends AutoCloseable {
    /**
     * @note ThreadSafety: Thread-confined; call serially on the owning render thread.
     * Acquires one frame and returns its semantic Core status without allocating a result value.
     *
     * @param int framebufferWidth Current framebuffer width in pixels
     * @param int framebufferHeight Current framebuffer height in pixels
     * @return PresentationFrameStatus Semantic acquisition status from Core
     * @throws NativePresentationRuntimeException When Core reports an operation-level failure
     * @warning MemoryOwnership: The runtime retains all reusable output storage and Native state.
     */
    PresentationFrameStatus beginFrameStatus(int framebufferWidth, int framebufferHeight)
        throws NativePresentationRuntimeException;

    /**
     * @note ThreadSafety: Thread-confined; call serially on the owning render thread.
     * Clears, submits and presents the open frame without allocating a result value.
     *
     * @param float clearRed Clear color red channel in [0, 1]
     * @param float clearGreen Clear color green channel in [0, 1]
     * @param float clearBlue Clear color blue channel in [0, 1]
     * @return PresentationFrameStatus Semantic submit-and-present status from Core
     * @throws NativePresentationRuntimeException When Core reports an operation-level failure
     * @warning MemoryOwnership: The runtime retains all reusable output storage and Native state.
     */
    PresentationFrameStatus submitAndPresentClearFrame(
        float clearRed,
        float clearGreen,
        float clearBlue) throws NativePresentationRuntimeException;

    /**
     * @note ThreadSafety: Thread-confined; call serially before host image destruction.
     * Retires all runtime-owned resources that reference the borrowed host image while preserving
     * the runtime, swapchain, open frame and clear path.
     *
     * @throws NativePresentationRuntimeException When Core cannot prove resource retirement
     * @warning MemoryOwnership: A successful return ends Native borrowing of the host image without
     * destroying the caller-owned image or memory.
     */
    void detachHostImagePresentationResources() throws NativePresentationRuntimeException;

    /**
     * @note ThreadSafety: Thread-confined; call serially for an open attached host-image frame.
     * Submits and presents the prerecorded host-image composition command.
     *
     * @return PresentationFrameStatus Semantic submit-and-present status from Core
     * @throws NativePresentationRuntimeException When Core reports an operation-level failure
     * @warning MemoryOwnership: The runtime retains all storage and continues borrowing the host image.
     */
    PresentationFrameStatus submitAndPresentHostImageFrame()
        throws NativePresentationRuntimeException;

    /**
     * @note ThreadSafety: Thread-confined; close serially on the owning render thread.
     * Releases the exclusively owned Native presentation runtime and is idempotent.
     *
     * @throws NativePresentationRuntimeException When Core reports a destroy failure
     * @warning MemoryOwnership: Releases the Native runtime but never the borrowed Vulkan handles.
     */
    @Override
    void close() throws NativePresentationRuntimeException;
}
