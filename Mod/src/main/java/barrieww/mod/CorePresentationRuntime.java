package barrieww.mod;

import barrieww.core.interoperability.NativePresentationRuntime;
import barrieww.core.interoperability.NativePresentationRuntimeException;
import barrieww.core.interoperability.PresentationFrameStatus;
import java.util.Objects;

/**
 * @note ThreadSafety: Thread-confined with the owned Core runtime; all calls must be serialized on
 * the creating render thread.
 * Adapts Core presentation operations without remapping semantic statuses or error integers.
 * @warning MemoryOwnership: This adapter exclusively owns and closes the delegated Core runtime;
 * Core owns its Native runtime and borrows the caller's Vulkan bootstrap handles.
 */
public final class CorePresentationRuntime implements PresentationRuntime {
    private final NativePresentationRuntime m_nativePresentationRuntime;

    /**
     * @note ThreadSafety: Construction and all delegated use occur on the owning render thread.
     * Creates an exclusive adapter around one open Core presentation runtime.
     *
     * @param NativePresentationRuntime nativePresentationRuntime Exclusively owned Core runtime
     * @warning MemoryOwnership: Ownership of nativePresentationRuntime transfers to this adapter.
     */
    CorePresentationRuntime(NativePresentationRuntime nativePresentationRuntime) {
        m_nativePresentationRuntime = Objects.requireNonNull(
            nativePresentationRuntime, "nativePresentationRuntime");
    }

    /**
     * @note ThreadSafety: Thread-confined; call serially on the owning render thread.
     * Delegates allocation-free acquisition and returns the exact Core semantic status.
     *
     * @param int framebufferWidth Current framebuffer width in pixels
     * @param int framebufferHeight Current framebuffer height in pixels
     * @return PresentationFrameStatus Exact Core acquisition status
     * @throws NativePresentationRuntimeException Exact checked Core operation failure
     * @warning MemoryOwnership: Delegation transfers no ownership and retains no new memory.
     */
    @Override
    public PresentationFrameStatus beginFrameStatus(
        int framebufferWidth,
        int framebufferHeight) throws NativePresentationRuntimeException {
        return m_nativePresentationRuntime.beginFrameStatus(framebufferWidth, framebufferHeight);
    }

    /**
     * @note ThreadSafety: Thread-confined; call serially on the owning render thread.
     * Delegates allocation-free clear submission and returns the exact Core semantic status.
     *
     * @param float clearRed Clear color red channel in [0, 1]
     * @param float clearGreen Clear color green channel in [0, 1]
     * @param float clearBlue Clear color blue channel in [0, 1]
     * @return PresentationFrameStatus Exact Core submit-and-present status
     * @throws NativePresentationRuntimeException Exact checked Core operation failure
     * @warning MemoryOwnership: Delegation transfers no ownership and retains no new memory.
     */
    @Override
    public PresentationFrameStatus submitAndPresentClearFrame(
        float clearRed,
        float clearGreen,
        float clearBlue) throws NativePresentationRuntimeException {
        return m_nativePresentationRuntime.submitAndPresentClearFrame(
            clearRed, clearGreen, clearBlue);
    }

    /**
     * @note ThreadSafety: Thread-confined; call serially before host image destruction.
     * Delegates host-image resource retirement without translating error context.
     *
     * @throws NativePresentationRuntimeException Exact checked Core detach failure
     * @warning MemoryOwnership: Successful delegation ends Native borrowing of the host image.
     */
    @Override
    public void detachHostImagePresentationResources()
        throws NativePresentationRuntimeException {
        m_nativePresentationRuntime.detachHostImagePresentationResources();
    }

    /**
     * @note ThreadSafety: Thread-confined; call serially for an open attached frame.
     * Delegates host-image submission and returns the exact Core semantic status.
     *
     * @return PresentationFrameStatus Exact Core submit-and-present status
     * @throws NativePresentationRuntimeException Exact checked Core operation failure
     * @warning MemoryOwnership: Delegation transfers no ownership and retains no new memory.
     */
    @Override
    public PresentationFrameStatus submitAndPresentHostImageFrame()
        throws NativePresentationRuntimeException {
        return m_nativePresentationRuntime.submitAndPresentHostImageFrame();
    }

    /**
     * @note ThreadSafety: Thread-confined; close serially on the owning render thread.
     * Delegates destruction to the exclusively owned Core runtime.
     *
     * @throws NativePresentationRuntimeException Exact checked Core destroy failure
     * @warning MemoryOwnership: Releases the owned Core and Native runtime resources.
     */
    @Override
    public void close() throws NativePresentationRuntimeException {
        m_nativePresentationRuntime.close();
    }
}
