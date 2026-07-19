#pragma once

#include <vulkan/vulkan.h>

#include "BarriEww/Vulkan/VulkanContextCreateInfo.hpp"
#include "BarriEww/Vulkan/VulkanQueueFamilyIndices.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: After construction every accessor is read-only and thread-safe
 *       (the held handles are immutable and there is no mutable state); this blanket
 *       statement covers all accessors below (spec §2.6). Construction itself is not
 *       thread-safe and is expected on the slow init path.
 * @brief Immutable holder of the Vulkan handles the native backend operates on. Per
 *        ADR-0001 the backend does NOT create the instance / device / queues: they are
 *        provided by the Java/FFM layer (obtained from Minecraft's Vulkan backend or
 *        created there). This class validates the provided handles and exposes them; it
 *        performs no Vulkan API calls of its own.
 * @warning MemoryOwnership: All held handles are BORROWED from the provider; this blanket
 *          statement covers every accessor below (spec §2.6). VulkanContext neither
 *          destroys the handles nor extends their lifetime; the provider must keep every
 *          handle alive for the whole lifetime of this VulkanContext.
 */
class VulkanContext {
public:
    /**
     * @note ThreadSafety: Not thread-safe; call on the init path.
     * @brief Constructs the context from externally-provided handles, validating that the
     *        mandatory handles (instance, physicalDevice, logicalDevice, graphicsQueue) are
     *        non-null and that the required queue families (graphics, present) are present.
     * @param createInfo The externally-provided Vulkan handles to borrow.
     * @throws std::invalid_argument When a mandatory handle is VK_NULL_HANDLE, or when a
     *         required queue family is missing.
     * @warning MemoryOwnership: Borrows every handle in createInfo; see the class note.
     *          No ownership is taken and none crosses back over the FFM boundary.
     */
    explicit VulkanContext(const VulkanContextCreateInfo& createInfo);

    /** The borrowed Vulkan instance provided at construction (see class notes). */
    [[nodiscard]] VkInstance instance() const noexcept { return m_instance; }

    /** The borrowed physical device provided at construction (see class notes). */
    [[nodiscard]] VkPhysicalDevice physicalDevice() const noexcept { return m_physicalDevice; }

    /** The borrowed logical device provided at construction (see class notes). */
    [[nodiscard]] VkDevice logicalDevice() const noexcept { return m_logicalDevice; }

    /** The resolved queue family indices provided at construction (see class notes). */
    [[nodiscard]] const VulkanQueueFamilyIndices& queueFamilyIndices() const noexcept {
        return m_queueFamilyIndices;
    }

    /** The borrowed graphics queue provided at construction (see class notes). */
    [[nodiscard]] VkQueue graphicsQueue() const noexcept { return m_graphicsQueue; }

    /** The borrowed present queue; may be VK_NULL_HANDLE (see class notes). */
    [[nodiscard]] VkQueue presentQueue() const noexcept { return m_presentQueue; }

    /** The borrowed transfer queue; may be VK_NULL_HANDLE (see class notes). */
    [[nodiscard]] VkQueue transferQueue() const noexcept { return m_transferQueue; }

    /** The borrowed compute queue; may be VK_NULL_HANDLE (see class notes). */
    [[nodiscard]] VkQueue computeQueue() const noexcept { return m_computeQueue; }

private:
    VkInstance m_instance;
    VkPhysicalDevice m_physicalDevice;
    VkDevice m_logicalDevice;
    VulkanQueueFamilyIndices m_queueFamilyIndices;
    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;
    VkQueue m_transferQueue;
    VkQueue m_computeQueue;
};

} // namespace barrieww
