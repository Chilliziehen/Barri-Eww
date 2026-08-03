package barrieww.mod;

import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertTrue;

import org.junit.jupiter.api.Test;

/**
 * @note ThreadSafety: Tests mutate the process-wide capability state and must not run concurrently.
 * Verifies that capability publication always describes exactly one logical Vulkan device.
 */
final class VulkanDeviceCapabilityStateTests {
    /**
     * @note ThreadSafety: Not concurrency-safe because this test replaces process-wide state.
     * Verifies that each publication replaces the preceding device and capability result.
     */
    @Test
    void publicationDescribesOnlyTheMostRecentlyCreatedLogicalDevice() {
        try {
            VulkanDeviceCapabilityState.publish(41L, true);

            assertTrue(VulkanDeviceCapabilityState.isBufferDeviceAddressEnabled(41L));
            assertFalse(VulkanDeviceCapabilityState.isBufferDeviceAddressEnabled(42L));

            VulkanDeviceCapabilityState.publish(43L, false);

            assertFalse(VulkanDeviceCapabilityState.isBufferDeviceAddressEnabled(41L));
            assertFalse(VulkanDeviceCapabilityState.isBufferDeviceAddressEnabled(43L));
        } finally {
            VulkanDeviceCapabilityState.clearIfOwned(43L);
        }
    }

    /** Verifies a closing device cannot clear capability state owned by another device. */
    @Test
    void wrongLogicalDeviceAddressDoesNotClearState() {
        VulkanDeviceCapabilityState.publish(41L, true);
        try {
            assertFalse(VulkanDeviceCapabilityState.clearIfOwned(42L));
            assertTrue(VulkanDeviceCapabilityState.isBufferDeviceAddressEnabled(41L));
        } finally {
            VulkanDeviceCapabilityState.clearIfOwned(41L);
        }
    }

    /** Verifies the owning logical device clears its capability state. */
    @Test
    void matchingLogicalDeviceAddressClearsState() {
        VulkanDeviceCapabilityState.publish(41L, true);

        assertTrue(VulkanDeviceCapabilityState.clearIfOwned(41L));
        assertFalse(VulkanDeviceCapabilityState.isBufferDeviceAddressEnabled(41L));
    }
}
