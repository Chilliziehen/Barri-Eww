package barrieww.core.interoperability;

/**
 * @note ThreadSafety: Initialization-confined; invoke once on the presentation creation thread.
 * Constructs a prospective Java owner after Native has transferred one runtime address to Core.
 * @warning MemoryOwnership: The caller retains responsibility for the Native runtime address
 *          until the enclosing handoff guard returns the fully constructed owner.
 */
@FunctionalInterface
interface PresentationRuntimeOwnerFactory {
    /**
     * @note ThreadSafety: Initialization-confined; invoke exactly once during ownership handoff.
     * Constructs and returns the Java runtime owner.
     *
     * @return NativePresentationRuntime Prospective fully constructed Java owner
     * @warning MemoryOwnership: The enclosing handoff guard commits ownership only after its
     *          remaining initialization succeeds; any failure leaves cleanup with that guard.
     */
    NativePresentationRuntime createRuntime();
}
