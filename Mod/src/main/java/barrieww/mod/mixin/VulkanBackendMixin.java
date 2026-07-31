package barrieww.mod.mixin;

import barrieww.mod.VulkanDeviceFeatureNegotiation;
import com.mojang.blaze3d.vulkan.VulkanBackend;
import com.mojang.blaze3d.vulkan.VulkanPhysicalDevice;
import com.mojang.blaze3d.vulkan.init.VulkanFeature;
import java.util.Set;
import org.lwjgl.vulkan.VkDevice;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.ModifyArg;
import org.spongepowered.asm.mixin.injection.ModifyArgs;
import org.spongepowered.asm.mixin.injection.invoke.arg.Args;

/**
 * @note ThreadSafety: Minecraft creates the Vulkan backend serially during renderer initialization.
 * Negotiates bufferDeviceAddress in the sole logical-device creation window and publishes its result.
 */
@Mixin(VulkanBackend.class)
public abstract class VulkanBackendMixin {
    private boolean m_isBufferDeviceAddressNegotiated;

    /**
     * @note ThreadSafety: Called once during serialized Minecraft Vulkan device initialization.
     * Copies the incoming feature set and adds bufferDeviceAddress only when the physical device
     * reports support, preserving Minecraft's required dynamicRendering feature unchanged.
     *
     * @param Args invocationArguments Arguments for Minecraft's private logical-device factory
     */
    @ModifyArgs(
        method = "createDevice(JLcom/mojang/blaze3d/shaders/ShaderSource;"
            + "Lcom/mojang/blaze3d/shaders/GpuDebugOptions;Ljava/lang/Runnable;)"
            + "Lcom/mojang/blaze3d/systems/GpuDevice;",
        at = @At(
            value = "INVOKE",
            target = "Lcom/mojang/blaze3d/vulkan/VulkanBackend;createDevice("
                + "Ljava/util/Collection;Lcom/mojang/blaze3d/vulkan/VulkanPhysicalDevice;"
                + "Ljava/util/Set;)Lorg/lwjgl/vulkan/VkDevice;"))
    private void barrieww$negotiateBufferDeviceAddress(Args invocationArguments) {
        VulkanPhysicalDevice physicalDevice = invocationArguments.get(1);
        Set<VulkanFeature> incomingFeatures = invocationArguments.get(2);
        VulkanDeviceFeatureNegotiation.DeviceFeatureNegotiation negotiation =
            VulkanDeviceFeatureNegotiation.negotiate(physicalDevice, incomingFeatures);
        m_isBufferDeviceAddressNegotiated = negotiation.isBufferDeviceAddressNegotiated();
        invocationArguments.set(2, negotiation.negotiatedFeatures());
    }

    /**
     * @note ThreadSafety: Called once on the serialized device-creation thread after vkCreateDevice
     * succeeds and before VMA creation begins.
     * Publishes and logs the capability result for the exact created logical device.
     *
     * @param VkDevice logicalDevice Successfully created Minecraft logical Vulkan device
     * @return VkDevice The unchanged logical device passed to Minecraft's VMA factory
     */
    @ModifyArg(
        method = "createDevice(JLcom/mojang/blaze3d/shaders/ShaderSource;"
            + "Lcom/mojang/blaze3d/shaders/GpuDebugOptions;Ljava/lang/Runnable;)"
            + "Lcom/mojang/blaze3d/systems/GpuDevice;",
        at = @At(
            value = "INVOKE",
            target = "Lcom/mojang/blaze3d/vulkan/VulkanBackend;createVma("
                + "Lorg/lwjgl/vulkan/VkDevice;)J"),
        index = 0)
    private VkDevice barrieww$publishBufferDeviceAddressCapability(VkDevice logicalDevice) {
        long logicalDeviceAddress = logicalDevice.address();
        VulkanDeviceFeatureNegotiation.publishCapability(
            logicalDeviceAddress,
            m_isBufferDeviceAddressNegotiated);
        return logicalDevice;
    }
}
