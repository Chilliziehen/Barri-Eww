package barrieww.mod;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertSame;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.mockStatic;
import static org.mockito.Mockito.when;
import static org.mockito.Mockito.withSettings;

import barrieww.core.interoperability.PresentationBootstrapHandles;
import barrieww.mod.mixin.GpuDeviceAccessor;
import com.mojang.blaze3d.systems.GpuDevice;
import com.mojang.blaze3d.systems.GpuDeviceBackend;
import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.blaze3d.vulkan.VulkanDevice;
import com.mojang.blaze3d.vulkan.VulkanInstance;
import com.mojang.blaze3d.vulkan.VulkanQueue;
import java.util.Optional;
import net.minecraft.SharedConstants;
import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.BeforeAll;
import org.junit.jupiter.api.Test;
import org.lwjgl.vulkan.VkDevice;
import org.lwjgl.vulkan.VkInstance;
import org.lwjgl.vulkan.VkPhysicalDevice;
import org.lwjgl.vulkan.VkQueue;
import org.mockito.MockedStatic;

/**
 * @note ThreadSafety: Tests replace RenderSystem static behavior and must run serially.
 * Verifies Minecraft-dependent bootstrap extraction without invoking unsafe Vulkan operations.
 * @warning MemoryOwnership: All mocked Vulkan wrappers and handle values are test-only borrows.
 */
final class MinecraftVulkanBootstrapExtractionTests {
    /** Initializes Minecraft version metadata required by VulkanInstance static initialization. */
    @BeforeAll
    static void initializeMinecraftVersion() {
        SharedConstants.tryDetectVersion();
    }

    /** Resets process-wide capability publication after each extraction test. */
    @AfterEach
    void resetCapabilityState() {
        VulkanDeviceCapabilityState.resetForTests();
    }

    /** Verifies unavailable RenderSystem readiness returns empty without touching the surface device. */
    @Test
    void unavailableRenderSystemReturnsEmpty() {
        VulkanDevice surfaceDevice = mock(VulkanDevice.class);
        try (MockedStatic<RenderSystem> renderSystem = mockStatic(RenderSystem.class)) {
            renderSystem.when(RenderSystem::getDevice).thenThrow(
                new IllegalStateException("device unavailable"));

            assertTrue(MinecraftVulkanBootstrapHandles.extractBorrowedHandles(
                surfaceDevice,
                4L).isEmpty());
        }
    }

    /** Verifies an untransformed Minecraft device reports unavailable accessor readiness. */
    @Test
    void deviceWithoutAccessorReturnsEmpty() {
        GpuDevice gpuDevice = mock(GpuDevice.class);
        VulkanDevice surfaceDevice = mock(VulkanDevice.class);

        assertExtractionEmpty(gpuDevice, surfaceDevice);
    }

    /** Verifies a transformed Minecraft device with a non-Vulkan backend returns empty. */
    @Test
    void nonVulkanBackendReturnsEmpty() {
        GpuDevice gpuDevice = createAccessibleGpuDevice(mock(GpuDeviceBackend.class));
        VulkanDevice surfaceDevice = mock(VulkanDevice.class);

        assertExtractionEmpty(gpuDevice, surfaceDevice);
    }

    /** Verifies RenderSystem and the surface must reference the same Vulkan backend instance. */
    @Test
    void mismatchedVulkanBackendReturnsEmpty() {
        VulkanDevice renderSystemVulkanDevice = mock(VulkanDevice.class);
        VulkanDevice surfaceDevice = mock(VulkanDevice.class);
        GpuDevice gpuDevice = createAccessibleGpuDevice(renderSystemVulkanDevice);

        assertExtractionEmpty(gpuDevice, surfaceDevice);
    }

    /** Verifies valid mocked wrappers flatten exact borrowed addresses into the Core record. */
    @Test
    void matchingVulkanBackendReturnsExactFlattenedHandles() {
        VulkanDevice surfaceDevice = mockCompleteSurfaceDevice();
        GpuDevice gpuDevice = createAccessibleGpuDevice(surfaceDevice);
        VulkanDeviceCapabilityState.publish(3L, true);

        Optional<PresentationBootstrapHandles> bootstrapHandles = extract(
            gpuDevice,
            surfaceDevice);

        assertEquals(
            Optional.of(new PresentationBootstrapHandles(
                1L, 2L, 3L, 4L, 5L, 5L, 6, 6)),
            bootstrapHandles);
    }

