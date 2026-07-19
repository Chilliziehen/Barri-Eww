#include "BarriEww/Vulkan/VulkanContext.hpp"

#include <stdexcept>

namespace barrieww {

VulkanContext::VulkanContext(const VulkanContextCreateInfo& createInfo)
    : m_instance(createInfo.instance)
    , m_physicalDevice(createInfo.physicalDevice)
    , m_logicalDevice(createInfo.logicalDevice)
    , m_queueFamilyIndices(createInfo.queueFamilyIndices)
    , m_graphicsQueue(createInfo.graphicsQueue)
    , m_presentQueue(createInfo.presentQueue)
    , m_transferQueue(createInfo.transferQueue)
    , m_computeQueue(createInfo.computeQueue) {
    // Slow-path validation (init). Exceptions are acceptable here and must never cross the
    // FFM boundary (§6.4 / §7.1.2): the Java layer translates a failed construction into a
    // checked exception on its own side.
    if (m_instance == VK_NULL_HANDLE) {
        throw std::invalid_argument(
            "VulkanContext: instance handle must not be VK_NULL_HANDLE");
    }
    if (m_physicalDevice == VK_NULL_HANDLE) {
        throw std::invalid_argument(
            "VulkanContext: physicalDevice handle must not be VK_NULL_HANDLE");
    }
    if (m_logicalDevice == VK_NULL_HANDLE) {
        throw std::invalid_argument(
            "VulkanContext: logicalDevice handle must not be VK_NULL_HANDLE");
    }
    if (m_graphicsQueue == VK_NULL_HANDLE) {
        throw std::invalid_argument(
            "VulkanContext: graphicsQueue handle must not be VK_NULL_HANDLE");
    }
    if (!m_queueFamilyIndices.hasRequiredFamilies()) {
        throw std::invalid_argument(
            "VulkanContext: queueFamilyIndices missing graphics or present family");
    }
}

} // namespace barrieww
