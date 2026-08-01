package barrieww.mod.mixin;

import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import barrieww.mod.VulkanDeviceCapabilityState;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import org.junit.jupiter.api.Test;
import org.lwjgl.vulkan.VkDevice;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * @note ThreadSafety: Tests mutate one mixin instance and process-wide lifecycle state serially.
 * Verifies ownership-aware capability clearing before Minecraft destroys its Vulkan device.
 */
final class VulkanDeviceMixinTests {
    /**
     * @note ThreadSafety: Not concurrency-safe because this test publishes and clears global state.
     * Verifies close HEAD clears capability state owned by the closing logical device.
     *
     * @throws ReflectiveOperationException When the shadow field or callback cannot be inspected
     * @warning MemoryOwnership: The mocked VkDevice owns no native resource and is never destroyed.
     */
    @Test
    void matchingClosingDeviceClearsCapabilityState() throws ReflectiveOperationException {
        VulkanDeviceCapabilityState.publish(41L, true);
        VulkanDeviceMixin deviceMixin = createDeviceMixin(41L);

        invokeCloseHook(deviceMixin);

        assertFalse(VulkanDeviceCapabilityState.isBufferDeviceAddressEnabled(41L));
    }

    /**
     * @note ThreadSafety: Not concurrency-safe because this test publishes and clears global state.
     * Verifies an older closing device cannot clear capability state owned by a replacement device.
     *
     * @throws ReflectiveOperationException When the shadow field or callback cannot be inspected
     * @warning MemoryOwnership: The mocked VkDevice owns no native resource and is never destroyed.
     */
    @Test
    void staleClosingDevicePreservesReplacementCapabilityState()
        throws ReflectiveOperationException {
        VulkanDeviceCapabilityState.publish(43L, true);
        VulkanDeviceMixin deviceMixin = createDeviceMixin(41L);
        try {
            invokeCloseHook(deviceMixin);

            assertTrue(VulkanDeviceCapabilityState.isBufferDeviceAddressEnabled(43L));
        } finally {
            VulkanDeviceCapabilityState.clearIfOwned(43L);
        }
    }

    /**
     * @note ThreadSafety: The returned mixin and mock are confined to one test thread.
     * Creates a mixin instance shadowing a logical device with the supplied address.
     *
     * @param long logicalDeviceAddress Mocked logical Vulkan device address
     * @return VulkanDeviceMixin Test-owned mixin instance
     * @throws ReflectiveOperationException When the shadow field cannot be assigned
     * @warning MemoryOwnership: The mocked VkDevice owns no native resource.
     */
    private static VulkanDeviceMixin createDeviceMixin(long logicalDeviceAddress)
        throws ReflectiveOperationException {
        VulkanDeviceMixin deviceMixin = new VulkanDeviceMixin() { };
        VkDevice logicalDevice = mock(VkDevice.class);
        when(logicalDevice.address()).thenReturn(logicalDeviceAddress);
        Field logicalDeviceField = VulkanDeviceMixin.class.getDeclaredField("m_logicalDevice");
        logicalDeviceField.setAccessible(true);
        logicalDeviceField.set(deviceMixin, logicalDevice);
        return deviceMixin;
    }

    /**
     * @note ThreadSafety: Not concurrency-safe; invokes one test-owned mixin instance.
     * Invokes the private close HEAD hook without running Minecraft destruction.
     *
     * @param VulkanDeviceMixin deviceMixin Test-owned mixin instance
     * @throws ReflectiveOperationException When the close hook cannot be invoked
     */
    private static void invokeCloseHook(VulkanDeviceMixin deviceMixin)
        throws ReflectiveOperationException {
        Method closeHook = VulkanDeviceMixin.class.getDeclaredMethod(
            "barrieww$clearOwnedCapability",
            CallbackInfo.class);
        closeHook.setAccessible(true);
        closeHook.invoke(deviceMixin, new Object[] {null});
    }
}
