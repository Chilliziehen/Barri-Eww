#include "BarriEww/Vulkan/VulkanHostImagePresentationResources.hpp"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <limits>
#include <utility>

namespace barrieww {

namespace {

constexpr VkImageSubresourceRange g_colorSubresourceRange{
    VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};

/**
 * @note ThreadSafety: Render-thread confined through the calling creation operation.
 * @brief Wraps one exact Vulkan creation failure without remapping.
 * @param VkResult vulkanResult Raw Vulkan result returned by the failed operation
 * @return VulkanHostImagePresentationResources::CreationFailure Wrapped result
 * @warning MemoryOwnership: Returns a value containing no owned memory or Vulkan object.
 */
VulkanHostImagePresentationResources::CreationFailure creationFailure(
    VkResult vulkanResult) noexcept {
    return {vulkanResult};
}

/**
 * @note ThreadSafety: Render-thread confined and read-only over caller-owned input.
 * @brief Validates every borrowed handle and fixed matrix dimension before Vulkan calls.
 * @param const VulkanHostImagePresentationResources::CreateInfo& createInfo Candidate input
 * @return bool True when creation can safely traverse and materialize the matrix
 * @warning MemoryOwnership: Borrows every handle and span and does not extend lifetimes.
 */
bool isValidCreateInfo(
    const VulkanHostImagePresentationResources::CreateInfo& createInfo) noexcept {
    if (createInfo.logicalDevice == VK_NULL_HANDLE
        || createInfo.hostImage == VK_NULL_HANDLE
        || createInfo.hostImageFormat == VK_FORMAT_UNDEFINED
        || createInfo.swapchainImageFormat == VK_FORMAT_UNDEFINED
        || createInfo.extent.width == 0u || createInfo.extent.height == 0u
        || createInfo.frameSlotCount == 0u || createInfo.swapchainImages.empty()
        || createInfo.swapchainImages.size() != createInfo.swapchainImageViews.size()) {
        return false;
    }
    if (createInfo.swapchainImages.size()
        > std::numeric_limits<std::size_t>::max() / createInfo.frameSlotCount) {
        return false;
    }
    const std::size_t commandBufferCount =
        createInfo.swapchainImages.size() * createInfo.frameSlotCount;
    if (commandBufferCount > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    return std::ranges::none_of(createInfo.swapchainImages,
                                 [](VkImage image) { return image == VK_NULL_HANDLE; })
        && std::ranges::none_of(
            createInfo.swapchainImageViews,
            [](VkImageView imageView) { return imageView == VK_NULL_HANDLE; });
}

} // namespace

std::expected<VulkanHostImagePresentationResources,
              VulkanHostImagePresentationResources::CreationFailure>
VulkanHostImagePresentationResources::create(const CreateInfo& createInfo) {
    if (!isValidCreateInfo(createInfo)) {
        return std::unexpected(creationFailure(VK_ERROR_INITIALIZATION_FAILED));
    }

    VulkanHostImagePresentationResources resources;
    resources.m_logicalDevice = createInfo.logicalDevice;
    resources.m_swapchainImageCount = createInfo.swapchainImages.size();
    auto compositionResources = VulkanHostImageCompositionResources::create({
        createInfo.logicalDevice,
        createInfo.hostImage,
        createInfo.hostImageFormat,
        createInfo.swapchainImageFormat,
        createInfo.extent,
    });
    if (!compositionResources.has_value()) {
        return std::unexpected(creationFailure(compositionResources.error()));
    }
    resources.m_compositionResources.emplace(std::move(compositionResources.value()));

    const VkCommandPoolCreateInfo commandPoolCreateInfo{
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        nullptr,
        0u,
        createInfo.graphicsQueueFamilyIndex,
    };
    VkResult vulkanResult = vkCreateCommandPool(createInfo.logicalDevice,
                                                &commandPoolCreateInfo, nullptr,
                                                &resources.m_commandPool);
    if (vulkanResult != VK_SUCCESS) {
        return std::unexpected(creationFailure(vulkanResult));
    }
    const std::size_t commandBufferCount =
        createInfo.frameSlotCount * createInfo.swapchainImages.size();
    resources.m_commandBuffers.resize(commandBufferCount);
    const VkCommandBufferAllocateInfo commandBufferAllocateInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        nullptr,
        resources.m_commandPool,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        static_cast<std::uint32_t>(commandBufferCount),
    };
    vulkanResult = vkAllocateCommandBuffers(createInfo.logicalDevice,
                                             &commandBufferAllocateInfo,
                                             resources.m_commandBuffers.data());
    if (vulkanResult != VK_SUCCESS) {
        return std::unexpected(creationFailure(vulkanResult));
    }

