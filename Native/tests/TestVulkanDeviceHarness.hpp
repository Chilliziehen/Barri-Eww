#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include <vulkan/vulkan.h>

#include "BarriEww/Vulkan/VulkanContext.hpp"
#include "BarriEww/Vulkan/VulkanContextCreateInfo.hpp"
#include "TestVulkanDeviceSelection.hpp"

namespace barrieww::testing {

/**
 * @note ThreadSafety: Not thread-safe; each test owns one harness on one thread.
 * @brief Test-only headless Vulkan bring-up: instance → first queue satisfying the profile
 *        → logical device with one queue → command pool. The general profile requires
 *        graphics and compute; focused callers may explicitly request graphics only. No
 *        surface or window; Vulkan 1.2 devices enable KHR dynamic rendering when present.
 *        Tests execute on a real driver (lavapipe on CI, native driver locally). create()
 *        returns nullptr when no Vulkan driver is present so tests can SKIP.
 *        Production bring-up is NOT this class: the real backend receives its device
 *        from the Java/FFM layer (ADR-0001); the harness only exists because tests
 *        have no host to receive from.
 * @warning MemoryOwnership: OWNS instance, device and command pool (destroyed in the
 *          destructor). VulkanContext instances built via makeContextCreateInfo()
 *          BORROW these handles and must not outlive the harness.
 */
class TestVulkanDeviceHarness {
public:
    /**
     * @note ThreadSafety: Creates one test-thread-confined harness per invocation.
     * @brief Attempts general test bring-up with one queue supporting both graphics and
     *        compute operations; returns nullptr when no suitable driver or device exists.
     * @return std::unique_ptr<TestVulkanDeviceHarness> Owner, or nullptr when unavailable
     * @warning MemoryOwnership: Transfers complete harness ownership to the caller.
     */
    [[nodiscard]] static std::unique_ptr<TestVulkanDeviceHarness> create() {
        auto harness = std::unique_ptr<TestVulkanDeviceHarness>(new TestVulkanDeviceHarness());
        constexpr VkQueueFlags requiredQueueFlags =
            VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
        return harness->initialize<false>(requiredQueueFlags)
            ? std::move(harness)
            : nullptr;
    }

    /**
     * @note ThreadSafety: Creates one test-thread-confined harness per invocation.
     * @brief Attempts bring-up constrained to Vulkan API 1.2 with the
     *        VK_KHR_dynamic_rendering extension and feature both required and enabled.
     * @param VkQueueFlags requiredQueueFlags Every operation the selected queue must support
     * @return std::unique_ptr<TestVulkanDeviceHarness> Owner, or nullptr when unavailable
     * @warning MemoryOwnership: Transfers complete harness ownership to the caller.
     */
    [[nodiscard]] static std::unique_ptr<TestVulkanDeviceHarness>
    createWithVulkan12DynamicRenderingExtension(VkQueueFlags requiredQueueFlags) {
        auto harness = std::unique_ptr<TestVulkanDeviceHarness>(new TestVulkanDeviceHarness());
        return harness->initialize<true>(requiredQueueFlags)
            ? std::move(harness)
            : nullptr;
    }

    TestVulkanDeviceHarness(const TestVulkanDeviceHarness&) = delete;
    TestVulkanDeviceHarness& operator=(const TestVulkanDeviceHarness&) = delete;

    ~TestVulkanDeviceHarness() {
        if (m_commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_logicalDevice, m_commandPool, nullptr);
        }
        if (m_logicalDevice != VK_NULL_HANDLE) {
            vkDestroyDevice(m_logicalDevice, nullptr);
        }
        if (m_instance != VK_NULL_HANDLE) {
            vkDestroyInstance(m_instance, nullptr);
        }
    }

    /** Whether the device was created with the bufferDeviceAddress feature (core 1.2). */
    [[nodiscard]] bool supportsBufferDeviceAddress() const noexcept {
        return m_supportsBufferDeviceAddress;
    }

    /** Whether dynamic rendering (core Vulkan 1.3 or KHR on Vulkan 1.2) was enabled. */
    [[nodiscard]] bool supportsDynamicRendering() const noexcept {
        return m_supportsDynamicRendering;
    }

    /**
     * @note ThreadSafety: Read-only after create() returns; concurrent reads are safe.
     * @brief Reports whether both KHR dynamic-rendering command entry points resolved.
     * @return bool True when vkCmdBeginRenderingKHR and vkCmdEndRenderingKHR are available
     * @warning MemoryOwnership: Reads a cached capability and transfers no ownership.
     */
    [[nodiscard]] bool supportsDynamicRenderingExtensionEntryPoints() const noexcept {
        return m_supportsDynamicRenderingExtensionEntryPoints;
    }

