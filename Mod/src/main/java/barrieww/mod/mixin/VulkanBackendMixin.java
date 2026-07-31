package barrieww.mod.mixin;

import barrieww.mod.VulkanDeviceCapabilityState;
import com.mojang.blaze3d.vulkan.VulkanBackend;
import com.mojang.blaze3d.vulkan.VulkanPhysicalDevice;
import com.mojang.blaze3d.vulkan.init.VulkanFeature;
import java.util.HashSet;
import java.util.Set;
import org.lwjgl.system.MemoryStack;
import org.lwjgl.vulkan.VK12;
import org.lwjgl.vulkan.VkDevice;
import org.lwjgl.vulkan.VkPhysicalDeviceFeatures2;
import org.lwjgl.vulkan.VkPhysicalDeviceVulkan12Features;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
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
    private static final Logger s_logger = LoggerFactory.getLogger("Barri-Eww");
    private static final VulkanFeature s_bufferDeviceAddressFeature = new VulkanFeature(
        VulkanBackend.VK12_FEATURES_STRUCT,
        "bufferDeviceAddress",
        VkPhysicalDeviceVulkan12Features.BUFFERDEVICEADDRESS);

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
        if (!containsFeatureNamed(VulkanBackend.REQUIRED_DEVICE_FEATURES, "dynamicRendering")
            || !containsFeatureNamed(incomingFeatures, "dynamicRendering")) {
            throw new IllegalStateException(
                "Minecraft Vulkan device creation no longer requires dynamicRendering");
        }

        m_isBufferDeviceAddressNegotiated = isFeatureSupported(physicalDevice);
        Set<VulkanFeature> negotiatedFeatures = new HashSet<>(incomingFeatures);
        if (m_isBufferDeviceAddressNegotiated) {
            negotiatedFeatures.add(s_bufferDeviceAddressFeature);
        }
        invocationArguments.set(2, negotiatedFeatures);
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
        VulkanDeviceCapabilityState.publish(
            logicalDeviceAddress,
            m_isBufferDeviceAddressNegotiated);
        s_logger.info(
            "Minecraft Vulkan bufferDeviceAddress capability: enabled={}, logical VkDevice=0x{}",
            m_isBufferDeviceAddressNegotiated,
            Long.toHexString(logicalDeviceAddress));
        return logicalDevice;
    }

    /**
     * @note ThreadSafety: Concurrency-safe for immutable feature sets.
     * Reports whether a Vulkan feature set contains the exact full feature name.
     *
     * @param Set<VulkanFeature> features Vulkan feature set to inspect
     * @param String featureName Exact Vulkan feature name
     * @return boolean True when the named feature is present
     */
    private static boolean containsFeatureNamed(
        Set<VulkanFeature> features,
        String featureName) {
        for (VulkanFeature feature : features) {
            if (feature.name().equals(featureName)) {
                return true;
            }
        }
        return false;
    }

    /**
     * @note ThreadSafety: Uses a thread-confined MemoryStack frame during serialized initialization.
     * Queries bufferDeviceAddress through a Vulkan 1.2 feature structure in a Features2 pNext chain.
     *
     * @param VulkanPhysicalDevice physicalDevice Minecraft physical-device wrapper to query
     * @return boolean True when the physical device supports bufferDeviceAddress
     */
    private static boolean isFeatureSupported(VulkanPhysicalDevice physicalDevice) {
        try (MemoryStack memoryStack = MemoryStack.stackPush()) {
            VkPhysicalDeviceFeatures2 physicalDeviceFeatures = VkPhysicalDeviceFeatures2
                .calloc(memoryStack)
                .sType$Default();
            s_bufferDeviceAddressFeature.struct().findOrCreateStructInPNextChain(
                physicalDeviceFeatures,
                memoryStack);
            VK12.vkGetPhysicalDeviceFeatures2(
                physicalDevice.vkPhysicalDevice(),
                physicalDeviceFeatures);
            return s_bufferDeviceAddressFeature.get(physicalDeviceFeatures);
        }
    }
}
