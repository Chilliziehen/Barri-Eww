#include "BarriEww/Vulkan/VulkanPresentationRuntime.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <utility>
#include <vector>

namespace barrieww {

namespace {

/** Elapsed steady-clock nanoseconds since a start point. */
std::uint64_t elapsedNanosecondsSince(
    const std::chrono::steady_clock::time_point& startTimePoint) {
    const auto elapsed = std::chrono::steady_clock::now() - startTimePoint;
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
}

} // namespace

std::expected<VulkanPresentationRuntime, VulkanPresentationRuntime::CreationFailure>
VulkanPresentationRuntime::create(const CreateInfo& createInfo) {
    if (createInfo.framebufferWidth == 0u || createInfo.framebufferHeight == 0u
        || createInfo.framesInFlightCount == 0u) {
        return std::unexpected(CreationFailure{VK_SUCCESS, true});
    }

    VkSurfaceCapabilitiesKHR surfaceCapabilities{};
    VkResult vulkanResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        createInfo.physicalDevice, createInfo.surface, &surfaceCapabilities);
    if (vulkanResult != VK_SUCCESS) {
        return std::unexpected(CreationFailure{vulkanResult, false});
    }
    if ((surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0u) {
        return std::unexpected(CreationFailure{VK_SUCCESS, true});
    }

    std::uint32_t surfaceFormatCount = 0u;
    vulkanResult = vkGetPhysicalDeviceSurfaceFormatsKHR(
        createInfo.physicalDevice, createInfo.surface, &surfaceFormatCount, nullptr);
    if (vulkanResult != VK_SUCCESS || surfaceFormatCount == 0u) {
        return std::unexpected(CreationFailure{vulkanResult, surfaceFormatCount == 0u});
    }
    std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
    vulkanResult = vkGetPhysicalDeviceSurfaceFormatsKHR(
        createInfo.physicalDevice, createInfo.surface, &surfaceFormatCount,
        surfaceFormats.data());
    if (vulkanResult != VK_SUCCESS) {
        return std::unexpected(CreationFailure{vulkanResult, false});
    }

    const auto selectFormat = [&](VkFormat desiredFormat) {
        return std::ranges::find_if(surfaceFormats, [&](const VkSurfaceFormatKHR& candidate) {
            return candidate.format == desiredFormat
                   && candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        });
    };
    auto selectedFormat = selectFormat(VK_FORMAT_B8G8R8A8_SRGB);
    if (selectedFormat == surfaceFormats.end()) {
        selectedFormat = selectFormat(VK_FORMAT_R8G8B8A8_SRGB);
    }
    const VkSurfaceFormatKHR surfaceFormat = selectedFormat == surfaceFormats.end()
        ? surfaceFormats.front()
        : *selectedFormat;

    std::uint32_t presentModeCount = 0u;
    vulkanResult = vkGetPhysicalDeviceSurfacePresentModesKHR(
        createInfo.physicalDevice, createInfo.surface, &presentModeCount, nullptr);
    if (vulkanResult != VK_SUCCESS || presentModeCount == 0u) {
        return std::unexpected(CreationFailure{vulkanResult, presentModeCount == 0u});
    }
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vulkanResult = vkGetPhysicalDeviceSurfacePresentModesKHR(
        createInfo.physicalDevice, createInfo.surface, &presentModeCount,
        presentModes.data());
    if (vulkanResult != VK_SUCCESS) {
        return std::unexpected(CreationFailure{vulkanResult, false});
    }
    const VkPresentModeKHR presentMode =
        std::ranges::find(presentModes, VK_PRESENT_MODE_MAILBOX_KHR) != presentModes.end()
            ? VK_PRESENT_MODE_MAILBOX_KHR
            : VK_PRESENT_MODE_FIFO_KHR;

    const VkExtent2D extent = surfaceCapabilities.currentExtent.width != UINT32_MAX
        ? surfaceCapabilities.currentExtent
        : VkExtent2D{
              std::clamp(createInfo.framebufferWidth,
                         surfaceCapabilities.minImageExtent.width,
                         surfaceCapabilities.maxImageExtent.width),
              std::clamp(createInfo.framebufferHeight,
                         surfaceCapabilities.minImageExtent.height,
                         surfaceCapabilities.maxImageExtent.height)};
    std::uint32_t imageCount = surfaceCapabilities.minImageCount + 1u;
    if (surfaceCapabilities.maxImageCount != 0u) {
        imageCount = std::min(imageCount, surfaceCapabilities.maxImageCount);
    }

    const std::array queueFamilyIndices{
        createInfo.graphicsQueueFamilyIndex, createInfo.presentQueueFamilyIndex};
    const bool splitQueueFamilies =
        createInfo.graphicsQueueFamilyIndex != createInfo.presentQueueFamilyIndex;
    const VkSharingMode sharingMode = splitQueueFamilies
        ? VK_SHARING_MODE_CONCURRENT
        : VK_SHARING_MODE_EXCLUSIVE;

    VkSwapchainCreateInfoKHR swapchainCreateInfo{};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = createInfo.surface;
    swapchainCreateInfo.minImageCount = imageCount;
    swapchainCreateInfo.imageFormat = surfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainCreateInfo.imageExtent = extent;
    swapchainCreateInfo.imageArrayLayers = 1u;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainCreateInfo.imageSharingMode = sharingMode;
    swapchainCreateInfo.queueFamilyIndexCount = splitQueueFamilies ? 2u : 0u;
    swapchainCreateInfo.pQueueFamilyIndices =
        splitQueueFamilies ? queueFamilyIndices.data() : nullptr;
    swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.presentMode = presentMode;
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    vulkanResult = vkCreateSwapchainKHR(
        createInfo.logicalDevice, &swapchainCreateInfo, nullptr, &swapchain);
    if (vulkanResult != VK_SUCCESS) {
        return std::unexpected(CreationFailure{vulkanResult, false});
    }

    const auto failAfterSwapchain = [&](std::vector<VkImageView>& imageViews,
                                        std::vector<FrameSlot>& frameSlots,
                                        VkResult failureResult, bool unsupportedSurface) {
        for (const FrameSlot& frameSlot : frameSlots) {
            if (frameSlot.imageAvailableSemaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(createInfo.logicalDevice,
                                   frameSlot.imageAvailableSemaphore, nullptr);
            }
            if (frameSlot.renderFinishedSemaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(createInfo.logicalDevice,
                                   frameSlot.renderFinishedSemaphore, nullptr);
            }
            if (frameSlot.inFlightFence != VK_NULL_HANDLE) {
                vkDestroyFence(createInfo.logicalDevice, frameSlot.inFlightFence, nullptr);
            }
        }
        for (VkImageView createdView : imageViews) {
            vkDestroyImageView(createInfo.logicalDevice, createdView, nullptr);
        }
        vkDestroySwapchainKHR(createInfo.logicalDevice, swapchain, nullptr);
        return std::unexpected(CreationFailure{failureResult, unsupportedSurface});
    };

    std::uint32_t actualImageCount = 0u;
    std::vector<VkImageView> imageViews;
    std::vector<FrameSlot> frameSlots;
    vulkanResult = vkGetSwapchainImagesKHR(
        createInfo.logicalDevice, swapchain, &actualImageCount, nullptr);
    if (vulkanResult != VK_SUCCESS || actualImageCount == 0u) {
        return failAfterSwapchain(imageViews, frameSlots, vulkanResult,
                                  actualImageCount == 0u);
    }
    std::vector<VkImage> images(actualImageCount);
    vulkanResult = vkGetSwapchainImagesKHR(
        createInfo.logicalDevice, swapchain, &actualImageCount, images.data());
    if (vulkanResult != VK_SUCCESS) {
        return failAfterSwapchain(imageViews, frameSlots, vulkanResult, false);
    }

    imageViews.reserve(images.size());
    for (VkImage image : images) {
        VkImageViewCreateInfo imageViewCreateInfo{};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.image = image;
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = surfaceFormat.format;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.levelCount = 1u;
        imageViewCreateInfo.subresourceRange.layerCount = 1u;
        VkImageView imageView = VK_NULL_HANDLE;
        vulkanResult = vkCreateImageView(
            createInfo.logicalDevice, &imageViewCreateInfo, nullptr, &imageView);
        if (vulkanResult != VK_SUCCESS) {
            return failAfterSwapchain(imageViews, frameSlots, vulkanResult, false);
        }
        imageViews.push_back(imageView);
    }

    frameSlots.resize(createInfo.framesInFlightCount);
    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (FrameSlot& frameSlot : frameSlots) {
        if (vkCreateSemaphore(createInfo.logicalDevice, &semaphoreCreateInfo, nullptr,
                              &frameSlot.imageAvailableSemaphore) != VK_SUCCESS
            || vkCreateSemaphore(createInfo.logicalDevice, &semaphoreCreateInfo, nullptr,
                                 &frameSlot.renderFinishedSemaphore) != VK_SUCCESS
            || vkCreateFence(createInfo.logicalDevice, &fenceCreateInfo, nullptr,
                             &frameSlot.inFlightFence) != VK_SUCCESS) {
            return failAfterSwapchain(imageViews, frameSlots, VK_ERROR_OUT_OF_DEVICE_MEMORY,
                                      false);
        }
    }

    VkCommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = createInfo.graphicsQueueFamilyIndex;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    if (vkCreateCommandPool(createInfo.logicalDevice, &commandPoolCreateInfo, nullptr,
                            &commandPool) != VK_SUCCESS) {
        return failAfterSwapchain(imageViews, frameSlots, VK_ERROR_OUT_OF_DEVICE_MEMORY,
                                  false);
    }
    std::vector<VkCommandBuffer> frameCommandBuffers(createInfo.framesInFlightCount,
                                                     VK_NULL_HANDLE);
    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = createInfo.framesInFlightCount;
    if (vkAllocateCommandBuffers(createInfo.logicalDevice, &commandBufferAllocateInfo,
                                 frameCommandBuffers.data()) != VK_SUCCESS) {
        vkDestroyCommandPool(createInfo.logicalDevice, commandPool, nullptr);
        return failAfterSwapchain(imageViews, frameSlots, VK_ERROR_OUT_OF_DEVICE_MEMORY,
                                  false);
    }

    return VulkanPresentationRuntime{
        createInfo.logicalDevice, createInfo.graphicsQueue, createInfo.presentQueue,
        createInfo.surface, swapchain, commandPool, surfaceFormat, presentMode, sharingMode,
        std::move(images), std::move(imageViews), std::move(frameCommandBuffers),
        std::move(frameSlots)};
}

VulkanPresentationRuntime::VulkanPresentationRuntime(
    VkDevice logicalDevice, VkQueue graphicsQueue, VkQueue presentQueue,
    VkSurfaceKHR surface, VkSwapchainKHR swapchain, VkCommandPool commandPool,
    VkSurfaceFormatKHR surfaceFormat, VkPresentModeKHR presentMode,
    VkSharingMode sharingMode, std::vector<VkImage> images,
    std::vector<VkImageView> imageViews,
    std::vector<VkCommandBuffer> frameCommandBuffers,
    std::vector<FrameSlot> frameSlots) noexcept
    : m_logicalDevice(logicalDevice)
    , m_graphicsQueue(graphicsQueue)
    , m_presentQueue(presentQueue)
    , m_surface(surface)
    , m_swapchain(swapchain)
    , m_commandPool(commandPool)
    , m_surfaceFormat(surfaceFormat)
    , m_presentMode(presentMode)
    , m_sharingMode(sharingMode)
    , m_images(std::move(images))
    , m_imageViews(std::move(imageViews))
    , m_imageInFlightFences(m_images.size(), VK_NULL_HANDLE)
    , m_frameCommandBuffers(std::move(frameCommandBuffers))
    , m_frameSlots(std::move(frameSlots)) {}

VulkanPresentationRuntime::VulkanPresentationRuntime(
    VulkanPresentationRuntime&& movedFrom) noexcept
    : m_logicalDevice(movedFrom.m_logicalDevice)
    , m_graphicsQueue(movedFrom.m_graphicsQueue)
    , m_presentQueue(movedFrom.m_presentQueue)
    , m_surface(movedFrom.m_surface)
    , m_swapchain(movedFrom.m_swapchain)
    , m_commandPool(movedFrom.m_commandPool)
    , m_surfaceFormat(movedFrom.m_surfaceFormat)
    , m_presentMode(movedFrom.m_presentMode)
    , m_sharingMode(movedFrom.m_sharingMode)
    , m_images(std::move(movedFrom.m_images))
    , m_imageViews(std::move(movedFrom.m_imageViews))
    , m_imageInFlightFences(std::move(movedFrom.m_imageInFlightFences))
    , m_frameCommandBuffers(std::move(movedFrom.m_frameCommandBuffers))
    , m_frameSlots(std::move(movedFrom.m_frameSlots))
    , m_swapchainGeneration(movedFrom.m_swapchainGeneration)
    , m_frameSequence(movedFrom.m_frameSequence)
    , m_currentFrameSlot(movedFrom.m_currentFrameSlot)
    , m_acquiredImageIndex(movedFrom.m_acquiredImageIndex)
    , m_isFrameOpen(movedFrom.m_isFrameOpen) {
    movedFrom.m_swapchain = VK_NULL_HANDLE;
    movedFrom.m_commandPool = VK_NULL_HANDLE;
    movedFrom.m_imageViews.clear();
    movedFrom.m_images.clear();
    movedFrom.m_frameSlots.clear();
    movedFrom.m_imageInFlightFences.clear();
    movedFrom.m_frameCommandBuffers.clear();
}

void VulkanPresentationRuntime::destroyOwnedObjects() noexcept {
    if (m_swapchain == VK_NULL_HANDLE) {
        return;
    }
    vkQueueWaitIdle(m_graphicsQueue);
    if (m_presentQueue != m_graphicsQueue) {
        vkQueueWaitIdle(m_presentQueue);
    }
    for (const FrameSlot& frameSlot : m_frameSlots) {
        if (frameSlot.imageAvailableSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_logicalDevice, frameSlot.imageAvailableSemaphore, nullptr);
        }
        if (frameSlot.renderFinishedSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_logicalDevice, frameSlot.renderFinishedSemaphore, nullptr);
        }
        if (frameSlot.inFlightFence != VK_NULL_HANDLE) {
            vkDestroyFence(m_logicalDevice, frameSlot.inFlightFence, nullptr);
        }
    }
    if (m_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_logicalDevice, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }
    for (VkImageView imageView : m_imageViews) {
        vkDestroyImageView(m_logicalDevice, imageView, nullptr);
    }
    vkDestroySwapchainKHR(m_logicalDevice, m_swapchain, nullptr);
    m_swapchain = VK_NULL_HANDLE;
}

