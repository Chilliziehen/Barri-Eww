package barrieww.mod;

/**
 * @note ThreadSafety: Render-thread-confined. Registration, callback and removal are serialized.
 * Decides whether complete main-render-target resize may proceed after borrowed resources retire.
 * @warning MemoryOwnership: The listener remains owner-controlled; registration only borrows it.
 */
@FunctionalInterface
public interface MainRenderTargetResizeListener {
    /**
     * @note ThreadSafety: Called on the render thread immediately before complete target resize.
     * Attempts to retire resources that borrow the current main render target.
     *
     * @return boolean True when resize may proceed; false when it must be cancelled
     * @warning MemoryOwnership: Successful return guarantees borrowed target resources are detached;
     * failure retains the old host target and its ownership unchanged.
     */
    boolean beforeMainRenderTargetResize();
}