    /**
     * @note ThreadSafety: Creation is render-thread confined and no command buffer is
     *       externally visible until the complete matrix has been recorded.
     * @brief Records a fixed row-major command matrix. Each frame slot owns one row and
     *        each swapchain image owns one column, so matrixIndex equals
     *        frameSlotIndex * swapchainImageCount + imageIndex. The host image remains in
     *        GENERAL and is made visible to fragment sampling; the selected swapchain image
     *        is fully overwritten by dynamic rendering and then prepared for presentation.
     *
     * Invariants and assumptions:
     * - hostImage remains GENERAL and its producer submission precedes this command buffer;
     * - host and swapchain extents are exact, positive, and fixed for the generation;
     * - borrowed images and image views outlive every recorded command buffer;
     * - dynamic rendering is enabled and every swapchain image supports color attachment;
     * - full overwrite permits discarding prior swapchain contents with UNDEFINED.
     *
     * Semantic pseudocode:
     * for each frameSlotIndex in frameSlotCount:
     *     for each imageIndex in swapchainImageCount:
     *         matrixIndex = frameSlotIndex * swapchainImageCount + imageIndex
     *         begin commandBuffers[matrixIndex]
     *         barrier hostImage GENERAL -> GENERAL from
     *             COLOR_ATTACHMENT_OUTPUT | TRANSFER and MEMORY_WRITE to
     *             FRAGMENT_SHADER and SHADER_READ
     *         barrier swapchainImages[imageIndex] UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL
     *         record common composition for swapchainImageViews[imageIndex]
     *         barrier swapchainImages[imageIndex]
     *             COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR
     *         end commandBuffers[matrixIndex]
     *
     * Creation complexity is O(frameSlotCount * swapchainImageCount) time and storage.
     * commandBuffer lookup is O(1) time with no allocation or recording.
     */
    for (std::uint32_t frameSlotIndex = 0u;
         frameSlotIndex < createInfo.frameSlotCount; ++frameSlotIndex) {
        for (std::size_t imageIndex = 0u;
             imageIndex < createInfo.swapchainImages.size(); ++imageIndex) {
            const std::size_t matrixIndex =
                frameSlotIndex * createInfo.swapchainImages.size() + imageIndex;
            const VkCommandBuffer commandBuffer = resources.m_commandBuffers[matrixIndex];
            const VkCommandBufferBeginInfo beginInfo{
                VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                nullptr,
                0u,
                nullptr,
            };
            vulkanResult = vkBeginCommandBuffer(commandBuffer, &beginInfo);
            if (vulkanResult != VK_SUCCESS) {
                return std::unexpected(creationFailure(vulkanResult));
            }

            const VkImageMemoryBarrier hostImageBarrier{
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                nullptr,
                VK_ACCESS_MEMORY_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                createInfo.hostImage,
                g_colorSubresourceRange,
            };
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                    | VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 0u, nullptr, 0u, nullptr,
                1u, &hostImageBarrier);

            const VkImageMemoryBarrier colorAttachmentBarrier{
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                nullptr,
                0u,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                createInfo.swapchainImages[imageIndex],
                g_colorSubresourceRange,
            };
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 0u, 0u, nullptr, 0u, nullptr, 1u,
                                 &colorAttachmentBarrier);

            resources.m_compositionResources->recordCommands(
                commandBuffer, createInfo.swapchainImageViews[imageIndex]);

            const VkImageMemoryBarrier presentBarrier{
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                nullptr,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                0u,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                createInfo.swapchainImages[imageIndex],
                g_colorSubresourceRange,
            };
            vkCmdPipelineBarrier(commandBuffer,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                 0u, 0u, nullptr, 0u, nullptr, 1u, &presentBarrier);
            vulkanResult = vkEndCommandBuffer(commandBuffer);
            if (vulkanResult != VK_SUCCESS) {
                return std::unexpected(creationFailure(vulkanResult));
            }
        }
    }

    return resources;
}

VulkanHostImagePresentationResources::VulkanHostImagePresentationResources(
    VulkanHostImagePresentationResources&& movedFrom) noexcept
    : m_logicalDevice(std::exchange(movedFrom.m_logicalDevice, VK_NULL_HANDLE)),
      m_compositionResources(std::move(movedFrom.m_compositionResources)),
      m_commandPool(std::exchange(movedFrom.m_commandPool, VK_NULL_HANDLE)),
      m_commandBuffers(std::move(movedFrom.m_commandBuffers)),
      m_swapchainImageCount(std::exchange(movedFrom.m_swapchainImageCount, 0u)) {
    movedFrom.m_compositionResources.reset();
}

VulkanHostImagePresentationResources::~VulkanHostImagePresentationResources() {
    destroyOwnedObjects();
}

VkCommandBuffer VulkanHostImagePresentationResources::commandBuffer(
    std::uint32_t frameSlotIndex, std::uint32_t imageIndex) const noexcept {
    assert(m_swapchainImageCount != 0u);
    assert(imageIndex < m_swapchainImageCount);
    const std::size_t matrixIndex = frameSlotIndex * m_swapchainImageCount + imageIndex;
    assert(matrixIndex < m_commandBuffers.size());
    return m_commandBuffers[matrixIndex];
}

std::expected<void, VkResult>
VulkanHostImagePresentationResources::destroyAfterSubmittedFrames(
    std::span<const VkFence> submittedFrameFences,
    VkFence unsubmittedOpenFrameFence) {
    if (m_logicalDevice == VK_NULL_HANDLE) {
        return {};
    }
    std::vector<VkFence> fencesToWait;
    fencesToWait.reserve(submittedFrameFences.size());
    std::ranges::copy_if(
        submittedFrameFences, std::back_inserter(fencesToWait),
        [unsubmittedOpenFrameFence](VkFence fence) {
            return fence != VK_NULL_HANDLE && fence != unsubmittedOpenFrameFence;
        });
    if (!fencesToWait.empty()) {
        const VkResult vulkanResult = vkWaitForFences(
            m_logicalDevice, static_cast<std::uint32_t>(fencesToWait.size()),
            fencesToWait.data(), VK_TRUE, std::numeric_limits<std::uint64_t>::max());
        if (vulkanResult != VK_SUCCESS) {
            return std::unexpected(vulkanResult);
        }
    }
    destroyOwnedObjects();
    return {};
}

void VulkanHostImagePresentationResources::destroyOwnedObjects() noexcept {
    if (m_logicalDevice == VK_NULL_HANDLE) {
        return;
    }
    if (m_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_logicalDevice, m_commandPool, nullptr);
    }
    m_compositionResources.reset();
    m_commandBuffers.clear();
    m_commandPool = VK_NULL_HANDLE;
    m_swapchainImageCount = 0u;
    m_logicalDevice = VK_NULL_HANDLE;
}

} // namespace barrieww
