package barrieww.mod;

import barrieww.core.interoperability.HostImagePresentationBinding;
import barrieww.core.interoperability.PresentationBootstrapHandles;

/**
 * @note ThreadSafety: Immutable value; safe to publish once its borrowed handles are stable.
 * Carries all host-neutral readiness and extent data needed for one configure generation.
 * @warning MemoryOwnership: Bootstrap and host-image handle values are borrowed; this record owns no
 * Vulkan object and may be retained only until candidate rejection or runtime close.
 *
 * @param PresentationBootstrapHandles bootstrapHandles Validated borrowed handles, or null if absent
 * @param int framebufferWidth Framebuffer width in pixels
 * @param int framebufferHeight Framebuffer height in pixels
 * @param boolean isNativeLibraryReady Whether Native library extraction completed successfully
 * @param HostImagePresentationBinding hostImageBinding Borrowed host image binding, or null
 * @param long hostTargetGeneration Monotonically captured positive main-target generation
 */
public record PresentationGenerationInputs(
    PresentationBootstrapHandles bootstrapHandles,
    int framebufferWidth,
    int framebufferHeight,
    boolean isNativeLibraryReady,
    HostImagePresentationBinding hostImageBinding,
    long hostTargetGeneration) implements PresentationGenerationPreparation {

    /** Returns whether every configure prerequisite is valid without throwing. */
    public boolean isReady() {
        return bootstrapHandles != null
            && framebufferWidth > 0
            && framebufferHeight > 0
            && isNativeLibraryReady
            && hostTargetGeneration > 0
            && hostImageBinding != null
            && hostImageBinding.hostImageHandle() != 0L
            && hostImageBinding.hostImageFormat() != null
            && hostImageBinding.requestedSurfaceFormat() != null
            && hostImageBinding.width() == framebufferWidth
            && hostImageBinding.height() == framebufferHeight;
    }

    /** Returns this already captured immutable snapshot when directly supplied as preparation. */
    @Override
    public PresentationGenerationInputs prepare() {
        return this;
    }
}
