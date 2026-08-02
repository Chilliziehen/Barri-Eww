package barrieww.mod;

import barrieww.core.interoperability.HostImagePresentationBinding;
import barrieww.core.interoperability.PresentationImageFormat;
import com.mojang.blaze3d.GpuFormat;
import com.mojang.blaze3d.pipeline.RenderTarget;
import com.mojang.blaze3d.textures.GpuTexture;
import com.mojang.blaze3d.textures.GpuTextureView;
import com.mojang.blaze3d.vulkan.VulkanGpuTexture;
import com.mojang.blaze3d.vulkan.VulkanGpuTextureView;
import java.util.Optional;
import net.minecraft.client.renderer.GameRenderer;

/**
 * @note ThreadSafety: Stateless extraction is concurrency-safe while Minecraft keeps renderer and
 * texture state stable for the duration of each call; production calls run on the render thread.
 * Extracts a strictly validated borrowed host color-image binding from Minecraft Vulkan state.
 * @warning MemoryOwnership: Minecraft owns every inspected resource and Vulkan handle. Extraction
 *          neither retains nor closes them, and the returned Core value contains borrowed scalars.
 */
public final class MinecraftHostImagePresentationBindingExtractor {
    private static final int s_requiredUsage = GpuTexture.USAGE_COPY_SRC
        | GpuTexture.USAGE_TEXTURE_BINDING;

    /** Prevents helper instantiation. */
    private MinecraftHostImagePresentationBindingExtractor() {
    }

    /**
     * @note ThreadSafety: Intended for the render thread while the renderer target generation is
     * stable. Runtime initialization failures are represented by an empty result.
     * Validates exact Minecraft Vulkan resource type, identity, lifetime, image properties,
     * requested extent, and the selected Vulkan UNORM surface format.
     *
     * @param GameRenderer gameRenderer Borrowed Minecraft game renderer, or null when unavailable
     * @param int framebufferWidth Required positive framebuffer width in pixels
     * @param int framebufferHeight Required positive framebuffer height in pixels
     * @param int selectedSurfaceFormatValue Raw Vulkan surface format selected by Minecraft
     * @return Optional<HostImagePresentationBinding> Immutable borrowed-image binding when every
     *         condition is satisfied; otherwise empty
     * @warning MemoryOwnership: The returned binding borrows the host Vulkan image handle. This
     *          method does not retain, close, or transfer ownership of any Minecraft resource.
     */
    public static Optional<HostImagePresentationBinding> extract(
        GameRenderer gameRenderer,
        int framebufferWidth,
        int framebufferHeight,
        int selectedSurfaceFormatValue) {
        try {
            PresentationImageFormat requestedSurfaceFormat = switch (selectedSurfaceFormatValue) {
                case 37 -> PresentationImageFormat.R8G8B8A8_UNORM;
                case 44 -> PresentationImageFormat.B8G8R8A8_UNORM;
                default -> null;
            };
            if (requestedSurfaceFormat == null
                || gameRenderer == null
                || framebufferWidth <= 0
                || framebufferHeight <= 0) {
                return Optional.empty();
            }

            RenderTarget mainRenderTarget = gameRenderer.mainRenderTarget();
            if (mainRenderTarget == null
                || mainRenderTarget.width != framebufferWidth
                || mainRenderTarget.height != framebufferHeight) {
                return Optional.empty();
            }

            GpuTexture colorTexture = mainRenderTarget.getColorTexture();
            GpuTextureView colorTextureView = mainRenderTarget.getColorTextureView();
            if (colorTexture == null
                || colorTextureView == null
                || colorTexture.getClass() != VulkanGpuTexture.class
                || colorTextureView.getClass() != VulkanGpuTextureView.class) {
                return Optional.empty();
            }

            VulkanGpuTexture texture = (VulkanGpuTexture) colorTexture;
            VulkanGpuTextureView textureView = (VulkanGpuTextureView) colorTextureView;
            GpuFormat textureFormat = texture.getFormat();
            int textureUsage = texture.usage();
            long hostImageHandle = texture.vkImage();
            if (textureView.texture() != texture
                || texture.isClosed()
                || textureView.isClosed()
                || textureFormat != GpuFormat.RGBA8_UNORM
                || !textureFormat.hasColorAspect()
                || (textureUsage & s_requiredUsage) != s_requiredUsage
                || texture.getWidth(0) != framebufferWidth
                || texture.getHeight(0) != framebufferHeight
                || textureView.getWidth(0) != framebufferWidth
                || textureView.getHeight(0) != framebufferHeight
                || texture.getDepthOrLayers() != 1
                || texture.getMipLevels() != 1
                || textureView.baseMipLevel() != 0
                || textureView.mipLevels() != 1
                || hostImageHandle == 0L) {
                return Optional.empty();
            }

            return Optional.of(HostImagePresentationBinding.create(
                hostImageHandle,
                PresentationImageFormat.R8G8B8A8_UNORM,
                requestedSurfaceFormat,
                framebufferWidth,
                framebufferHeight,
                framebufferWidth,
                framebufferHeight));
        } catch (RuntimeException initializationFailure) {
            return Optional.empty();
        }
    }
}
