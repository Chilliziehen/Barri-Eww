package barrieww.core.interoperability;

import java.util.Objects;

/**
 * @note ThreadSafety: Immutable value; safe to share across threads before runtime creation.
 * Describes the borrowed host color image and exact framebuffer extent used by host-image
 * presentation.
 *
 * @param long hostImageHandle Nonzero borrowed Vulkan image handle
 * @param PresentationImageFormat hostImageFormat Neutral format of the borrowed host image
 * @param PresentationImageFormat requestedSurfaceFormat Neutral swapchain surface format
 * @param int width Positive host image width in pixels
 * @param int height Positive host image height in pixels
 * @warning MemoryOwnership: The host owns the Vulkan image and its memory. Native borrows the
 *          image until resources are detached or the presentation runtime is closed.
 */
public record HostImagePresentationBinding(
        long hostImageHandle,
        PresentationImageFormat hostImageFormat,
        PresentationImageFormat requestedSurfaceFormat,
        int width,
        int height) {

    /**
     * Creates a validated host-image binding whose dimensions exactly match the framebuffer.
     *
     * @param long hostImageHandle Nonzero borrowed Vulkan image handle
     * @param PresentationImageFormat hostImageFormat Neutral host image format
     * @param PresentationImageFormat requestedSurfaceFormat Neutral requested surface format
     * @param int width Positive host image width in pixels
     * @param int height Positive host image height in pixels
     * @param int framebufferWidth Positive framebuffer width in pixels
     * @param int framebufferHeight Positive framebuffer height in pixels
     * @return HostImagePresentationBinding Validated immutable binding
     * @throws NullPointerException When either format is null
     * @throws IllegalArgumentException When the handle or dimensions are invalid or dimensions
     *         differ
     * @warning MemoryOwnership: The returned value borrows hostImageHandle and neither Java nor
     *          Native acquires ownership of the image or its memory.
     */
    public static HostImagePresentationBinding create(
            long hostImageHandle, PresentationImageFormat hostImageFormat,
            PresentationImageFormat requestedSurfaceFormat, int width, int height,
            int framebufferWidth, int framebufferHeight) {
        Objects.requireNonNull(hostImageFormat, "hostImageFormat");
        Objects.requireNonNull(requestedSurfaceFormat, "requestedSurfaceFormat");
        if (hostImageHandle == 0L) {
            throw new IllegalArgumentException("hostImageHandle must be nonzero");
        }
        if (width <= 0 || height <= 0 || framebufferWidth <= 0 || framebufferHeight <= 0) {
            throw new IllegalArgumentException("Host image and framebuffer dimensions must be positive");
        }
        if (width != framebufferWidth || height != framebufferHeight) {
            throw new IllegalArgumentException(
                    "Host image dimensions must exactly match framebuffer dimensions");
        }
        return new HostImagePresentationBinding(hostImageHandle, hostImageFormat,
                requestedSurfaceFormat, width, height);
    }
}
