package barrieww.mod.mixin;

import barrieww.core.interoperability.PresentationBootstrapHandles;
import barrieww.mod.MinecraftVulkanBootstrapHandles;
import com.mojang.blaze3d.vulkan.VulkanDevice;
import com.mojang.blaze3d.vulkan.VulkanGpuSurface;
import java.util.Optional;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.spongepowered.asm.mixin.Final;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * @note ThreadSafety: Minecraft constructs the surface on its serialized render initialization path.
 * Probes borrowed Vulkan handles after construction without changing surface or swapchain behavior.
 */
@Mixin(VulkanGpuSurface.class)
public abstract class VulkanGpuSurfaceMixin {
    private static final Logger s_logger = LoggerFactory.getLogger("Barri-Eww");

    @Shadow(aliases = "device")
    @Final
    private VulkanDevice m_device;

    @Shadow(aliases = "surface")
    @Final
    private long m_surface;

    /**
     * @note ThreadSafety: Called once at constructor TAIL on Minecraft's render initialization thread.
     * Logs the nonzero borrowed logical-device address when complete bootstrap extraction succeeds.
     *
     * @param VulkanDevice device Minecraft Vulkan device supplied to the surface
     * @param long windowHandle Borrowed GLFW window handle supplied to Minecraft
     * @param CallbackInfo callbackInformation Mixin callback metadata
     * @warning MemoryOwnership: The probe borrows all Minecraft Vulkan objects and owns none.
     */
    @Inject(method = "<init>", at = @At("TAIL"))
    private void barrieww$probeBorrowedBootstrapHandles(
        VulkanDevice device,
        long windowHandle,
        CallbackInfo callbackInformation) {
        Optional<PresentationBootstrapHandles> bootstrapHandles =
            MinecraftVulkanBootstrapHandles.extractBorrowedHandles(m_device, m_surface);
        if (bootstrapHandles.isPresent()) {
            s_logger.info(
                "Borrowed Minecraft Vulkan bootstrap VkDevice=0x{}",
                Long.toHexString(bootstrapHandles.orElseThrow().logicalDeviceHandle()));
        }
    }
}
