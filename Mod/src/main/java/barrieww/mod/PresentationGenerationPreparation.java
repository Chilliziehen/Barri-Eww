package barrieww.mod;

import barrieww.core.interoperability.NativePresentationRuntimeException;

/**
 * @note ThreadSafety: Render-thread-confined. The coordinator invokes a preparation exactly once
 * after the preceding complete runtime is safely closed.
 * Prepares host-neutral inputs for one presentation generation on the configure slow path.
 * @warning MemoryOwnership: Returned bootstrap and host-image handles remain host-owned and borrowed;
 * the callback transfers no Vulkan ownership to the coordinator.
 */
@FunctionalInterface
public interface PresentationGenerationPreparation {
    /**
     * @note ThreadSafety: Called once on the owning render thread after prior runtime retirement.
     * Captures the complete readiness snapshot for the generation being configured.
     *
     * @return PresentationGenerationInputs Prepared inputs, or null when unavailable
     * @throws NativePresentationRuntimeException When checked Native preparation fails
     * @warning MemoryOwnership: Returned handles are borrowed until candidate rejection or runtime
     * close; the preparation retains ownership of no coordinator resource.
     */
    PresentationGenerationInputs prepare() throws NativePresentationRuntimeException;
}
