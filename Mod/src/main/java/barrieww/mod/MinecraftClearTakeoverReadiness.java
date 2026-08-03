package barrieww.mod;

import com.mojang.blaze3d.GpuFormat;
import com.mojang.blaze3d.pipeline.RenderTarget;
import com.mojang.blaze3d.textures.GpuTexture;
import com.mojang.blaze3d.textures.GpuTextureView;
import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.GameRenderer;

/**
 * @note ThreadSafety: Stateless readiness checks are concurrency-safe when Minecraft texture state
 * is stable for the duration of each call. The render thread supplies that stability in production.
 * Validates only static Minecraft color-texture properties needed before D9 step 1 clear takeover,
 * without importing, reading or retaining the host image.
 * @warning MemoryOwnership: All Minecraft texture and view objects are borrowed for each call.
 */
public final class MinecraftClearTakeoverReadiness {
    private static final int s_requiredUsage = GpuTexture.USAGE_COPY_SRC
        | GpuTexture.USAGE_TEXTURE_BINDING;

    /** Prevents helper instantiation. */
    private MinecraftClearTakeoverReadiness() {
    }

    /**
     * @note ThreadSafety: Intended for the render thread while Minecraft renderer state is stable.
     * Safely resolves the current main color texture and validates static clear-takeover properties,
     * returning false when initialization is absent or incomplete.
     *
     * @return boolean True only when the current Minecraft color texture is completely ready
     * @warning MemoryOwnership: Borrows the Minecraft singleton, render target, texture and view.
     */
    public static boolean isMinecraftColorTextureReady() {
        try {
            Minecraft minecraft = Minecraft.getInstance();
            if (minecraft == null) {
                return false;
            }
            return isGameRendererColorTextureReady(minecraft.gameRenderer);
        } catch (RuntimeException initializationFailure) {
            return false;
        }
    }

    /**
     * @note ThreadSafety: Intended for the render thread while GameRenderer target state is stable.
     * Safely resolves the renderer's current main color view and returns false when target
     * initialization is absent or incomplete. The current texture extent is intentionally independent
     * of the pending Native generation extent because Minecraft resizes this target later in the frame.
     *
     * @param GameRenderer gameRenderer Borrowed Minecraft game renderer, or null when unavailable
     * @return boolean True only when the renderer's main color texture is completely ready
     * @warning MemoryOwnership: Borrows the renderer, render target, texture and view for this call.
     */
    public static boolean isGameRendererColorTextureReady(GameRenderer gameRenderer) {
        try {
            if (gameRenderer == null) {
                return false;
            }
            RenderTarget mainRenderTarget = gameRenderer.mainRenderTarget();
            if (mainRenderTarget == null) {
                return false;
            }
            GpuTextureView colorTextureView = mainRenderTarget.getColorTextureView();
            if (colorTextureView == null) {
                return false;
            }
            return isReady(
                colorTextureView.texture(),
                colorTextureView);
        } catch (RuntimeException initializationFailure) {
            return false;
        }
    }

    /**
     * @note ThreadSafety: Pure for texture state that remains immutable during the call.
     * Validates a live full view of one positive single-mip, single-layer two-dimensional color
     * texture with required copy-source and texture-binding usages. No pending generation extent is
     * accepted or compared because D9 step 1 does not import or read this texture.
     *
     * @param GpuTexture texture Borrowed host color texture, or null when absent
     * @param GpuTextureView textureView Borrowed full color texture view, or null when absent
     * @return boolean True only when every host color-texture readiness condition holds
     * @warning MemoryOwnership: The helper observes but never closes or retains either argument.
     */
    public static boolean isReady(
        GpuTexture texture,
        GpuTextureView textureView) {
        if (texture == null
            || textureView == null
            || textureView.texture() != texture
            || texture.isClosed()
            || textureView.isClosed()) {
            return false;
        }

        GpuFormat textureFormat = texture.getFormat();
        int textureUsage = texture.usage();
        return textureFormat != null
            && textureFormat.hasColorAspect()
            && texture.getWidth(0) > 0
            && texture.getHeight(0) > 0
            && textureView.getWidth(0) > 0
            && textureView.getHeight(0) > 0
            && texture.getDepthOrLayers() == 1
            && texture.getMipLevels() == 1
            && textureView.baseMipLevel() == 0
            && textureView.mipLevels() == 1
            && (textureUsage & s_requiredUsage) == s_requiredUsage;
    }
}
