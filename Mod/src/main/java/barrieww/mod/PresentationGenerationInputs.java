package barrieww.mod;

import barrieww.core.interoperability.PresentationBootstrapHandles;

/**
 * @note ThreadSafety: Immutable value; safe to publish once its borrowed handles are stable.
 * Carries all host-neutral readiness and extent data needed for one configure generation.
 * @warning MemoryOwnership: Bootstrap handle values are borrowed; this record owns no Vulkan object.
 *
 * @param PresentationBootstrapHandles bootstrapHandles Validated borrowed handles, or null if absent
 * @param int framebufferWidth Framebuffer width in pixels
 * @param int framebufferHeight Framebuffer height in pixels
 * @param boolean isNativeLibraryReady Whether Native library extraction completed successfully
 * @param boolean isHostColorTextureReady Whether the host color texture is ready for interception
 */
public record PresentationGenerationInputs(
    PresentationBootstrapHandles bootstrapHandles,
    int framebufferWidth,
    int framebufferHeight,
    boolean isNativeLibraryReady,
    boolean isHostColorTextureReady) {

    /** Returns whether every configure prerequisite is valid without throwing. */
    public boolean isReady() {
        return bootstrapHandles != null
            && framebufferWidth > 0
            && framebufferHeight > 0
            && isNativeLibraryReady
            && isHostColorTextureReady;
    }
}
