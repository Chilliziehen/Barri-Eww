#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include <vulkan/vulkan.h>

namespace barrieww {

/**
 * @note ThreadSafety: Render-thread confined. Creation, access, detachment, movement and
 *       destruction must be serialized on the presentation render thread.
 * @brief Owns one host-image-dependent presentation generation and its prerecorded
 *        frame-slot by swapchain-image command matrix.
 * @warning MemoryOwnership: Owns the sampled host image view, sampler, descriptors,
 *          pipeline objects, command pool and command buffers. Borrows the logical
 *          device, host image, swapchain images, and swapchain image views.
 */
class VulkanHostImagePresentationResources {
public:
    struct CreateInfo {
        VkDevice logicalDevice;
        std::uint32_t graphicsQueueFamilyIndex;
        VkImage hostImage;
        VkFormat hostImageFormat;
        VkFormat swapchainImageFormat;
        VkExtent2D extent;
        std::span<const VkImage> swapchainImages;
        std::span<const VkImageView> swapchainImageViews;
        std::uint32_t frameSlotCount;
    };

    struct CreationFailure {
        VkResult vulkanResult;
    };

    /**
     * @note ThreadSafety: Render-thread confined; external synchronization is required.
     * @brief Transactionally creates all fixed host presentation resources and records
     *        every command matrix entry once.
     * @param const CreateInfo& createInfo Borrowed generation handles and fixed dimensions
     * @return std::expected<VulkanHostImagePresentationResources, CreationFailure> The
     *         complete owner, or the exact Vulkan failure; invalid input reports
     *         VK_ERROR_INITIALIZATION_FAILED.
     */
    [[nodiscard]] static std::expected<VulkanHostImagePresentationResources,
                                       CreationFailure>
    create(const CreateInfo& createInfo);

    VulkanHostImagePresentationResources(
        const VulkanHostImagePresentationResources&) = delete;
    VulkanHostImagePresentationResources& operator=(
        const VulkanHostImagePresentationResources&) = delete;

    /** Move transfers all owned resources and leaves the source detached. */
    VulkanHostImagePresentationResources(
        VulkanHostImagePresentationResources&& movedFrom) noexcept;

    VulkanHostImagePresentationResources& operator=(
        VulkanHostImagePresentationResources&&) = delete;

    /** Destroys attached resources; caller guarantees that no submission references them. */
    ~VulkanHostImagePresentationResources();

    /** Returns the prevalidated O(1) matrix entry for the given frame slot and image. */
    [[nodiscard]] VkCommandBuffer commandBuffer(std::uint32_t frameSlotIndex,
                                                std::uint32_t imageIndex) const noexcept;

    /**
     * @note ThreadSafety: Render-thread confined; no concurrent submission is permitted.
     * @brief Waits submitted frame-slot fences, excluding a reset but unsubmitted open-frame
     *        fence, then destroys all owned resources. A detached owner is idempotent.
     * @param std::span<const VkFence> submittedFrameFences Fences whose submissions may
     *        reference this generation
     * @param VkFence unsubmittedOpenFrameFence Fence to omit, or VK_NULL_HANDLE
     * @return std::expected<void, VkResult> Success after destruction, or the exact wait
     *         failure with all resources retained.
     */
    [[nodiscard]] std::expected<void, VkResult> destroyAfterSubmittedFrames(
        std::span<const VkFence> submittedFrameFences,
        VkFence unsubmittedOpenFrameFence);

private:
    VulkanHostImagePresentationResources() = default;

    void destroyOwnedObjects() noexcept;

    VkDevice m_logicalDevice = VK_NULL_HANDLE;
    VkImageView m_hostImageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;
    std::size_t m_swapchainImageCount = 0u;
};

} // namespace barrieww