    /** Verifies matching wrappers remain unavailable when capability state names another device. */
    @Test
    void capabilityMismatchReturnsEmpty() {
        VulkanDevice surfaceDevice = mockCompleteSurfaceDevice();
        GpuDevice gpuDevice = createAccessibleGpuDevice(surfaceDevice);
        VulkanDeviceCapabilityState.publish(7L, true);

        assertTrue(extract(gpuDevice, surfaceDevice).isEmpty());
    }

    /**
     * @note ThreadSafety: RenderSystem static mocking is confined to the invoking test thread.
     * Asserts extraction returns empty for the supplied Minecraft and surface devices.
     *
     * @param GpuDevice gpuDevice RenderSystem device supplied by the test
     * @param VulkanDevice surfaceDevice Vulkan surface device supplied by the test
     */
    private static void assertExtractionEmpty(
        GpuDevice gpuDevice,
        VulkanDevice surfaceDevice) {
        assertTrue(extract(gpuDevice, surfaceDevice).isEmpty());
    }

    /**
     * @note ThreadSafety: RenderSystem static mocking is confined to the invoking test thread.
     * Extracts handles while RenderSystem returns the supplied device.
     *
     * @param GpuDevice gpuDevice RenderSystem device supplied by the test
     * @param VulkanDevice surfaceDevice Vulkan surface device supplied by the test
     * @return Optional<PresentationBootstrapHandles> Production extraction result
     * @warning MemoryOwnership: Mocked Vulkan wrappers remain owned by the invoking test.
     */
    private static Optional<PresentationBootstrapHandles> extract(
        GpuDevice gpuDevice,
        VulkanDevice surfaceDevice) {
        try (MockedStatic<RenderSystem> renderSystem = mockStatic(RenderSystem.class)) {
            renderSystem.when(RenderSystem::getDevice).thenReturn(gpuDevice);
            return MinecraftVulkanBootstrapHandles.extractBorrowedHandles(surfaceDevice, 4L);
        }
    }

    /**
     * @note ThreadSafety: The returned mock is confined to one test thread.
     * Creates a Minecraft device mock carrying the transformed accessor interface.
     *
     * @param GpuDeviceBackend gpuDeviceBackend Backend returned through the accessor
     * @return GpuDevice Minecraft device mock implementing GpuDeviceAccessor
     */
    private static GpuDevice createAccessibleGpuDevice(GpuDeviceBackend gpuDeviceBackend) {
        GpuDevice gpuDevice = mock(
            GpuDevice.class,
            withSettings().extraInterfaces(GpuDeviceAccessor.class));
        GpuDeviceAccessor gpuDeviceAccessor = (GpuDeviceAccessor) gpuDevice;
        when(gpuDeviceAccessor.barrieww$getBackend()).thenReturn(gpuDeviceBackend);
        assertSame(gpuDeviceBackend, gpuDeviceAccessor.barrieww$getBackend());
        return gpuDevice;
    }

    /**
     * @note ThreadSafety: The returned mocks are confined to one test thread.
     * Creates a complete Vulkan device wrapper graph with fixed borrowed addresses.
     *
     * @return VulkanDevice Surface device exposing fixed instance, physical, logical, and queue values
     * @warning MemoryOwnership: Mocked wrappers own no native resource and perform no destruction.
     */
    private static VulkanDevice mockCompleteSurfaceDevice() {
        VulkanDevice surfaceDevice = mock(VulkanDevice.class);
        VulkanInstance vulkanInstance = mock(VulkanInstance.class);
        VkInstance instance = mock(VkInstance.class);
        VkPhysicalDevice physicalDevice = mock(VkPhysicalDevice.class);
        VkDevice logicalDevice = mock(VkDevice.class);
        VulkanQueue graphicsQueue = mock(VulkanQueue.class);
        VkQueue queue = mock(VkQueue.class);
        when(surfaceDevice.instance()).thenReturn(vulkanInstance);
        when(vulkanInstance.vkInstance()).thenReturn(instance);
        when(instance.address()).thenReturn(1L);
        when(surfaceDevice.vkDevice()).thenReturn(logicalDevice);
        when(logicalDevice.getPhysicalDevice()).thenReturn(physicalDevice);
        when(physicalDevice.address()).thenReturn(2L);
        when(logicalDevice.address()).thenReturn(3L);
        when(surfaceDevice.graphicsQueue()).thenReturn(graphicsQueue);
        when(graphicsQueue.vkQueue()).thenReturn(queue);
        when(queue.address()).thenReturn(5L);
        when(graphicsQueue.queueFamilyIndex()).thenReturn(6);
        return surfaceDevice;
    }
}
