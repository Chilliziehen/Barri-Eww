#pragma once

#include <vulkan/vulkan.h>

#include "BarriEww/Vulkan/VulkanContextCreateInformation.hpp"
#include "BarriEww/Vulkan/VulkanQueueFamilyIndices.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: After construction the accessors are read-only and thread-safe
 *       (the held handles are immutable and there is no mutable state). Construction
 *       itself is not thread-safe and is expected on the slow init path.
 * @brief Immutable holder of the Vulkan handles the native backend operates on. Per
 *        ADR-0001 the backend does NOT create the instance / device / queues: they are
 *        provided by the Java/FFM layer (obtained from Minecraft's Vulkan backend or
 *        created there). This class validates the provided handles and exposes them; it
 *        performs no Vulkan API calls of its own.
 * @warning MemoryOwnership: All held handles are BORROWED from the provider. VulkanContext
 *          neither destroys them nor extends their lifetime; the provider must keep every
 *          handle alive for the whole lifetime of this VulkanContext.
 */
class VulkanContext {
public:
    /**
     * @note ThreadSafety: Not thread-safe; call on the init path.
     * @brief Constructs the context from externally-provided handles, validating that the
     *        mandatory handles (instance, physicalDevice, logicalDevice, graphicsQueue) are
     *        non-null and that the required queue families (graphics, present) are present.
     * @param createInformation The externally-provided Vulkan handles to borrow.
     * @throws std::invalid_argument When a mandatory handle is VK_NULL_HANDLE, or when a
     *         required queue family is missing.
     * @warning MemoryOwnership: Borrows every handle in createInformation; see the class
     *          note. No ownership is taken and none crosses back over the FFM boundary.
     */
    explicit VulkanContext(const VulkanContextCreateInformation& createInformation);

    /**
     * @note ThreadSafety: Thread-safe (returns an immutable borrowed handle).
     * @brief The borrowed Vulkan instance.
     * @return VkInstance The instance handle provided at construction.
     */
    [[nodiscard]] VkInstance instance() const noexcept { return m_instance; }

    /**
     * @note ThreadSafety: Thread-safe (returns an immutable borrowed handle).
     * @brief The borrowed physical device.
     * @return VkPhysicalDevice The physical device handle provided at construction.
     */
    [[nodiscard]] VkPhysicalDevice physicalDevice() const noexcept { return m_physicalDevice; }

    /**
     * @note ThreadSafety: Thread-safe (returns an immutable borrowed handle).
     * @brief The borrowed logical device.
     * @return VkDevice The logical device handle provided at construction.
     */
    [[nodiscard]] VkDevice logicalDevice() const noexcept { return m_logicalDevice; }

    /**
     * @note ThreadSafety: Thread-safe (returns a reference to an immutable value member).
     * @brief The resolved queue family indices for the physical device.
     * @return const VulkanQueueFamilyIndices& The queue family indices provided at construction.
     */
    [[nodiscard]] const VulkanQueueFamilyIndices& queueFamilyIndices() const noexcept {
        return m_queueFamilyIndices;
    }

    /**
     * @note ThreadSafety: Thread-safe (returns an immutable borrowed handle).
     * @brief The borrowed graphics queue.
     * @return VkQueue The graphics queue handle provided at construction.
     */
    [[nodiscard]] VkQueue graphicsQueue() const noexcept { return m_graphicsQueue; }

    /**
     * @note ThreadSafety: Thread-safe (returns an immutable borrowed handle).
     * @brief The borrowed present queue.
     * @return VkQueue The present queue handle provided at construction (may be VK_NULL_HANDLE).
     */
    [[nodiscard]] VkQueue presentQueue() const noexcept { return m_presentQueue; }

    /**
     * @note ThreadSafety: Thread-safe (returns an immutable borrowed handle).
     * @brief The borrowed transfer queue.
     * @return VkQueue The transfer queue handle provided at construction (may be VK_NULL_HANDLE).
     */
    [[nodiscard]] VkQueue transferQueue() const noexcept { return m_transferQueue; }

    /**
     * @note ThreadSafety: Thread-safe (returns an immutable borrowed handle).
     * @brief The borrowed compute queue.
     * @return VkQueue The compute queue handle provided at construction (may be VK_NULL_HANDLE).
     */
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