VulkanPresentationRuntime::~VulkanPresentationRuntime() {
    destroyOwnedObjects();
}

VulkanPresentationRuntime::BeginFrameResult VulkanPresentationRuntime::beginFrame(
    std::uint32_t framebufferWidth, std::uint32_t framebufferHeight) {
    BeginFrameResult beginResult{};
    beginResult.frameSlotIndex = m_currentFrameSlot;
    beginResult.swapchainGeneration = m_swapchainGeneration;
    if (framebufferWidth == 0u || framebufferHeight == 0u) {
        beginResult.status = FrameStatus::SurfaceUnavailable;
        return beginResult;
    }

    FrameSlot& frameSlot = m_frameSlots[m_currentFrameSlot];
    if (frameSlot.hasCompletedMetrics) {
        beginResult.priorMetricsValid = true;
        beginResult.priorMetrics = frameSlot.completedMetrics;
    }

    const auto fenceWaitStart = std::chrono::steady_clock::now();
    VkResult vulkanResult = vkWaitForFences(m_logicalDevice, 1u, &frameSlot.inFlightFence,
                                            VK_TRUE, UINT64_MAX);
    if (vulkanResult != VK_SUCCESS) {
        beginResult.status = FrameStatus::RecreateRequired;
        beginResult.vulkanResult = vulkanResult;
        return beginResult;
    }
    m_openFrameMetrics = FrameMetrics{};
    m_openFrameMetrics.fenceWaitNanoseconds = elapsedNanosecondsSince(fenceWaitStart);

    const auto acquireStart = std::chrono::steady_clock::now();
    std::uint32_t imageIndex = 0u;
    vulkanResult = vkAcquireNextImageKHR(m_logicalDevice, m_swapchain, UINT64_MAX,
                                         frameSlot.imageAvailableSemaphore, VK_NULL_HANDLE,
                                         &imageIndex);
    if (vulkanResult == VK_ERROR_OUT_OF_DATE_KHR) {
        beginResult.status = FrameStatus::RecreateRequired;
        beginResult.vulkanResult = vulkanResult;
        return beginResult;
    }
    if (vulkanResult != VK_SUCCESS && vulkanResult != VK_SUBOPTIMAL_KHR) {
        beginResult.status = FrameStatus::RecreateRequired;
        beginResult.vulkanResult = vulkanResult;
        return beginResult;
    }
    m_openFrameMetrics.acquireNanoseconds = elapsedNanosecondsSince(acquireStart);

    if (m_imageInFlightFences[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(m_logicalDevice, 1u, &m_imageInFlightFences[imageIndex], VK_TRUE,
                        UINT64_MAX);
    }
    m_imageInFlightFences[imageIndex] = frameSlot.inFlightFence;
    vkResetFences(m_logicalDevice, 1u, &frameSlot.inFlightFence);

    m_acquiredImageIndex = imageIndex;
    m_isFrameOpen = true;
    m_frameStartTimePoint = std::chrono::steady_clock::now();

    beginResult.status = vulkanResult == VK_SUBOPTIMAL_KHR
        ? FrameStatus::Suboptimal
        : FrameStatus::Success;
    beginResult.imageIndex = imageIndex;
    beginResult.frameSequence = m_frameSequence;
    beginResult.vulkanResult = vulkanResult;
    return beginResult;
}

VulkanPresentationRuntime::SubmitFrameResult
VulkanPresentationRuntime::submitAndPresentFrame(VkCommandBuffer commandBuffer) {
    SubmitFrameResult submitResult{};
    if (!m_isFrameOpen) {
        submitResult.vulkanResult = VK_NOT_READY;
        return submitResult;
    }

    FrameSlot& frameSlot = m_frameSlots[m_currentFrameSlot];
    const std::array<VkPipelineStageFlags, 1> waitStages{
        VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1u;
    submitInfo.pWaitSemaphores = &frameSlot.imageAvailableSemaphore;
    submitInfo.pWaitDstStageMask = waitStages.data();
    submitInfo.commandBufferCount = commandBuffer == VK_NULL_HANDLE ? 0u : 1u;
    submitInfo.pCommandBuffers = commandBuffer == VK_NULL_HANDLE ? nullptr : &commandBuffer;
    submitInfo.signalSemaphoreCount = 1u;
    submitInfo.pSignalSemaphores = &frameSlot.renderFinishedSemaphore;

    const auto submitStart = std::chrono::steady_clock::now();
    VkResult vulkanResult = vkQueueSubmit(m_graphicsQueue, 1u, &submitInfo,
                                          frameSlot.inFlightFence);
    m_openFrameMetrics.nativeSubmitCallNanoseconds = elapsedNanosecondsSince(submitStart);
    if (vulkanResult != VK_SUCCESS) {
        m_isFrameOpen = false;
        submitResult.status = FrameStatus::RecreateRequired;
        submitResult.vulkanResult = vulkanResult;
        return submitResult;
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1u;
    presentInfo.pWaitSemaphores = &frameSlot.renderFinishedSemaphore;
    presentInfo.swapchainCount = 1u;
    presentInfo.pSwapchains = &m_swapchain;
    presentInfo.pImageIndices = &m_acquiredImageIndex;

    const auto presentStart = std::chrono::steady_clock::now();
    const VkResult presentResult = vkQueuePresentKHR(m_presentQueue, &presentInfo);
    m_openFrameMetrics.presentCallNanoseconds = elapsedNanosecondsSince(presentStart);
    m_openFrameMetrics.totalCpuFrameNanoseconds =
        elapsedNanosecondsSince(m_frameStartTimePoint);
    m_openFrameMetrics.frameSequence = m_frameSequence;
    m_openFrameMetrics.swapchainGeneration = m_swapchainGeneration;
    m_openFrameMetrics.frameSlotIndex = m_currentFrameSlot;
    m_openFrameMetrics.imageIndex = m_acquiredImageIndex;
    m_openFrameMetrics.presentResultValue = static_cast<std::int32_t>(presentResult);
    m_openFrameMetrics.validFlags = s_cpuMetricsValidFlag;

    frameSlot.completedMetrics = m_openFrameMetrics;
    frameSlot.hasCompletedMetrics = true;

    m_isFrameOpen = false;
    m_currentFrameSlot = (m_currentFrameSlot + 1u) % framesInFlightCount();
    ++m_frameSequence;

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
        submitResult.status = FrameStatus::RecreateRequired;
    } else if (presentResult == VK_SUBOPTIMAL_KHR) {
        submitResult.status = FrameStatus::Suboptimal;
    } else if (presentResult != VK_SUCCESS) {
        submitResult.status = FrameStatus::RecreateRequired;
    }
    submitResult.vulkanResult = presentResult;
    return submitResult;
}

VulkanPresentationRuntime::SubmitFrameResult
VulkanPresentationRuntime::submitAndPresentClearFrame(const float clearColor[3]) {
    if (!m_isFrameOpen) {
        SubmitFrameResult submitResult{};
        submitResult.vulkanResult = VK_NOT_READY;
        return submitResult;
    }

    const VkCommandBuffer commandBuffer = m_frameCommandBuffers[m_currentFrameSlot];
    recordClearCommandBuffer(commandBuffer, m_images[m_acquiredImageIndex], clearColor);
    return submitAndPresentFrame(commandBuffer);
}

void VulkanPresentationRuntime::recordClearCommandBuffer(VkCommandBuffer commandBuffer,
                                                         VkImage swapchainImage,
                                                         const float clearColor[3]) {
    vkResetCommandBuffer(commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkImageSubresourceRange colorRange{};
    colorRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorRange.levelCount = 1u;
    colorRange.layerCount = 1u;

    VkImageMemoryBarrier toTransferDestination{};
    toTransferDestination.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransferDestination.srcAccessMask = 0;
    toTransferDestination.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toTransferDestination.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransferDestination.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransferDestination.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransferDestination.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransferDestination.image = swapchainImage;
    toTransferDestination.subresourceRange = colorRange;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &toTransferDestination);

    VkClearColorValue clearValue{};
    clearValue.float32[0] = clearColor[0];
    clearValue.float32[1] = clearColor[1];
    clearValue.float32[2] = clearColor[2];
    clearValue.float32[3] = 1.0f;
    vkCmdClearColorImage(commandBuffer, swapchainImage,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &colorRange);

    VkImageMemoryBarrier toPresent{};
    toPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toPresent.dstAccessMask = 0;
    toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    toPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toPresent.image = swapchainImage;
    toPresent.subresourceRange = colorRange;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &toPresent);

    vkEndCommandBuffer(commandBuffer);
}

VulkanPresentationRuntime::SubmitFrameResult VulkanPresentationRuntime::presentClearFrame(
    std::uint32_t framebufferWidth, std::uint32_t framebufferHeight,
    const float clearColor[3], BeginFrameResult& outBeginResult) {
    outBeginResult = beginFrame(framebufferWidth, framebufferHeight);
    if (outBeginResult.status != FrameStatus::Success
        && outBeginResult.status != FrameStatus::Suboptimal) {
        SubmitFrameResult submitResult{};
        submitResult.status = outBeginResult.status;
        submitResult.vulkanResult = outBeginResult.vulkanResult;
        return submitResult;
    }
    return submitAndPresentClearFrame(clearColor);
}

} // namespace barrieww
