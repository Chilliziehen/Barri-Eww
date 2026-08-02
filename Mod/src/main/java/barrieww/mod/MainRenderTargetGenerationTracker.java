package barrieww.mod;

import java.util.Objects;

/**
 * @note ThreadSafety: Render-thread-only. Every method must be called serially on the render thread.
 * Tracks successful current-main-target replacements and one active surface resize listener.
 * @warning MemoryOwnership: The tracker borrows the registered listener only until exact unregister;
 * target identity arguments are observed synchronously and never retained.
 */
public final class MainRenderTargetGenerationTracker {
    private static MainRenderTargetResizeListener s_resizeListener;
    private static long s_currentGeneration;

    /** Prevents instantiation of the global render-thread lifecycle tracker. */
    private MainRenderTargetGenerationTracker() {
    }

    /**
     * @note ThreadSafety: Render-thread-confined and non-reentrant.
     * Registers the sole active surface listener without replacing an existing registration.
     *
     * @param MainRenderTargetResizeListener resizeListener Listener borrowed until exact unregister
     * @throws NullPointerException When resizeListener is null
     * @throws IllegalStateException When another listener is already registered
     * @warning MemoryOwnership: The tracker borrows but does not own the listener.
     */
    public static void registerResizeListener(MainRenderTargetResizeListener resizeListener) {
        Objects.requireNonNull(resizeListener, "resizeListener");
        if (s_resizeListener != null) {
            throw new IllegalStateException("A main render target resize listener is already registered");
        }
        s_resizeListener = resizeListener;
    }

    /**
     * @note ThreadSafety: Render-thread-confined and non-reentrant.
     * Removes only the exact active listener identity, leaving mismatched registration unchanged.
     *
     * @param MainRenderTargetResizeListener resizeListener Expected active listener identity
     * @return boolean True when the exact listener was removed; false otherwise
     * @warning MemoryOwnership: Successful removal ends the tracker's borrowed listener reference.
     */
    public static boolean unregisterResizeListener(MainRenderTargetResizeListener resizeListener) {
        if (s_resizeListener != resizeListener) {
            return false;
        }
        s_resizeListener = null;
        return true;
    }

    /**
     * @note ThreadSafety: Render-thread-confined and non-reentrant.
     * Delegates the complete-resize decision to the active listener, or permits resize when absent.
     *
     * @return boolean Exact listener decision, or true when no surface listener is registered
     * @warning MemoryOwnership: The callback is synchronous and transfers no ownership.
     */
    public static boolean beforeMainRenderTargetResize() {
        MainRenderTargetResizeListener resizeListener = s_resizeListener;
        return resizeListener == null || resizeListener.beforeMainRenderTargetResize();
    }

    /**
     * @note ThreadSafety: Render-thread-confined and non-reentrant.
     * Publishes one generation only after resize TAIL identifies the current main target.
     *
     * @param Object resizedRenderTarget Target whose resize completed successfully
     * @param Object currentMainRenderTarget Current main target identity after successful resize
     * @warning MemoryOwnership: Target identities are observed synchronously and never retained.
     */
    public static void mainRenderTargetResizeSucceeded(
        Object resizedRenderTarget,
        Object currentMainRenderTarget) {
        if (resizedRenderTarget == currentMainRenderTarget && currentMainRenderTarget != null) {
            s_currentGeneration++;
        }
    }

    /** Returns the monotonically published successful main-target generation. */
    public static long currentGeneration() {
        return s_currentGeneration;
    }
}
