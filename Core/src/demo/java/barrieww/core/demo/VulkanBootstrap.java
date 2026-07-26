package barrieww.core.demo;

import static org.lwjgl.glfw.GLFWVulkan.glfwCreateWindowSurface;
import static org.lwjgl.glfw.GLFWVulkan.glfwGetRequiredInstanceExtensions;
import static org.lwjgl.system.MemoryStack.stackPush;
import static org.lwjgl.system.MemoryUtil.NULL;
import static org.lwjgl.vulkan.KHRSurface.vkGetPhysicalDeviceSurfaceSupportKHR;
import static org.lwjgl.vulkan.KHRSwapchain.VK_KHR_SWAPCHAIN_EXTENSION_NAME;
import static org.lwjgl.vulkan.VK10.VK_QUEUE_GRAPHICS_BIT;
import static org.lwjgl.vulkan.VK10.VK_STRUCTURE_TYPE_APPLICATION_INFO;
import static org.lwjgl.vulkan.VK10.VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
import static org.lwjgl.vulkan.VK10.VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
import static org.lwjgl.vulkan.VK10.VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
import static org.lwjgl.vulkan.VK10.VK_SUCCESS;
import static org.lwjgl.vulkan.VK10.VK_TRUE;
import static org.lwjgl.vulkan.VK10.vkCreateDevice;
import static org.lwjgl.vulkan.VK10.vkCreateInstance;
import static org.lwjgl.vulkan.VK10.vkDestroyDevice;
import static org.lwjgl.vulkan.VK10.vkDestroyInstance;
import static org.lwjgl.vulkan.VK10.vkEnumeratePhysicalDevices;
import static org.lwjgl.vulkan.VK10.vkGetDeviceQueue;
import static org.lwjgl.vulkan.VK10.vkGetPhysicalDeviceQueueFamilyProperties;
import static org.lwjgl.vulkan.VK13.VK_API_VERSION_1_3;

import barrieww.core.interoperability.PresentationBootstrapHandles;
import java.nio.IntBuffer;
import java.nio.LongBuffer;
import org.lwjgl.PointerBuffer;
import org.lwjgl.system.MemoryStack;
import org.lwjgl.vulkan.KHRSurface;
import org.lwjgl.vulkan.VkApplicationInfo;
import org.lwjgl.vulkan.VkDevice;
import org.lwjgl.vulkan.VkDeviceCreateInfo;
import org.lwjgl.vulkan.VkDeviceQueueCreateInfo;
import org.lwjgl.vulkan.VkInstance;
import org.lwjgl.vulkan.VkInstanceCreateInfo;
import org.lwjgl.vulkan.VkPhysicalDevice;
import org.lwjgl.vulkan.VkQueue;
import org.lwjgl.vulkan.VkQueueFamilyProperties;

/**
 * @note ThreadSafety: Single-threaded; created and destroyed on the demo main thread.
 * Java-owned Vulkan bootstrap (ADR-0004): creates the instance, surface, device and one
 * graphics+present queue for a GLFW window, and exposes the borrowed handle values for the
 * Native presentation runtime. Native borrows these; Java destroys them in reverse order.
 */
public final class VulkanBootstrap implements AutoCloseable {
    private final VkInstance m_instance;
    private final VkDevice m_logicalDevice;
    private final long m_surface;
    private final PresentationBootstrapHandles m_handles;

    private VulkanBootstrap(VkInstance instance, VkDevice logicalDevice, long surface,
                            PresentationBootstrapHandles handles) {
        m_instance = instance;
        m_logicalDevice = logicalDevice;
        m_surface = surface;
        m_handles = handles;
    }

    /** The borrowed Vulkan handle values for the Native presentation runtime. */
    public PresentationBootstrapHandles handles() {
        return m_handles;
    }

