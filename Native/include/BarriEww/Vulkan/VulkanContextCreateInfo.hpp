#pragma once

#include <vulkan/vulkan.h>

#include "BarriEww/Vulkan/VulkanQueueFamilyIndices.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief The externally-provided Vulkan handles used to construct a VulkanContext
 *        (named after the Vulkan Vk*CreateInfo idiom, spec §1.1.2 exemption). Per
 *        ADR-0001 the native backend does not create these; the Java/FFM layer supplies
 *        them, having obtained them from Minecraft's Vulkan backend or created them itself.
 *        Mandatory handles are instance, physicalDevice, logicalDevice and graphicsQueue;
 *        presentQueue is required for on-screen rendering, while transferQueue and
 *        computeQueue are optional and may be VK_NULL_HANDLE when unused.
 * @warning MemoryOwnership: Every handle here is BORROWED. Neither this struct nor the
 *          VulkanContext built from it creates or destroys them; the provider retains
 *          ownership and must outlive the VulkanContext.
 */
struct VulkanContextCreateInfo {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice logicalDevice = VK_NULL_HANDLE;
    VulkanQueueFamilyIndices queueFamilyIndices;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkQueue transferQueue = VK_NULL_HANDLE;
    VkQueue computeQueue = VK_NULL_HANDLE;
};

} // namespace barrieww
