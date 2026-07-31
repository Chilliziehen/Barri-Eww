package barrieww.mod;

import barrieww.core.interoperability.PresentationBootstrapHandles;
import java.util.Optional;

/**
 * @note ThreadSafety: Validation is stateless and returns immutable values.
 * Validates primitive borrowed presentation handles independently of Minecraft runtime types.
 * @warning MemoryOwnership: Validation transfers no ownership of borrowed Vulkan handle values.
 */
final class PresentationBootstrapHandlesValidation {
    /** Prevents validation utility instantiation. */
    private PresentationBootstrapHandlesValidation() {
    }

    /**
     * @note ThreadSafety: Concurrency-safe because validation is stateless and returns an immutable
     * record containing only primitive values.
     * Validates borrowed primitive handles and same-queue presentation before creating the Core value.
     *
     * @param long instanceHandle Borrowed Vulkan instance handle
     * @param long physicalDeviceHandle Borrowed Vulkan physical-device handle
     * @param long logicalDeviceHandle Borrowed Vulkan logical-device handle
     * @param long surfaceHandle Borrowed Vulkan surface handle
     * @param long graphicsQueueHandle Borrowed Vulkan graphics-queue handle
     * @param long presentQueueHandle Borrowed Vulkan present-queue handle
     * @param int graphicsQueueFamilyIndex Graphics queue-family index
     * @param int presentQueueFamilyIndex Present queue-family index
     * @param boolean isCapabilityMatching Whether capability state matches the logical device
     * @return Optional<PresentationBootstrapHandles> Exact flattened values, or empty when invalid
     * @warning MemoryOwnership: Validation transfers no ownership of the borrowed handle values.
     */
    static Optional<PresentationBootstrapHandles> validateAndCreate(
        long instanceHandle,
        long physicalDeviceHandle,
        long logicalDeviceHandle,
        long surfaceHandle,
        long graphicsQueueHandle,
        long presentQueueHandle,
        int graphicsQueueFamilyIndex,
        int presentQueueFamilyIndex,
        boolean isCapabilityMatching) {
        boolean hasAllHandles = instanceHandle != 0L
            && physicalDeviceHandle != 0L
            && logicalDeviceHandle != 0L
            && surfaceHandle != 0L
            && graphicsQueueHandle != 0L
            && presentQueueHandle != 0L;
        if (!hasAllHandles
            || graphicsQueueHandle != presentQueueHandle
            || graphicsQueueFamilyIndex != presentQueueFamilyIndex
            || !isCapabilityMatching) {
            return Optional.empty();
        }
        return Optional.of(new PresentationBootstrapHandles(
            instanceHandle,
            physicalDeviceHandle,
            logicalDeviceHandle,
            surfaceHandle,
            graphicsQueueHandle,
            presentQueueHandle,
            graphicsQueueFamilyIndex,
            presentQueueFamilyIndex));
    }
}
