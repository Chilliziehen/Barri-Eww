package barrieww.mod;

/**
 * @note ThreadSafety: Capability publication is volatile and atomic. Reads may occur concurrently
 * after device initialization and observe either the preceding complete state or the replacement.
 * Publishes the buffer-device-address negotiation result for exactly one logical Vulkan device.
 */
public final class VulkanDeviceCapabilityState {
    private static volatile DeviceCapability s_deviceCapability = new DeviceCapability(0L, false);

    /** Prevents utility-class instantiation. */
    private VulkanDeviceCapabilityState() {
    }

    /**
     * @note ThreadSafety: Concurrency-safe atomic publication. Minecraft device creation is expected
     * to invoke this once; a later publication completely replaces the earlier device state.
     * Publishes the negotiated capability for one successfully created logical Vulkan device.
     *
     * @param long logicalDeviceAddress Nonzero native address of the logical Vulkan device
     * @param boolean isBufferDeviceAddressEnabled Whether bufferDeviceAddress was negotiated
     */
    public static void publish(
        long logicalDeviceAddress,
        boolean isBufferDeviceAddressEnabled) {
        s_deviceCapability = new DeviceCapability(
            logicalDeviceAddress,
            isBufferDeviceAddressEnabled);
    }

    /**
     * @note ThreadSafety: Concurrency-safe volatile read of one immutable published value.
     * Reports success only when the supplied logical device is the published device and its
     * buffer-device-address feature was negotiated.
     *
     * @param long logicalDeviceAddress Native address of the logical Vulkan device being checked
     * @return boolean True only for the published device with successful feature negotiation
     */
    public static boolean isBufferDeviceAddressEnabled(long logicalDeviceAddress) {
        DeviceCapability deviceCapability = s_deviceCapability;
        return logicalDeviceAddress != 0L
            && deviceCapability.logicalDeviceAddress() == logicalDeviceAddress
            && deviceCapability.isBufferDeviceAddressEnabled();
    }

    /** Resets capability publication solely for package-local unit-test isolation. */
    static void resetForTests() {
        s_deviceCapability = new DeviceCapability(0L, false);
    }

    /** Immutable complete capability publication for one logical Vulkan device. */
    private record DeviceCapability(
        long logicalDeviceAddress,
        boolean isBufferDeviceAddressEnabled) {
    }
}
