#pragma once

#include <expected>

#include <vulkan/vulkan.h>

namespace barrieww {

/**
 * @note ThreadSafety: Render-thread confined. Creation, recording, movement,
 * and destruction must be externally serialized.
 * @brief Owns the common host-image fullscreen composition resources: sampled
 * image view, nearest sampler, descriptor objects, pipeline layout, and
 * graphics pipeline.
 * @warning MemoryOwnership: Owns every dependent Vulkan object listed above.
 * Borrows the logical device and host image, which must outlive this owner.
 */
class VulkanHostImageCompositionResources {
  public:
    /**
     * @note ThreadSafety: Immutable input consumed on the render thread.
     * @brief Describes the borrowed host image and fixed output pipeline
     * dimensions.
     * @warning MemoryOwnership: Every handle remains owned by the caller and must
     * outlive the created composition resources.
     */
    struct CreateInfo {
        VkDevice logicalDevice;
        VkImage hostImage;
        VkFormat hostImageFormat;
        VkFormat outputImageFormat;
        VkExtent2D extent;
    };

    /**
     * @note ThreadSafety: Render-thread confined; external synchronization is
     * required.
     * @brief Transactionally creates all common composition resources from
     * generated production shader bytes and resolves KHR dynamic-rendering entry
     * points.
     * @param const CreateInfo& createInfo Borrowed handles, formats, and fixed
     * extent
     * @return std::expected<VulkanHostImageCompositionResources, VkResult>
     * Complete owner or the exact Vulkan failure; invalid input reports
     * VK_ERROR_INITIALIZATION_FAILED.
     * @warning MemoryOwnership: Success transfers newly created Vulkan objects
     * into the returned owner; every CreateInfo handle remains borrowed.
     */
    [[nodiscard]] static std::expected<VulkanHostImageCompositionResources, VkResult>
    create(const CreateInfo& createInfo);

    /**
     * @note ThreadSafety: Copying is unavailable under every threading condition.
     * @brief Prevents duplication of unique Vulkan resource ownership.
     * @param const VulkanHostImageCompositionResources& copiedFrom Unavailable
     * source
     * @warning MemoryOwnership: Copies no Vulkan object ownership.
     */
    VulkanHostImageCompositionResources(const VulkanHostImageCompositionResources& copiedFrom) =
        delete;

    /**
     * @note ThreadSafety: Copy assignment is unavailable under every threading
     * condition.
     * @brief Prevents replacement through duplicated Vulkan resource ownership.
     * @param const VulkanHostImageCompositionResources& copiedFrom Unavailable
     * source
     * @return VulkanHostImageCompositionResources& Unavailable destination
     * reference
     * @warning MemoryOwnership: Copies and releases no Vulkan object ownership.
     */
    VulkanHostImageCompositionResources&
    operator=(const VulkanHostImageCompositionResources& copiedFrom) = delete;

    /**
     * @note ThreadSafety: Render-thread confined; movement must not overlap
     * recording.
     * @brief Transfers every owned composition object and detaches the source.
     * @param VulkanHostImageCompositionResources&& movedFrom Source owner to
     * detach
     * @warning MemoryOwnership: Transfers all owned objects; borrowed handles
     * stay borrowed.
     */
    VulkanHostImageCompositionResources(VulkanHostImageCompositionResources&& movedFrom) noexcept;

    /**
     * @note ThreadSafety: Move assignment is unavailable under every threading
     * condition.
     * @brief Prevents implicit destruction of attached resources during
     * replacement.
     * @param VulkanHostImageCompositionResources&& movedFrom Unavailable source
     * @return VulkanHostImageCompositionResources& Unavailable destination
     * reference
     * @warning MemoryOwnership: Transfers and releases no Vulkan object
     * ownership.
     */
    VulkanHostImageCompositionResources&
    operator=(VulkanHostImageCompositionResources&& movedFrom) = delete;

    /**
     * @note ThreadSafety: Render-thread confined; destruction must not overlap
     * submission.
     * @brief Destroys all attached composition resources in reverse dependency
     * order.
     * @warning MemoryOwnership: Destroys owned objects and never destroys
     * borrowed handles.
     */
    ~VulkanHostImageCompositionResources();

    /**
     * @note ThreadSafety: Recording-thread confined; callers externally
     * synchronize every command buffer and all referenced Vulkan objects.
     * @brief Records begin KHR dynamic rendering, pipeline and descriptor
     * binding, a three-vertex fullscreen draw, and end rendering for one output
     * image view. The caller owns all image barriers and command-buffer begin/end
     * operations.
     * @param VkCommandBuffer commandBuffer Borrowed command buffer in recording
     * state
     * @param VkImageView outputImageView Borrowed color-attachment view matching
     * output format
     * @warning MemoryOwnership: Borrows both handles and records references
     * without extending their lifetimes.
     */
    void recordCommands(VkCommandBuffer commandBuffer, VkImageView outputImageView) const noexcept;

  private:
    /**
     * @note ThreadSafety: Render-thread confined through transactional creation.
     * @brief Constructs a detached candidate with every owned handle null.
     * @warning MemoryOwnership: Acquires no resources.
     */
    VulkanHostImageCompositionResources() = default;

    /**
     * @note ThreadSafety: Render-thread confined; no concurrent recording is
     * permitted.
     * @brief Destroys all currently attached composition objects and detaches
     * this owner.
     * @warning MemoryOwnership: Releases owned objects and never releases
     * borrowed handles.
     */
    void destroyOwnedObjects() noexcept;

    VkDevice m_logicalDevice = VK_NULL_HANDLE;
    VkExtent2D m_extent{};
    VkImageView m_hostImageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    PFN_vkCmdBeginRenderingKHR m_beginRenderingFunction = nullptr;
    PFN_vkCmdEndRenderingKHR m_endRenderingFunction = nullptr;
};

} // namespace barrieww
