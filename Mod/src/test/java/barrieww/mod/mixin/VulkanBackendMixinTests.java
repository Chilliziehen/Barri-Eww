package barrieww.mod.mixin;

import static org.junit.jupiter.api.Assertions.assertDoesNotThrow;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertNotSame;
import static org.junit.jupiter.api.Assertions.assertSame;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import barrieww.mod.VulkanDeviceCapabilityState;
import com.mojang.blaze3d.vulkan.init.VulkanFeature;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.List;
import java.util.Set;
import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.Test;
import org.lwjgl.vulkan.VkDevice;
import org.spongepowered.asm.mixin.injection.invoke.arg.Args;

/**
 * @note ThreadSafety: Tests mutate one mixin instance and process-wide capability state serially.
 * Verifies callback containment at Minecraft's exact logical-device creation invocation.
 */
final class VulkanBackendMixinTests {
    /** Resets process-wide capability publication after each callback test. */
    @AfterEach
    void resetCapabilityState() throws ReflectiveOperationException {
        Method resetMethod = VulkanDeviceCapabilityState.class.getDeclaredMethod("resetForTests");
        resetMethod.setAccessible(true);
        resetMethod.invoke(null);
    }

    /**
     * @note ThreadSafety: Not concurrency-safe because the callback mutates one mixin instance.
     * Verifies missing dynamicRendering is contained as unavailable readiness without an exception.
     *
     * @throws ReflectiveOperationException When the exact callback cannot be inspected
     */
    @Test
    void missingDynamicRenderingLeavesEquivalentFeatureCopyAndDisablesNegotiation()
        throws ReflectiveOperationException {
        VulkanBackendMixin backendMixin = new VulkanBackendMixin() { };
        Set<VulkanFeature> incomingFeatures = Set.of();
        MutableInvocationArguments invocationArguments = new MutableInvocationArguments(
            new Object[] {List.of(), null, incomingFeatures});
        Method negotiationMethod = VulkanBackendMixin.class.getDeclaredMethod(
            "barrieww$negotiateBufferDeviceAddress",
            Args.class);
        negotiationMethod.setAccessible(true);

        assertDoesNotThrow(() -> negotiationMethod.invoke(backendMixin, invocationArguments));

        Set<VulkanFeature> negotiatedFeatures = invocationArguments.get(2);
        assertEquals(incomingFeatures, negotiatedFeatures);
        assertNotSame(incomingFeatures, negotiatedFeatures);
        VkDevice logicalDevice = mock(VkDevice.class);
        when(logicalDevice.address()).thenReturn(41L);
        Method publicationMethod = VulkanBackendMixin.class.getDeclaredMethod(
            "barrieww$publishBufferDeviceAddressCapability",
            VkDevice.class);
        publicationMethod.setAccessible(true);
        publicationMethod.invoke(backendMixin, logicalDevice);
        assertFalse(VulkanDeviceCapabilityState.isBufferDeviceAddressEnabled(41L));
    }

    /**
     * @note ThreadSafety: Not concurrency-safe because this test mutates one mixin instance and
     * process-wide capability state.
     * Verifies the VMA seam publishes the exact logical-device address and returns the same wrapper.
     *
     * @throws ReflectiveOperationException When the callback or negotiation field cannot be inspected
     * @warning MemoryOwnership: The mocked VkDevice owns no native resource and is never destroyed.
     */
    @Test
    void createVmaSeamPublishesExactLogicalDevice() throws ReflectiveOperationException {
        VulkanBackendMixin backendMixin = new VulkanBackendMixin() { };
        Field negotiationField = VulkanBackendMixin.class.getDeclaredField(
            "m_isBufferDeviceAddressNegotiated");
        negotiationField.setAccessible(true);
        negotiationField.setBoolean(backendMixin, true);
        VkDevice logicalDevice = mock(VkDevice.class);
        when(logicalDevice.address()).thenReturn(41L);
        Method publicationMethod = VulkanBackendMixin.class.getDeclaredMethod(
            "barrieww$publishBufferDeviceAddressCapability",
            VkDevice.class);
        publicationMethod.setAccessible(true);

        Object returnedDevice = publicationMethod.invoke(backendMixin, logicalDevice);

        assertSame(logicalDevice, returnedDevice);
        assertTrue(VulkanDeviceCapabilityState.isBufferDeviceAddressEnabled(41L));
        assertFalse(VulkanDeviceCapabilityState.isBufferDeviceAddressEnabled(42L));
    }

    /** Mutable test implementation of Mixin invocation arguments. */
    private static final class MutableInvocationArguments extends Args {
        /**
         * @note ThreadSafety: Construction and mutation are confined to one test thread.
         * Creates mutable invocation arguments around the supplied values.
         *
         * @param Object[] values Initial invocation argument values
         */
        private MutableInvocationArguments(Object[] values) {
            super(values);
        }

        /**
         * @note ThreadSafety: Not concurrency-safe; tests mutate arguments from one thread.
         * Replaces one invocation argument.
         *
         * @param int argumentIndex Zero-based invocation argument index
         * @param T value Replacement argument value
         */
        @Override
        public <T> void set(int argumentIndex, T value) {
            values[argumentIndex] = value;
        }

        /**
         * @note ThreadSafety: Not concurrency-safe; tests mutate arguments from one thread.
         * Replaces all invocation arguments after validating the exact argument count.
         *
         * @param Object[] values Replacement invocation argument values
         */
        @Override
        public void setAll(Object... values) {
            if (values.length != this.values.length) {
                throw new IllegalArgumentException("Invocation argument count does not match");
            }
            System.arraycopy(values, 0, this.values, 0, values.length);
        }
    }
}
