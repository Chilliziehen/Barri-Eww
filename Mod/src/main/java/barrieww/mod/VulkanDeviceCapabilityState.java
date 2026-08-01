package barrieww.mod;

import java.util.concurrent.atomic.AtomicReference;

/**
 * @note ThreadSafety: Capability publication and owned clearing are atomic. Reads may occur concurrently
 * after device initialization and observe either the preceding complete state or the replacement.
 * Publishes the buffer-device-address negotiation result for exactly one logical Vulkan device.
 */
public final class VulkanDeviceCapabilityState {
    private static final DeviceCapability s_emptyDeviceCapability =
        new DeviceCapability(0L, false);
    private static final AtomicReference<DeviceCapability> s_deviceCapability =
        new AtomicReference<>(s_emptyDeviceCapability);

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
        s_deviceCapability.set(new DeviceCapability(
            logicalDeviceAddress,
            isBufferDeviceAddressEnabled));
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
        DeviceCapability deviceCapability = s_deviceCapability.get();
        return logicalDeviceAddress != 0L
            && deviceCapability.logicalDeviceAddress() == logicalDeviceAddress
            && deviceCapability.isBufferDeviceAddressEnabled();
    }

    /**
     * @note ThreadSafety: Concurrency-safe atomic compare-and-clear. A replacement publication cannot
     * be erased by an older device closing concurrently.
     * Clears capability state only when the supplied logical device currently owns publication.
     *
     * @param long logicalDeviceAddress Native address of the closing logical Vulkan device
     * @return boolean True when matching owned state was cleared; false for an absent or newer owner
     */
    public static boolean clearIfOwned(long logicalDeviceAddress) {
        while (true) {
            DeviceCapability deviceCapability = s_deviceCapability.get();
            if (logicalDeviceAddress == 0L
                || deviceCapability.logicalDeviceAddress() != logicalDeviceAddress) {
                return false;
            }
            if (s_deviceCapability.compareAndSet(
                deviceCapability,
                s_emptyDeviceCapability)) {
                return true;
            }
        }
    }

    /** Immutable complete capability publication for one logical Vulkan device. */
    private record DeviceCapability(
        long logicalDeviceAddress,
        boolean isBufferDeviceAddressEnabled) {
    }
}
