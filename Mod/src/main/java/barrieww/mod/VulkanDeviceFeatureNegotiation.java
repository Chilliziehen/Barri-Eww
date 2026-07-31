package barrieww.mod;

import com.mojang.blaze3d.vulkan.VulkanBackend;
import com.mojang.blaze3d.vulkan.VulkanPhysicalDevice;
import com.mojang.blaze3d.vulkan.init.VulkanFeature;
import java.util.HashSet;
import java.util.Set;
import java.util.function.BooleanSupplier;
import org.lwjgl.system.MemoryStack;
import org.lwjgl.vulkan.VK12;
import org.lwjgl.vulkan.VkPhysicalDeviceFeatures2;
import org.lwjgl.vulkan.VkPhysicalDeviceVulkan12Features;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * @note ThreadSafety: Minecraft invokes production negotiation serially during device creation;
 * pure negotiation uses only caller-owned immutable inputs and returns an immutable result.
 * Negotiates bufferDeviceAddress while containing optional Barri-Eww readiness failures.
 */
public final class VulkanDeviceFeatureNegotiation {
    private static final Logger s_logger = LoggerFactory.getLogger("Barri-Eww");
    private static final VulkanFeature s_bufferDeviceAddressFeature = new VulkanFeature(
        VulkanBackend.VK12_FEATURES_STRUCT,
        "bufferDeviceAddress",
        VkPhysicalDeviceVulkan12Features.BUFFERDEVICEADDRESS);

    /** Prevents utility-class instantiation. */
    private VulkanDeviceFeatureNegotiation() {
    }

    /**
     * @note ThreadSafety: Called once during serialized Minecraft Vulkan device initialization.
     * Negotiates bufferDeviceAddress for one physical device after validating Minecraft's
     * dynamicRendering readiness.
     *
     * @param VulkanPhysicalDevice physicalDevice Minecraft physical-device wrapper to query
     * @param Set<VulkanFeature> incomingFeatures Features selected by Minecraft for device creation
     * @return DeviceFeatureNegotiation Copied features and the Barri-Eww capability result
     */
    public static DeviceFeatureNegotiation negotiate(
        VulkanPhysicalDevice physicalDevice,
        Set<VulkanFeature> incomingFeatures) {
        return negotiate(
            VulkanBackend.REQUIRED_DEVICE_FEATURES,
            incomingFeatures,
            s_bufferDeviceAddressFeature,
            () -> isFeatureSupported(physicalDevice),
            s_logger);
    }

    /**
     * @note ThreadSafety: Concurrency-safe when supplied feature sets, query, and logger support
     * concurrent use. This method does not mutate caller-owned feature sets.
     * Copies incoming features, contains missing dynamicRendering readiness, and conditionally adds
     * bufferDeviceAddress when the supplied support query succeeds. RuntimeException from the
     * optional query is contained as unavailable readiness; Error is not intercepted.
     *
     * @param Set<VulkanFeature> requiredFeatures Minecraft's required device features
     * @param Set<VulkanFeature> incomingFeatures Features selected for this device creation
     * @param VulkanFeature bufferDeviceAddressFeature Feature descriptor to add when supported
     * @param BooleanSupplier featureSupportQuery Deferred physical-device feature support query
     * @param Logger logger Logger receiving one contextual readiness warning when required
     * @return DeviceFeatureNegotiation Immutable copied negotiated features and capability result
     */
    static DeviceFeatureNegotiation negotiate(
        Set<VulkanFeature> requiredFeatures,
        Set<VulkanFeature> incomingFeatures,
        VulkanFeature bufferDeviceAddressFeature,
        BooleanSupplier featureSupportQuery,
        Logger logger) {
        Set<VulkanFeature> negotiatedFeatures = new HashSet<>(incomingFeatures);
        boolean isDynamicRenderingRequired = containsFeatureNamed(
            requiredFeatures,
            "dynamicRendering");
        boolean isDynamicRenderingIncoming = containsFeatureNamed(
            incomingFeatures,
            "dynamicRendering");
        if (!isDynamicRenderingRequired || !isDynamicRenderingIncoming) {
            logger.warn(
                "Minecraft Vulkan dynamicRendering readiness is unavailable; required={}, incoming={}; "
                    + "bufferDeviceAddress negotiation disabled",
                isDynamicRenderingRequired,
                isDynamicRenderingIncoming);
            return new DeviceFeatureNegotiation(Set.copyOf(negotiatedFeatures), false);
        }

        boolean isBufferDeviceAddressNegotiated;
        try {
            isBufferDeviceAddressNegotiated = featureSupportQuery.getAsBoolean();
        } catch (RuntimeException featureSupportException) {
            logger.warn(
                "Minecraft Vulkan bufferDeviceAddress support query failed; negotiation disabled",
                featureSupportException);
            return new DeviceFeatureNegotiation(Set.copyOf(negotiatedFeatures), false);
        }
        if (isBufferDeviceAddressNegotiated) {
            negotiatedFeatures.add(bufferDeviceAddressFeature);
        }
        return new DeviceFeatureNegotiation(
            Set.copyOf(negotiatedFeatures),
            isBufferDeviceAddressNegotiated);
    }

    /**
     * @note ThreadSafety: Called once on the serialized device-creation thread after Minecraft's
     * complete Vulkan device factory returns successfully.
     * Publishes and logs the capability result for the exact initialized logical device.
     *
     * @param long logicalDeviceAddress Native address of the created logical Vulkan device
     * @param boolean isBufferDeviceAddressNegotiated Whether feature negotiation succeeded
     */
    public static void publishCapability(
        long logicalDeviceAddress,
        boolean isBufferDeviceAddressNegotiated) {
        publishCapability(
            logicalDeviceAddress,
            isBufferDeviceAddressNegotiated,
            s_logger);
    }

    /**
     * @note ThreadSafety: Capability publication is atomic; logger synchronization is delegated to
     * its implementation.
     * Publishes and logs one capability result using the supplied logger.
     *
     * @param long logicalDeviceAddress Native address of the created logical Vulkan device
     * @param boolean isBufferDeviceAddressNegotiated Whether feature negotiation succeeded
     * @param Logger logger Logger receiving the one capability result
     */
    static void publishCapability(
        long logicalDeviceAddress,
        boolean isBufferDeviceAddressNegotiated,
        Logger logger) {
        VulkanDeviceCapabilityState.publish(
            logicalDeviceAddress,
            isBufferDeviceAddressNegotiated);
        logger.info(
            "Minecraft Vulkan bufferDeviceAddress capability: enabled={}, logical VkDevice=0x{}",
            isBufferDeviceAddressNegotiated,
            Long.toHexString(logicalDeviceAddress));
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
     * @warning MemoryOwnership: MemoryStack owns the temporary Features2 structure and its pNext
     * chain until this method returns. Vulkan reads them synchronously and retains no pointer.
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

    /** Immutable feature-set and capability result for one logical-device creation attempt. */
    public record DeviceFeatureNegotiation(
        Set<VulkanFeature> negotiatedFeatures,
        boolean isBufferDeviceAddressNegotiated) {
    }
}
