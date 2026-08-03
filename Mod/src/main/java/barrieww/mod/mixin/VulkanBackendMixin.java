package barrieww.mod.mixin;

import barrieww.mod.VulkanDeviceFeatureNegotiation;
import com.mojang.blaze3d.systems.GpuDevice;
import com.mojang.blaze3d.systems.GpuDeviceBackend;
import com.mojang.blaze3d.vulkan.VulkanBackend;
import com.mojang.blaze3d.vulkan.VulkanDevice;
import com.mojang.blaze3d.vulkan.VulkanPhysicalDevice;
import com.mojang.blaze3d.vulkan.init.VulkanFeature;
import java.util.Set;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.ModifyArgs;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;
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
     * @note ThreadSafety: Called once on the serialized device-creation thread only after the outer
     * Minecraft factory has successfully created VMA, VulkanDevice, and GpuDevice.
     * Publishes the capability result for the actual returned Vulkan backend logical device.
     *
     * @param CallbackInfoReturnable<GpuDevice> callbackInformation Completed factory return metadata
     */
    @Inject(
        method = "createDevice(JLcom/mojang/blaze3d/shaders/ShaderSource;"
            + "Lcom/mojang/blaze3d/shaders/GpuDebugOptions;Ljava/lang/Runnable;)"
            + "Lcom/mojang/blaze3d/systems/GpuDevice;",
        at = @At("RETURN"))
    private void barrieww$publishCreatedDeviceCapability(
        CallbackInfoReturnable<GpuDevice> callbackInformation) {
        GpuDevice gpuDevice = callbackInformation.getReturnValue();
        if (!(gpuDevice instanceof GpuDeviceAccessor gpuDeviceAccessor)) {
            return;
        }
        GpuDeviceBackend gpuDeviceBackend = gpuDeviceAccessor.barrieww$getBackend();
        if (!(gpuDeviceBackend instanceof VulkanDevice vulkanDevice)) {
            return;
        }
        VulkanDeviceFeatureNegotiation.publishCapability(
            vulkanDevice.vkDevice().address(),
            m_isBufferDeviceAddressNegotiated);
    }
}
