package barrieww.mod;

import barrieww.core.interoperability.PresentationBootstrapHandles;
import barrieww.mod.mixin.GpuDeviceAccessor;
import com.mojang.blaze3d.systems.GpuDevice;
import com.mojang.blaze3d.systems.GpuDeviceBackend;
import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.blaze3d.vulkan.VulkanDevice;
import com.mojang.blaze3d.vulkan.VulkanQueue;
import java.util.Optional;
import org.lwjgl.vulkan.VkDevice;
import org.lwjgl.vulkan.VkPhysicalDevice;

/**
 * @note ThreadSafety: Extraction is stateless and intended for the render-thread surface
 * constructor. Returned records are immutable, but callers must obey Minecraft object lifetimes.
 * Extracts borrowed Minecraft Vulkan bootstrap handles without retaining Minecraft types in Core.
 * @warning MemoryOwnership: Minecraft owns every returned Vulkan object. The adapter transfers no
 * ownership and must not destroy or extend the lifetime of any handle.
 */
public final class MinecraftVulkanBootstrapHandles {
    /** Prevents adapter instantiation. */
    private MinecraftVulkanBootstrapHandles() {
    }

    /**
     * @note ThreadSafety: Intended for the render-thread surface constructor after RenderSystem
     * device publication. It reads immutable device identity and capability state.
     * Lazily extracts borrowed handles only when RenderSystem and the surface use the same Vulkan
     * backend and the logical-device capability publication matches.
     *
     * @param VulkanDevice surfaceDevice Vulkan device supplied to the Minecraft surface
     * @param long surfaceHandle Borrowed non-dispatchable Vulkan surface handle
     * @return Optional<PresentationBootstrapHandles> Flattened borrowed handles, or empty on mismatch
     * @warning MemoryOwnership: Minecraft retains ownership of all extracted Vulkan objects.
     */
    public static Optional<PresentationBootstrapHandles> extractBorrowedHandles(
        VulkanDevice surfaceDevice,
        long surfaceHandle) {
        GpuDevice gpuDevice;
        try {
            gpuDevice = RenderSystem.getDevice();
        } catch (IllegalStateException deviceUnavailableException) {
            return Optional.empty();
        }

        if (!(gpuDevice instanceof GpuDeviceAccessor gpuDeviceAccessor)) {
            return Optional.empty();
        }
        GpuDeviceBackend gpuDeviceBackend = gpuDeviceAccessor.barrieww$getBackend();
        if (!(gpuDeviceBackend instanceof VulkanDevice renderSystemVulkanDevice)
            || renderSystemVulkanDevice != surfaceDevice) {
            return Optional.empty();
        }

        VkDevice logicalDevice = surfaceDevice.vkDevice();
        VkPhysicalDevice physicalDevice = logicalDevice.getPhysicalDevice();
        VulkanQueue graphicsQueue = surfaceDevice.graphicsQueue();
        long logicalDeviceHandle = logicalDevice.address();
        long graphicsQueueHandle = graphicsQueue.vkQueue().address();
        int graphicsQueueFamilyIndex = graphicsQueue.queueFamilyIndex();
        return PresentationBootstrapHandlesValidation.validateAndCreate(
            surfaceDevice.instance().vkInstance().address(),
            physicalDevice.address(),
            logicalDeviceHandle,
            surfaceHandle,
            graphicsQueueHandle,
            graphicsQueueHandle,
            graphicsQueueFamilyIndex,
            graphicsQueueFamilyIndex,
            VulkanDeviceCapabilityState.isBufferDeviceAddressEnabled(logicalDeviceHandle));
    }

}
