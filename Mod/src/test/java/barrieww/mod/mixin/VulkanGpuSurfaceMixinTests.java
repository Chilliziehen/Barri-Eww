package barrieww.mod.mixin;

import static org.junit.jupiter.api.Assertions.assertDoesNotThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.mockStatic;

import barrieww.core.interoperability.PresentationBootstrapHandles;
import barrieww.mod.MinecraftVulkanBootstrapHandles;
import com.mojang.blaze3d.vulkan.VulkanDevice;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.Optional;
import org.junit.jupiter.api.Test;
import org.mockito.MockedStatic;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * @note ThreadSafety: Tests mutate one mixin instance and one scoped static mock serially.
 * Verifies the constructor-tail surface probe remains a non-destructive delegate.
 */
final class VulkanGpuSurfaceMixinTests {
    /**
     * @note ThreadSafety: Not concurrency-safe because the test replaces adapter static behavior.
     * Verifies constructor-tail probing forwards exact surface state and accepts valid borrowed data.
     *
     * @throws ReflectiveOperationException When shadow fields or the callback cannot be inspected
     * @warning MemoryOwnership: The mocked Vulkan device and primitive handles own no native resource.
     */
    @Test
    void constructorTailForwardsBorrowedSurfaceState() throws ReflectiveOperationException {
        VulkanGpuSurfaceMixin surfaceMixin = new VulkanGpuSurfaceMixin() { };
        VulkanDevice surfaceDevice = mock(VulkanDevice.class);
        setField(surfaceMixin, "m_device", surfaceDevice);
        setField(surfaceMixin, "m_surface", 4L);
        Method probeMethod = VulkanGpuSurfaceMixin.class.getDeclaredMethod(
            "barrieww$probeBorrowedBootstrapHandles",
            VulkanDevice.class,
            long.class,
            CallbackInfo.class);
        probeMethod.setAccessible(true);
        PresentationBootstrapHandles bootstrapHandles = new PresentationBootstrapHandles(
            1L, 2L, 3L, 4L, 5L, 5L, 6, 6);

        try (MockedStatic<MinecraftVulkanBootstrapHandles> adapter =
            mockStatic(MinecraftVulkanBootstrapHandles.class)) {
            adapter.when(() -> MinecraftVulkanBootstrapHandles.extractBorrowedHandles(
                surfaceDevice,
                4L)).thenReturn(Optional.of(bootstrapHandles));

            assertDoesNotThrow(() -> probeMethod.invoke(surfaceMixin, surfaceDevice, 9L, null));

            adapter.verify(() -> MinecraftVulkanBootstrapHandles.extractBorrowedHandles(
                surfaceDevice,
                4L));
        }
    }

    /**
     * @note ThreadSafety: Not concurrency-safe because reflection mutates one test-owned instance.
     * Assigns one private shadow field for callback testing.
     *
     * @param VulkanGpuSurfaceMixin surfaceMixin Test-owned mixin instance
     * @param String fieldName Exact shadow field name
     * @param Object value Replacement field value
     * @throws ReflectiveOperationException When the field cannot be assigned
     */
    private static void setField(
        VulkanGpuSurfaceMixin surfaceMixin,
        String fieldName,
        Object value) throws ReflectiveOperationException {
        Field field = VulkanGpuSurfaceMixin.class.getDeclaredField(fieldName);
        field.setAccessible(true);
        field.set(surfaceMixin, value);
    }
}
