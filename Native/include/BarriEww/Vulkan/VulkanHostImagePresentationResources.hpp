#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <vector>

#include <vulkan/vulkan.h>

#include "BarriEww/Vulkan/VulkanHostImageCompositionResources.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Render-thread confined. Creation, access, detachment, movement and
 *       destruction must be serialized on the presentation render thread.
 * @brief Owns one host-image-dependent presentation generation and its prerecorded
 *        frame-slot by swapchain-image command matrix.
 * @warning MemoryOwnership: Owns common composition resources, command pool and command
 *          buffers. Borrows the logical device, host image, swapchain images, and views.
 */
class VulkanHostImagePresentationResources {
public:
    /**
     * @note ThreadSafety: Immutable input consumed on the presentation render thread.
     * @brief Describes borrowed Vulkan objects and fixed dimensions for one host-image
     *        presentation generation. Image and image-view spans must have equal lengths.
     * @warning MemoryOwnership: Every handle and span element remains owned by the caller
     *          and must outlive the created resource owner.
     */
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

    /**
     * @note ThreadSafety: Immutable value returned to the presentation render thread.
     * @brief Preserves the exact Vulkan result from creation, allocation, or command
     *        recording; invalid input is represented by VK_ERROR_INITIALIZATION_FAILED.
     * @warning MemoryOwnership: Contains no owned memory or Vulkan object.
     */
    struct CreationFailure {
        VkResult vulkanResult;
    };

    /**
     * @note ThreadSafety: Compile-time constant requiring no synchronization.
     * @brief Required swapchain usage policy for the clear priming path and host-image
     *        color-attachment composition path. Runtime surface validation consumes it.
     */
    static constexpr VkImageUsageFlags s_requiredSwapchainImageUsageFlags =
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    /**
     * @note ThreadSafety: Render-thread confined; external synchronization is required.
     * @brief Transactionally creates all fixed host presentation resources and records
     *        every command matrix entry once.
     * @param const CreateInfo& createInfo Borrowed generation handles and fixed dimensions
     * @return std::expected<VulkanHostImagePresentationResources, CreationFailure> The
     *         complete owner, or the exact Vulkan failure; invalid input reports
     *         VK_ERROR_INITIALIZATION_FAILED.
     * @warning MemoryOwnership: Success transfers newly created Vulkan objects into the
     *          returned owner; all CreateInfo handles and span elements remain borrowed.
     */
    [[nodiscard]] static std::expected<VulkanHostImagePresentationResources,
                                       CreationFailure>
    create(const CreateInfo& createInfo);

    /**
     * @note ThreadSafety: Copying is unavailable under every threading condition.
     * @brief Prevents duplication of unique Vulkan resource ownership.
     * @param const VulkanHostImagePresentationResources& copiedFrom Source that would copy
     * @warning MemoryOwnership: No resources are copied or transferred.
     */
    VulkanHostImagePresentationResources(
        const VulkanHostImagePresentationResources& copiedFrom) = delete;

    /**
     * @note ThreadSafety: Copy assignment is unavailable under every threading condition.
     * @brief Prevents replacement through duplicated Vulkan resource ownership.
     * @param const VulkanHostImagePresentationResources& copiedFrom Source that would copy
     * @return VulkanHostImagePresentationResources& Unavailable because the operation is deleted
     * @warning MemoryOwnership: No resources are copied, transferred, or destroyed.
     */
    VulkanHostImagePresentationResources& operator=(
        const VulkanHostImagePresentationResources& copiedFrom) = delete;

    /**
     * @note ThreadSafety: Render-thread confined; movement must not overlap resource use.
     * @brief Transfers every owned Vulkan object and leaves the source detached.
     * @param VulkanHostImagePresentationResources&& movedFrom Source owner to detach
     * @warning MemoryOwnership: Transfers all owned resources to this instance; borrowed
     *          device, image, and image-view lifetimes remain the caller's responsibility.
     */
    VulkanHostImagePresentationResources(
        VulkanHostImagePresentationResources&& movedFrom) noexcept;

    /**
     * @note ThreadSafety: Move assignment is unavailable under every threading condition.
     * @brief Prevents implicit destruction of attached resources during replacement.
     * @param VulkanHostImagePresentationResources&& movedFrom Source that would transfer
     * @return VulkanHostImagePresentationResources& Unavailable because the operation is deleted
     * @warning MemoryOwnership: No resources are transferred or destroyed.
     */
    VulkanHostImagePresentationResources& operator=(
        VulkanHostImagePresentationResources&& movedFrom) = delete;

    /**
     * @note ThreadSafety: Render-thread confined; destruction must not overlap submission.
     * @brief Destroys attached resources. The caller guarantees no in-flight submission
     *        references them when destruction occurs without explicit detachment.
     * @warning MemoryOwnership: Destroys only Native-owned dependent objects and never
     *          destroys the borrowed device, host image, swapchain images, or their views.
     */
    ~VulkanHostImagePresentationResources();

    /**
     * @note ThreadSafety: Render-thread confined read; no concurrent detachment is allowed.
     * @brief Selects one prerecorded primary command buffer in constant time. Both indices
     *        must be within the dimensions validated at creation.
     * @param std::uint32_t frameSlotIndex Frame-slot row in the command matrix
     * @param std::uint32_t imageIndex Swapchain-image column in the command matrix
     * @return VkCommandBuffer Borrowed prerecorded primary command buffer
     * @warning MemoryOwnership: The returned command buffer remains owned by this resource
     *          owner and becomes invalid when the owner detaches or is destroyed.
     */
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
     * @warning MemoryOwnership: On success destroys every Native-owned dependent resource;
     *          on failure retains all ownership unchanged. Fence ownership remains external.
     */
    [[nodiscard]] std::expected<void, VkResult> destroyAfterSubmittedFrames(
        std::span<const VkFence> submittedFrameFences,
        VkFence unsubmittedOpenFrameFence);

private:
    /**
     * @note ThreadSafety: Render-thread confined through transactional creation.
     * @brief Constructs a detached candidate whose owned handles are all null.
     * @warning MemoryOwnership: Acquires no resources; later creation steps populate ownership.
     */
    VulkanHostImagePresentationResources() = default;

    /**
     * @note ThreadSafety: Render-thread confined; no concurrent resource use is permitted.
     * @brief Destroys every currently attached Native-owned object in reverse dependency
     *        order and leaves this owner detached. A detached owner is a no-op.
     * @warning MemoryOwnership: Releases owned command and composition resources; never
     *          releases borrowed device, image, or image-view objects.
     */
    void destroyOwnedObjects() noexcept;

    VkDevice m_logicalDevice = VK_NULL_HANDLE;
    std::optional<VulkanHostImageCompositionResources> m_compositionResources;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;
    std::size_t m_swapchainImageCount = 0u;
};

} // namespace barrieww
