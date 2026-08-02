package barrieww.mod.mixin;

import barrieww.mod.MainRenderTargetGenerationTracker;
import com.mojang.blaze3d.pipeline.RenderTarget;
import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.GameRenderer;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * @note ThreadSafety: Minecraft resizes render targets serially on the render thread.
 * Publishes successful current-main-target resize generations without observing unrelated targets.
 * @warning MemoryOwnership: Minecraft retains every target; this callback retains no object identity.
 */
@Mixin(RenderTarget.class)
public class RenderTargetMixin {
    /**
     * @note ThreadSafety: Render-thread-confined at successful RenderTarget resize TAIL.
     * Publishes only when this resized target is the currently initialized renderer main target.
     * Incomplete initialization and runtime lookup failures leave the generation unchanged.
     *
     * @param int framebufferWidth Completed framebuffer width in pixels
     * @param int framebufferHeight Completed framebuffer height in pixels
     * @param CallbackInfo callbackInformation Non-cancellable Mixin callback metadata
     * @warning MemoryOwnership: All Minecraft references are borrowed only for synchronous identity
     * comparison and are never retained.
     */
    @Inject(method = "resize(II)V", at = @At("TAIL"))
    private void barrieww$publishMainRenderTargetResize(
        int framebufferWidth,
        int framebufferHeight,
        CallbackInfo callbackInformation) {
        try {
            Minecraft minecraft = Minecraft.getInstance();
            if (minecraft == null) {
                return;
            }
            GameRenderer gameRenderer = minecraft.gameRenderer;
            if (gameRenderer == null) {
                return;
            }
            MainRenderTargetGenerationTracker.mainRenderTargetResizeSucceeded(
                this,
                gameRenderer.mainRenderTarget());
        } catch (RuntimeException initializationFailure) {
            return;
        }
    }
}
