package barrieww.mod;

/**
 * @note ThreadSafety: Render-thread-confined. Registration, callback and removal are serialized.
 * Coordinates borrowed-resource retirement before main-target resize and destruction.
 * @warning MemoryOwnership: The listener remains owner-controlled; registration only borrows it.
 */
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

    /**
     * @note ThreadSafety: Called once on the render thread immediately before GameRenderer destroys
     * its main target, after the tracker has removed this listener identity.
     * Consumes the listener owner's complete presentation runtime so no borrowed host-image resource
     * survives target destruction. Implementations must contain checked teardown failure.
     *
     * @warning MemoryOwnership: The callback ends all Native borrowing before Minecraft destroys the
     * target. A consumed Native runtime address must never be retried.
     */
    void beforeMainRenderTargetDestroy();
}
