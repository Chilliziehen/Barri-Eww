package barrieww.mod;

import barrieww.core.interoperability.HostImagePresentationBinding;
import barrieww.core.interoperability.PresentationImageFormat;
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

    /**
     * Preserves source compatibility until Task9 replaces eager clear-readiness mixin preparation.
     * The zero generation deliberately keeps this legacy snapshot unready for host-image takeover.
     *
     * @param PresentationBootstrapHandles bootstrapHandles Borrowed bootstrap handles, or null
     * @param int framebufferWidth Framebuffer width in pixels
     * @param int framebufferHeight Framebuffer height in pixels
     * @param boolean isNativeLibraryReady Whether Native extraction completed successfully
     * @param boolean isHostColorTextureReady Whether the legacy clear-stage host texture was ready
     * @warning MemoryOwnership: Synthetic binding values are never passed to Native because the
     * zero generation fails readiness; all actual handles remain caller-owned.
     */
    public PresentationGenerationInputs(
        PresentationBootstrapHandles bootstrapHandles,
        int framebufferWidth,
        int framebufferHeight,
        boolean isNativeLibraryReady,
        boolean isHostColorTextureReady) {
        this(
            bootstrapHandles,
            framebufferWidth,
            framebufferHeight,
            isNativeLibraryReady,
            isHostColorTextureReady
                ? new HostImagePresentationBinding(
                    1L,
                    PresentationImageFormat.B8G8R8A8_UNORM,
                    PresentationImageFormat.B8G8R8A8_UNORM,
                    framebufferWidth,
                    framebufferHeight)
                : null,
            0L);
    }

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

    /** Returns this already captured immutable snapshot for source-compatible eager callers. */
    @Override
    public PresentationGenerationInputs prepare() {
        return this;
    }

    /** Returns whether Native extraction was ready in this snapshot. */
    public boolean isNativeLibraryReady() {
        return isNativeLibraryReady;
    }

    /** Returns whether the legacy eager caller supplied a host texture snapshot. */
    public boolean isHostColorTextureReady() {
        return hostImageBinding != null;
    }
}
