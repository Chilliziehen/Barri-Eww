package barrieww.mod.mixin;

import barrieww.mod.VulkanDeviceCapabilityState;
import com.mojang.blaze3d.vulkan.VulkanDevice;
import org.lwjgl.vulkan.VkDevice;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.spongepowered.asm.mixin.Final;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * @note ThreadSafety: Minecraft closes the Vulkan device on its serialized renderer lifecycle path.
 * Clears capability publication owned by the closing device before Minecraft destroys native state.
 * @warning MemoryOwnership: Minecraft owns and destroys the shadowed VkDevice; this mixin only reads
 * its borrowed address before destruction and never changes ownership.
 */
@Mixin(VulkanDevice.class)
public abstract class VulkanDeviceMixin {
    private static final Logger s_logger = LoggerFactory.getLogger("Barri-Eww");

    @Shadow(aliases = "vkDevice")
    @Final
    private VkDevice m_logicalDevice;

    /**
     * @note ThreadSafety: Called once at close HEAD on Minecraft's serialized renderer lifecycle.
     * Clears capability state only when this closing logical device still owns publication.
     *
     * @param CallbackInfo callbackInformation Mixin callback metadata
     * @warning MemoryOwnership: The borrowed VkDevice remains owned by Minecraft and valid until the
     * target close method destroys it after this callback returns.
     */
    @Inject(method = "close", at = @At("HEAD"))
    private void barrieww$clearOwnedCapability(CallbackInfo callbackInformation) {
        long logicalDeviceAddress = m_logicalDevice.address();
        if (VulkanDeviceCapabilityState.clearIfOwned(logicalDeviceAddress)) {
            s_logger.info(
                "Cleared Minecraft Vulkan capability state for VkDevice=0x{}",
                Long.toHexString(logicalDeviceAddress));
        }
    }
}
