package barrieww.mod.mixin;

import barrieww.mod.MainRenderTargetGenerationTracker;
import net.minecraft.client.renderer.GameRenderer;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * @note ThreadSafety: Minecraft invokes complete renderer resize serially on the render thread.
 * Guards the complete renderer resize before any borrowed main-target image can be destroyed.
 * @warning MemoryOwnership: The callback owns no renderer resource and synchronously delegates the
 * borrowed-image retirement decision to the active surface listener.
 */
@Mixin(GameRenderer.class)
public class GameRendererMixin {
    /**
     * @note ThreadSafety: Render-thread-confined and non-reentrant at complete resize HEAD.
     * Cancels the entire renderer resize when Native cannot detach resources borrowing the current
     * main target, preserving both main-target and level-renderer state as one atomic operation.
     * An absent surface listener permits vanilla resize without cancellation.
     *
     * @param int framebufferWidth Requested framebuffer width in pixels
     * @param int framebufferHeight Requested framebuffer height in pixels
     * @param CallbackInfo callbackInformation Cancellable Mixin callback metadata
     * @warning MemoryOwnership: The tracker callback is synchronous and transfers no ownership.
     */
    @Inject(method = "resize(II)V", at = @At("HEAD"), cancellable = true)
    private void barrieww$beforeMainRenderTargetResize(
        int framebufferWidth,
        int framebufferHeight,
        CallbackInfo callbackInformation) {
        if (!MainRenderTargetGenerationTracker.beforeMainRenderTargetResize()) {
            callbackInformation.cancel();
        }
    }
}