    /**
     * Creates the instance, surface, device and queue for the given GLFW window.
     *
     * @param long windowHandle The GLFW window handle
     * @return VulkanBootstrap The created bootstrap owning the Vulkan objects
     */
    public static VulkanBootstrap create(long windowHandle) {
        try (MemoryStack stack = stackPush()) {
            VkApplicationInfo applicationInfo = VkApplicationInfo.calloc(stack)
                    .sType(VK_STRUCTURE_TYPE_APPLICATION_INFO)
                    .apiVersion(VK_API_VERSION_1_3);

            PointerBuffer requiredExtensions = glfwGetRequiredInstanceExtensions();
            if (requiredExtensions == null) {
                throw new IllegalStateException(
                        "GLFW reported no required Vulkan instance extensions");
            }
            VkInstanceCreateInfo instanceCreateInfo = VkInstanceCreateInfo.calloc(stack)
                    .sType(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO)
                    .pApplicationInfo(applicationInfo)
                    .ppEnabledExtensionNames(requiredExtensions);

            PointerBuffer instancePointer = stack.mallocPointer(1);
            checkVulkan(vkCreateInstance(instanceCreateInfo, null, instancePointer),
                    "vkCreateInstance");
            VkInstance instance = new VkInstance(instancePointer.get(0), instanceCreateInfo);

            LongBuffer surfacePointer = stack.mallocLong(1);
            checkVulkan(glfwCreateWindowSurface(instance, windowHandle, null, surfacePointer),
                    "glfwCreateWindowSurface");
            long surface = surfacePointer.get(0);

            VkPhysicalDevice physicalDevice = selectPhysicalDevice(stack, instance);
            int queueFamilyIndex = selectGraphicsPresentQueueFamily(stack, physicalDevice,
                    surface);

            VkDeviceQueueCreateInfo.Buffer queueCreateInfos =
                    VkDeviceQueueCreateInfo.calloc(1, stack);
            queueCreateInfos.get(0)
                    .sType(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO)
                    .queueFamilyIndex(queueFamilyIndex)
                    .pQueuePriorities(stack.floats(1.0f));

            PointerBuffer deviceExtensions = stack.mallocPointer(1);
            deviceExtensions.put(0, stack.UTF8(VK_KHR_SWAPCHAIN_EXTENSION_NAME));
            VkDeviceCreateInfo deviceCreateInfo = VkDeviceCreateInfo.calloc(stack)
                    .sType(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO)
                    .pQueueCreateInfos(queueCreateInfos)
                    .ppEnabledExtensionNames(deviceExtensions);

            PointerBuffer devicePointer = stack.mallocPointer(1);
            checkVulkan(vkCreateDevice(physicalDevice, deviceCreateInfo, null, devicePointer),
                    "vkCreateDevice");
            VkDevice logicalDevice = new VkDevice(devicePointer.get(0), physicalDevice,
                    deviceCreateInfo, VK_API_VERSION_1_3);

            PointerBuffer queuePointer = stack.mallocPointer(1);
            vkGetDeviceQueue(logicalDevice, queueFamilyIndex, 0, queuePointer);
            VkQueue queue = new VkQueue(queuePointer.get(0), logicalDevice);

            PresentationBootstrapHandles handles = new PresentationBootstrapHandles(
                    instance.address(), physicalDevice.address(), logicalDevice.address(),
                    surface, queue.address(), queue.address(), queueFamilyIndex,
                    queueFamilyIndex);
            return new VulkanBootstrap(instance, logicalDevice, surface, handles);
        }
    }

    private static VkPhysicalDevice selectPhysicalDevice(MemoryStack stack,
                                                         VkInstance instance) {
        IntBuffer deviceCount = stack.mallocInt(1);
        checkVulkan(vkEnumeratePhysicalDevices(instance, deviceCount, null),
                "vkEnumeratePhysicalDevices");
        if (deviceCount.get(0) == 0) {
            throw new IllegalStateException("No Vulkan physical device is available");
        }
        PointerBuffer physicalDevices = stack.mallocPointer(deviceCount.get(0));
        checkVulkan(vkEnumeratePhysicalDevices(instance, deviceCount, physicalDevices),
                "vkEnumeratePhysicalDevices");
        return new VkPhysicalDevice(physicalDevices.get(0), instance);
    }

    private static int selectGraphicsPresentQueueFamily(MemoryStack stack,
                                                        VkPhysicalDevice physicalDevice,
                                                        long surface) {
        IntBuffer familyCount = stack.mallocInt(1);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, familyCount, null);
        VkQueueFamilyProperties.Buffer families =
                VkQueueFamilyProperties.calloc(familyCount.get(0), stack);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, familyCount, families);
        IntBuffer presentSupport = stack.mallocInt(1);
        for (int familyIndex = 0; familyIndex < families.capacity(); ++familyIndex) {
            boolean graphicsCapable =
                    (families.get(familyIndex).queueFlags() & VK_QUEUE_GRAPHICS_BIT) != 0;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, familyIndex, surface,
                    presentSupport);
            if (graphicsCapable && presentSupport.get(0) == VK_TRUE) {
                return familyIndex;
            }
        }
        throw new IllegalStateException(
                "No queue family supports both graphics and presentation");
    }

    private static void checkVulkan(int vulkanResult, String operation) {
        if (vulkanResult != VK_SUCCESS) {
            throw new IllegalStateException(operation + " failed with VkResult " + vulkanResult);
        }
    }

    /**
     * @note ThreadSafety: Single-threaded; call after the Native runtime is destroyed.
     * Destroys the device, surface and instance in reverse creation order.
     */
    @Override
    public void close() {
        vkDestroyDevice(m_logicalDevice, null);
        KHRSurface.vkDestroySurfaceKHR(m_instance, m_surface, null);
        vkDestroyInstance(m_instance, null);
    }
}
