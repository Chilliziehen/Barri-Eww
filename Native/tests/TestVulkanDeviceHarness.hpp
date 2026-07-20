#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

#include "BarriEww/Vulkan/VulkanContext.hpp"
#include "BarriEww/Vulkan/VulkanContextCreateInfo.hpp"

namespace barrieww::testing {

/**
 * @note ThreadSafety: Not thread-safe; each test owns one harness on one thread.
 * @brief Test-only headless Vulkan bring-up: instance → first transfer-capable
 *        physical device → logical device with one queue → command pool. No surface,
 *        no window, no extensions — enough to execute transfer work for recorder
 *        tests on a real driver (lavapipe on CI, native driver locally). create()
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
    /** Attempts full bring-up; returns nullptr when no usable driver/device exists. */
    [[nodiscard]] static std::unique_ptr<TestVulkanDeviceHarness> create() {
        auto harness = std::unique_ptr<TestVulkanDeviceHarness>(new TestVulkanDeviceHarness());
        return harness->initialize() ? std::move(harness) : nullptr;
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

    /** Full bring-up; false when any step finds no usable driver/device. */
    [[nodiscard]] bool initialize() {
        VkApplicationInfo applicationInfo{};
        applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        applicationInfo.pApplicationName = "BarriEwwNativeTests";
        applicationInfo.apiVersion = VK_API_VERSION_1_2;
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

        // First device with a transfer-capable family (graphics/compute imply transfer).
        for (VkPhysicalDevice candidateDevice : physicalDevices) {
            std::uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(candidateDevice, &queueFamilyCount,
                                                     nullptr);
            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(candidateDevice, &queueFamilyCount,
                                                     queueFamilies.data());
            for (std::uint32_t familyIndex = 0; familyIndex < queueFamilyCount;
                 ++familyIndex) {
                constexpr VkQueueFlags transferCapableFlags =
                    VK_QUEUE_TRANSFER_BIT | VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
                if (queueFamilies[familyIndex].queueFlags & transferCapableFlags) {
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

        // Query and, when available, enable bufferDeviceAddress (core 1.2 feature) so
        // BDA-using tests can run; tests SKIP via supportsBufferDeviceAddress() otherwise.
        VkPhysicalDeviceVulkan12Features supportedVulkan12Features{};
        supportedVulkan12Features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        VkPhysicalDeviceFeatures2 supportedFeatures{};
        supportedFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        supportedFeatures.pNext = &supportedVulkan12Features;
        vkGetPhysicalDeviceFeatures2(m_physicalDevice, &supportedFeatures);
        m_supportsBufferDeviceAddress =
            supportedVulkan12Features.bufferDeviceAddress == VK_TRUE;

        VkPhysicalDeviceVulkan12Features enabledVulkan12Features{};
        enabledVulkan12Features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        enabledVulkan12Features.bufferDeviceAddress =
            m_supportsBufferDeviceAddress ? VK_TRUE : VK_FALSE;

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
        if (vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_logicalDevice)
            != VK_SUCCESS) {
            return false;
        }
        vkGetDeviceQueue(m_logicalDevice, m_queueFamilyIndex, 0u, &m_queue);

        VkCommandPoolCreateInfo commandPoolCreateInfo{};
        commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolCreateInfo.queueFamilyIndex = m_queueFamilyIndex;
        return vkCreateCommandPool(m_logicalDevice, &commandPoolCreateInfo, nullptr,
                                   &m_commandPool)
               == VK_SUCCESS;
    }

    VkInstance m_instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_logicalDevice = VK_NULL_HANDLE;
    std::uint32_t m_queueFamilyIndex = 0;
    VkQueue m_queue = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    bool m_supportsBufferDeviceAddress = false;
};

} // namespace barrieww::testing