    /** Borrowed-handle create info for constructing a VulkanContext (see class notes). */
    [[nodiscard]] VulkanContextCreateInfo makeContextCreateInfo() const {
        VulkanContextCreateInfo contextCreateInfo{};
        contextCreateInfo.instance = m_instance;
        contextCreateInfo.physicalDevice = m_physicalDevice;
        contextCreateInfo.logicalDevice = m_logicalDevice;
        contextCreateInfo.queueFamilyIndices.graphicsFamily = m_queueFamilyIndex;
        contextCreateInfo.queueFamilyIndices.presentFamily = m_queueFamilyIndex;
        contextCreateInfo.graphicsQueue = m_queue;
        contextCreateInfo.presentQueue = m_queue;
        return contextCreateInfo;
    }

    /** Allocates one primary command buffer from the harness pool. */
    [[nodiscard]] VkCommandBuffer allocateCommandBuffer() const {
        VkCommandBufferAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandPool = m_commandPool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = 1u;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        vkAllocateCommandBuffers(m_logicalDevice, &allocateInfo, &commandBuffer);
        return commandBuffer;
    }

    /** Submits one command buffer and blocks until the queue is idle (test-grade sync). */
    [[nodiscard]] bool submitAndWait(VkCommandBuffer commandBuffer) const {
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1u;
        submitInfo.pCommandBuffers = &commandBuffer;
        if (vkQueueSubmit(m_queue, 1u, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
            return false;
        }
        return vkQueueWaitIdle(m_queue) == VK_SUCCESS;
    }

private:
    TestVulkanDeviceHarness() = default;

    /**
     * @note ThreadSafety: Test-thread confined during harness creation.
     * @brief Performs full bring-up under either the existing general profile or the
     *        compile-time-selected Vulkan 1.2 dynamic-rendering-extension profile.
     * @param VkQueueFlags requiredQueueFlags Every operation the selected queue must support
     * @return bool True when every required driver, device, feature, and object exists
     * @warning MemoryOwnership: Acquires all successful Vulkan objects into this harness.
     */
    template<bool requiresVulkan12DynamicRenderingExtension>
    [[nodiscard]] bool initialize(VkQueueFlags requiredQueueFlags) {
        VkApplicationInfo applicationInfo{};
        applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        applicationInfo.pApplicationName = "BarriEwwNativeTests";
        applicationInfo.apiVersion = requiresVulkan12DynamicRenderingExtension
            ? VK_API_VERSION_1_2
            : VK_API_VERSION_1_3;
        VkInstanceCreateInfo instanceCreateInfo{};
        instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.pApplicationInfo = &applicationInfo;
        if (vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance) != VK_SUCCESS) {
            return false;
        }

        std::uint32_t physicalDeviceCount = 0;
        vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, nullptr);
        if (physicalDeviceCount == 0u) {
            return false;
        }
        std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
        vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, physicalDevices.data());

        /** Every candidate remains eligible for inspection until all profile requirements
         *  identify one suitable device and queue family. */
        for (VkPhysicalDevice candidateDevice : physicalDevices) {
            if constexpr (requiresVulkan12DynamicRenderingExtension) {
                VkPhysicalDeviceProperties candidateProperties{};
                vkGetPhysicalDeviceProperties(candidateDevice, &candidateProperties);
                const bool hasDynamicRenderingExtension = supportsDeviceExtension(
                    candidateDevice, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
                bool hasDynamicRenderingFeature = false;
                if (candidateProperties.apiVersion >= VK_API_VERSION_1_2
                    && hasDynamicRenderingExtension) {
                    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures{};
                    dynamicRenderingFeatures.sType =
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
                    VkPhysicalDeviceFeatures2 candidateFeatures{};
                    candidateFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
                    candidateFeatures.pNext = &dynamicRenderingFeatures;
                    vkGetPhysicalDeviceFeatures2(candidateDevice, &candidateFeatures);
                    hasDynamicRenderingFeature =
                        dynamicRenderingFeatures.dynamicRendering == VK_TRUE;
                }
                if (!isStrictDynamicRenderingDeviceEligible(
                        candidateProperties.apiVersion,
                        hasDynamicRenderingExtension,
                        hasDynamicRenderingFeature)) {
                    continue;
                }
            }
            std::uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(candidateDevice, &queueFamilyCount,
                                                     nullptr);
            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(candidateDevice, &queueFamilyCount,
                                                     queueFamilies.data());
            for (std::uint32_t familyIndex = 0; familyIndex < queueFamilyCount;
                 ++familyIndex) {
                if (queueFamilySupportsRequiredOperations(
                        queueFamilies[familyIndex].queueFlags, requiredQueueFlags)) {
                    m_physicalDevice = candidateDevice;
                    m_queueFamilyIndex = familyIndex;
                    break;
                }
            }
            if (m_physicalDevice != VK_NULL_HANDLE) {
                break;
            }
        }
        if (m_physicalDevice == VK_NULL_HANDLE) {
            return false;
        }

        // Query and, when available, enable bufferDeviceAddress and dynamicRendering.
        VkPhysicalDeviceProperties deviceProperties{};
        vkGetPhysicalDeviceProperties(m_physicalDevice, &deviceProperties);
        const bool deviceSupportsVulkan13 =
            deviceProperties.apiVersion >= VK_API_VERSION_1_3;

        const bool supportsDynamicRenderingExtension = supportsDeviceExtension(
            m_physicalDevice, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);

        VkPhysicalDeviceDynamicRenderingFeaturesKHR supportedDynamicRenderingFeatures{};
        supportedDynamicRenderingFeatures.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
        VkPhysicalDeviceVulkan12Features supportedVulkan12Features{};
        supportedVulkan12Features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        supportedVulkan12Features.pNext =
            (deviceSupportsVulkan13 || supportsDynamicRenderingExtension)
                ? &supportedDynamicRenderingFeatures
                : nullptr;
        VkPhysicalDeviceFeatures2 supportedFeatures{};
        supportedFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        supportedFeatures.pNext = &supportedVulkan12Features;
        vkGetPhysicalDeviceFeatures2(m_physicalDevice, &supportedFeatures);
        m_supportsBufferDeviceAddress =
            supportedVulkan12Features.bufferDeviceAddress == VK_TRUE;
        m_supportsDynamicRendering =
            supportedDynamicRenderingFeatures.dynamicRendering == VK_TRUE;
        if constexpr (requiresVulkan12DynamicRenderingExtension) {
            if (!supportsDynamicRenderingExtension || !m_supportsDynamicRendering) {
                return false;
            }
        }

        VkPhysicalDeviceDynamicRenderingFeaturesKHR enabledDynamicRenderingFeatures{};
        enabledDynamicRenderingFeatures.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
        enabledDynamicRenderingFeatures.dynamicRendering =
            m_supportsDynamicRendering ? VK_TRUE : VK_FALSE;
        VkPhysicalDeviceVulkan12Features enabledVulkan12Features{};
        enabledVulkan12Features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        enabledVulkan12Features.bufferDeviceAddress =
            m_supportsBufferDeviceAddress ? VK_TRUE : VK_FALSE;
        enabledVulkan12Features.pNext =
            m_supportsDynamicRendering ? &enabledDynamicRenderingFeatures : nullptr;

        const char* enabledExtensionNames[] = {VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME};

        const float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = m_queueFamilyIndex;
        queueCreateInfo.queueCount = 1u;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = &enabledVulkan12Features;
        deviceCreateInfo.queueCreateInfoCount = 1u;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        if (m_supportsDynamicRendering && supportsDynamicRenderingExtension) {
            deviceCreateInfo.enabledExtensionCount = 1u;
            deviceCreateInfo.ppEnabledExtensionNames = enabledExtensionNames;
        }
        if (vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_logicalDevice)
            != VK_SUCCESS) {
            return false;
        }
        vkGetDeviceQueue(m_logicalDevice, m_queueFamilyIndex, 0u, &m_queue);
        m_supportsDynamicRenderingExtensionEntryPoints =
            vkGetDeviceProcAddr(m_logicalDevice, "vkCmdBeginRenderingKHR") != nullptr
            && vkGetDeviceProcAddr(m_logicalDevice, "vkCmdEndRenderingKHR") != nullptr;
        if constexpr (requiresVulkan12DynamicRenderingExtension) {
            if (!m_supportsDynamicRenderingExtensionEntryPoints) {
                return false;
            }
        }

        VkCommandPoolCreateInfo commandPoolCreateInfo{};
        commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolCreateInfo.queueFamilyIndex = m_queueFamilyIndex;
        return vkCreateCommandPool(m_logicalDevice, &commandPoolCreateInfo, nullptr,
                                   &m_commandPool)
               == VK_SUCCESS;
    }

    /**
     * @note ThreadSafety: Read-only driver query during single-threaded initialization.
     * @brief Reports whether one physical device advertises an exact extension name.
     * @param VkPhysicalDevice physicalDevice Borrowed physical device to query
     * @param const char* extensionName Null-terminated Vulkan extension name
     * @return bool True when the extension is advertised
     * @warning MemoryOwnership: Borrows the device and name and transfers no ownership.
     */
    [[nodiscard]] static bool supportsDeviceExtension(
        VkPhysicalDevice physicalDevice, const char* extensionName) {
        std::uint32_t extensionCount = 0u;
        if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr,
                                                 &extensionCount, nullptr)
            != VK_SUCCESS) {
            return false;
        }
        std::vector<VkExtensionProperties> extensionProperties(extensionCount);
        if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr,
                                                 &extensionCount,
                                                 extensionProperties.data())
            != VK_SUCCESS) {
            return false;
        }
        return std::ranges::any_of(
            extensionProperties,
            [extensionName](const VkExtensionProperties& extensionProperty) {
                return std::string_view{extensionProperty.extensionName}
                    == extensionName;
            });
    }

    VkInstance m_instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_logicalDevice = VK_NULL_HANDLE;
    std::uint32_t m_queueFamilyIndex = 0;
    VkQueue m_queue = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    bool m_supportsBufferDeviceAddress = false;
    bool m_supportsDynamicRendering = false;
    bool m_supportsDynamicRenderingExtensionEntryPoints = false;
};

} // namespace barrieww::testing
