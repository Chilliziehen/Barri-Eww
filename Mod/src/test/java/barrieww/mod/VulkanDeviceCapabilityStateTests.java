package barrieww.mod;

import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertTrue;

import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.Test;

/**
 * @note ThreadSafety: Tests mutate the process-wide capability state and must not run concurrently.
 * Verifies that capability publication always describes exactly one logical Vulkan device.
 */
final class VulkanDeviceCapabilityStateTests {
    /** Resets process-wide state after each test to preserve test isolation. */
    @AfterEach
    void resetCapabilityState() {
        VulkanDeviceCapabilityState.resetForTests();
    }

    /**
     * @note ThreadSafety: Not concurrency-safe because this test replaces process-wide state.
     * Verifies that each publication replaces the preceding device and capability result.
     */
    @Test
    void publicationDescribesOnlyTheMostRecentlyCreatedLogicalDevice() {
        VulkanDeviceCapabilityState.publish(41L, true);

        assertTrue(VulkanDeviceCapabilityState.isBufferDeviceAddressEnabled(41L));
        assertFalse(VulkanDeviceCapabilityState.isBufferDeviceAddressEnabled(42L));

        VulkanDeviceCapabilityState.publish(43L, false);

        assertFalse(VulkanDeviceCapabilityState.isBufferDeviceAddressEnabled(43L));
    }
}
