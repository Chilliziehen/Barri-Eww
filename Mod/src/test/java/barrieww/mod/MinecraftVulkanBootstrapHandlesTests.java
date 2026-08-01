package barrieww.mod;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

import barrieww.core.interoperability.PresentationBootstrapHandles;
import java.util.Optional;
import org.junit.jupiter.api.Test;

/**
 * @note ThreadSafety: Tests invoke stateless validation with immutable primitive values.
 * Verifies validation of borrowed Minecraft Vulkan bootstrap handles without Minecraft objects.
 * @warning MemoryOwnership: Test values model borrowed handles; no Vulkan object is created or owned.
 */
final class MinecraftVulkanBootstrapHandlesTests {
    /** Verifies every required borrowed Vulkan handle must be nonzero. */
    @Test
    void zeroHandleIsRejected() {
        assertTrue(createHandles(0L, 2L, 3L, 4L, 5L, 5L, 6, 6, true).isEmpty());
        assertTrue(createHandles(1L, 0L, 3L, 4L, 5L, 5L, 6, 6, true).isEmpty());
        assertTrue(createHandles(1L, 2L, 0L, 4L, 5L, 5L, 6, 6, true).isEmpty());
        assertTrue(createHandles(1L, 2L, 3L, 0L, 5L, 5L, 6, 6, true).isEmpty());
        assertTrue(createHandles(1L, 2L, 3L, 4L, 0L, 5L, 6, 6, true).isEmpty());
        assertTrue(createHandles(1L, 2L, 3L, 4L, 5L, 0L, 6, 6, true).isEmpty());
    }

    /** Verifies the Minecraft surface must present on the exact borrowed graphics queue. */
    @Test
    void queueMismatchIsRejected() {
        assertTrue(createHandles(1L, 2L, 3L, 4L, 5L, 7L, 6, 6, true).isEmpty());
    }

    /** Verifies graphics and present queue-family indices must identify the same family. */
    @Test
    void queueFamilyMismatchIsRejected() {
        assertTrue(createHandles(1L, 2L, 3L, 4L, 5L, 5L, 6, 8, true).isEmpty());
    }

    /** Verifies handles are unavailable unless capability state matches the logical device. */
    @Test
    void capabilityMismatchIsRejected() {
        assertTrue(createHandles(1L, 2L, 3L, 4L, 5L, 5L, 6, 6, false).isEmpty());
    }

    /** Verifies valid primitive values are flattened unchanged into the Core record. */
    @Test
    void validValuesReturnExactCoreRecord() {
        PresentationBootstrapHandles expectedHandles = new PresentationBootstrapHandles(
            1L, 2L, 3L, 4L, 5L, 5L, 6, 6);

        assertEquals(
            Optional.of(expectedHandles),
            createHandles(1L, 2L, 3L, 4L, 5L, 5L, 6, 6, true));
    }

    /**
     * @note ThreadSafety: Concurrency-safe because validation is stateless.
     * Invokes primitive production validation with concise fixed test values.
     *
     * @param long instanceHandle Borrowed Vulkan instance handle
     * @param long physicalDeviceHandle Borrowed Vulkan physical-device handle
     * @param long logicalDeviceHandle Borrowed Vulkan logical-device handle
     * @param long surfaceHandle Borrowed Vulkan surface handle
     * @param long graphicsQueueHandle Borrowed Vulkan graphics-queue handle
     * @param long presentQueueHandle Borrowed Vulkan present-queue handle
     * @param int graphicsQueueFamilyIndex Graphics queue-family index
     * @param int presentQueueFamilyIndex Present queue-family index
     * @param boolean isCapabilityMatching Whether negotiated capability matches the logical device
     * @return Optional<PresentationBootstrapHandles> Validated handles or an empty result
     * @warning MemoryOwnership: All handle values remain borrowed; validation transfers no ownership.
     */
    private static Optional<PresentationBootstrapHandles> createHandles(
        long instanceHandle,
        long physicalDeviceHandle,
        long logicalDeviceHandle,
        long surfaceHandle,
        long graphicsQueueHandle,
        long presentQueueHandle,
        int graphicsQueueFamilyIndex,
        int presentQueueFamilyIndex,
        boolean isCapabilityMatching) {
        return PresentationBootstrapHandlesValidation.validateAndCreate(
            instanceHandle,
            physicalDeviceHandle,
            logicalDeviceHandle,
            surfaceHandle,
            graphicsQueueHandle,
            presentQueueHandle,
            graphicsQueueFamilyIndex,
            presentQueueFamilyIndex,
            isCapabilityMatching);
    }
}
