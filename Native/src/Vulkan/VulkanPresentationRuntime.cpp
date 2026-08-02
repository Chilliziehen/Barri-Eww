#include "BarriEww/Vulkan/VulkanPresentationRuntime.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <utility>
#include <vector>

#if defined(BARRIEWW_PRESENTATION_RUNTIME_TEST_SEAM)
    #include "VulkanPresentationRuntimeTestSeam.hpp"
#endif

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
    return createSwapchainRuntime(createInfo, VK_FORMAT_UNDEFINED,
                                  VK_IMAGE_USAGE_TRANSFER_DST_BIT, false, false);
}

std::expected<VulkanPresentationRuntime, VulkanPresentationRuntime::CreationFailure>
VulkanPresentationRuntime::createHostImagePresentation(
    const HostImageCreateInfo& createInfo) {
    const CreateInfo& presentationCreateInfo = createInfo.presentationCreateInfo;
    const bool isKnownHostImageFormat =
        createInfo.hostImageFormat == VK_FORMAT_R8G8B8A8_UNORM
        || createInfo.hostImageFormat == VK_FORMAT_B8G8R8A8_UNORM;
    const bool isKnownSurfaceFormat =
        createInfo.requestedSurfaceFormat == VK_FORMAT_R8G8B8A8_UNORM
        || createInfo.requestedSurfaceFormat == VK_FORMAT_B8G8R8A8_UNORM;
    if (presentationCreateInfo.physicalDevice == VK_NULL_HANDLE
        || presentationCreateInfo.logicalDevice == VK_NULL_HANDLE
        || presentationCreateInfo.surface == VK_NULL_HANDLE
        || presentationCreateInfo.graphicsQueue == VK_NULL_HANDLE
        || presentationCreateInfo.presentQueue == VK_NULL_HANDLE
        || createInfo.hostImage == VK_NULL_HANDLE || !isKnownHostImageFormat
        || !isKnownSurfaceFormat || presentationCreateInfo.framebufferWidth == 0u
        || presentationCreateInfo.framebufferHeight == 0u
        || presentationCreateInfo.framesInFlightCount == 0u
        || createInfo.hostImageWidth == 0u || createInfo.hostImageHeight == 0u) {
        return std::unexpected(CreationFailure{VK_ERROR_INITIALIZATION_FAILED, false});
    }
    if (createInfo.hostImageWidth != presentationCreateInfo.framebufferWidth
        || createInfo.hostImageHeight != presentationCreateInfo.framebufferHeight) {
        return std::unexpected(CreationFailure{VK_SUCCESS, true});
    }

    auto runtimeResult = createSwapchainRuntime(
        presentationCreateInfo, createInfo.requestedSurfaceFormat,
        VulkanHostImagePresentationResources::s_requiredSwapchainImageUsageFlags,
        true, true);
    if (!runtimeResult.has_value()) {
        return runtimeResult;
    }

    VulkanHostImagePresentationResources::CreateInfo hostResourceCreateInfo{
        presentationCreateInfo.logicalDevice,
        presentationCreateInfo.graphicsQueueFamilyIndex,
        createInfo.hostImage,
        createInfo.hostImageFormat,
        runtimeResult->m_surfaceFormat.format,
        VkExtent2D{presentationCreateInfo.framebufferWidth,
                   presentationCreateInfo.framebufferHeight},
        runtimeResult->m_images,
        runtimeResult->m_imageViews,
        presentationCreateInfo.framesInFlightCount,
    };
    auto hostResourceResult =
        VulkanHostImagePresentationResources::create(hostResourceCreateInfo);
    if (!hostResourceResult.has_value()) {
        return std::unexpected(
            CreationFailure{hostResourceResult.error().vulkanResult, false});
    }
    runtimeResult->m_hostImagePresentationResources.emplace(
        std::move(hostResourceResult.value()));
    return std::move(runtimeResult.value());
}

std::expected<VulkanPresentationRuntime, VulkanPresentationRuntime::CreationFailure>
VulkanPresentationRuntime::createSwapchainRuntime(
    const CreateInfo& createInfo, VkFormat requiredSurfaceFormat,
    VkImageUsageFlags requiredImageUsageFlags, bool requiresExactExtent,
    bool preservesExactCreationFailures) {
    /**
     * @note ThreadSafety: Creation is presentation-thread confined and publishes no handle
     *       until every swapchain, view, synchronization, and clear-path object exists.
     * @brief Applies one explicit policy to common swapchain construction. Legacy creation
     *        supplies undefined format, transfer-only usage, clamped extent, and historical
     *        failure mapping. Host creation supplies exact UNORM format, combined usage,
     *        exact extent, and raw failure preservation. This keeps the old path's observable
     *        choices unchanged while sharing transactional ownership mechanics.
     *
     * Semantic pseudocode:
     * validate framebuffer and required surface usage
     * enumerate formats and select legacy preference or exact requested format
     * enumerate present modes and select MAILBOX else FIFO
     * calculate surface extent; reject mismatch when exact extent is required
     * select OPAQUE else PRE_MULTIPLIED else POST_MULTIPLIED else INHERIT composite alpha
     * reject the surface when no standard composite alpha is supported
     * create swapchain with the supplied usage policy
     * transfer swapchain immediately into the partial rollback owner
     * create every swapchain view and frame synchronization object into that owner
     * create the clear command pool and command buffers into that owner
     * preallocate image-fence ownership state while rollback remains active
     * move every array into a completed runtime through its non-allocating constructor
     * release scalar rollback ownership only after runtime construction completes
     * on any Vulkan or C++ failure, rollback destroys completed objects in reverse order
     *
     * The algorithm is O(surfaceFormatCount + presentModeCount + swapchainImageCount +
     * framesInFlightCount) time and O(the same counts) temporary/owned storage.
     */
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
    if ((surfaceCapabilities.supportedUsageFlags & requiredImageUsageFlags)
        != requiredImageUsageFlags) {
        return std::unexpected(CreationFailure{VK_SUCCESS, true});
    }

    std::uint32_t surfaceFormatCount = 0u;
    vulkanResult = vkGetPhysicalDeviceSurfaceFormatsKHR(
        createInfo.physicalDevice, createInfo.surface, &surfaceFormatCount, nullptr);
    if (vulkanResult != VK_SUCCESS) {
        return std::unexpected(CreationFailure{
            vulkanResult,
            preservesExactCreationFailures ? false : surfaceFormatCount == 0u});
    }
    if (surfaceFormatCount == 0u) {
        return std::unexpected(CreationFailure{VK_SUCCESS, true});
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
    auto selectedFormat = requiredSurfaceFormat == VK_FORMAT_UNDEFINED
        ? selectFormat(VK_FORMAT_B8G8R8A8_SRGB)
        : selectFormat(requiredSurfaceFormat);
    if (requiredSurfaceFormat == VK_FORMAT_UNDEFINED
        && selectedFormat == surfaceFormats.end()) {
        selectedFormat = selectFormat(VK_FORMAT_R8G8B8A8_SRGB);
    }
    if (requiredSurfaceFormat != VK_FORMAT_UNDEFINED
        && selectedFormat == surfaceFormats.end()) {
        return std::unexpected(CreationFailure{VK_SUCCESS, true});
    }
    const VkSurfaceFormatKHR surfaceFormat = selectedFormat == surfaceFormats.end()
        ? surfaceFormats.front()
        : *selectedFormat;

    std::uint32_t presentModeCount = 0u;
    vulkanResult = vkGetPhysicalDeviceSurfacePresentModesKHR(
        createInfo.physicalDevice, createInfo.surface, &presentModeCount, nullptr);
    if (vulkanResult != VK_SUCCESS) {
        return std::unexpected(CreationFailure{
            vulkanResult,
            preservesExactCreationFailures ? false : presentModeCount == 0u});
    }
    if (presentModeCount == 0u) {
        return std::unexpected(CreationFailure{VK_SUCCESS, true});
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
    if (requiresExactExtent
        && (extent.width != createInfo.framebufferWidth
            || extent.height != createInfo.framebufferHeight)) {
        return std::unexpected(CreationFailure{VK_SUCCESS, true});
    }
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
    constexpr std::array compositeAlphaPreference{
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };
    const auto selectedCompositeAlpha = std::ranges::find_if(
        compositeAlphaPreference,
        [&surfaceCapabilities](VkCompositeAlphaFlagBitsKHR candidate) {
            return (surfaceCapabilities.supportedCompositeAlpha & candidate) != 0u;
        });
    if (selectedCompositeAlpha == compositeAlphaPreference.end()) {
        return std::unexpected(CreationFailure{VK_SUCCESS, true});
    }

    VkSwapchainCreateInfoKHR swapchainCreateInfo{};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = createInfo.surface;
    swapchainCreateInfo.minImageCount = imageCount;
    swapchainCreateInfo.imageFormat = surfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainCreateInfo.imageExtent = extent;
    swapchainCreateInfo.imageArrayLayers = 1u;
    swapchainCreateInfo.imageUsage = requiredImageUsageFlags;
    swapchainCreateInfo.imageSharingMode = sharingMode;
    swapchainCreateInfo.queueFamilyIndexCount = splitQueueFamilies ? 2u : 0u;
    swapchainCreateInfo.pQueueFamilyIndices =
        splitQueueFamilies ? queueFamilyIndices.data() : nullptr;
    swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
    swapchainCreateInfo.compositeAlpha = *selectedCompositeAlpha;
    swapchainCreateInfo.presentMode = presentMode;
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    vulkanResult = vkCreateSwapchainKHR(
        createInfo.logicalDevice, &swapchainCreateInfo, nullptr, &swapchain);
    if (vulkanResult != VK_SUCCESS) {
        return std::unexpected(CreationFailure{vulkanResult, false});
    }
    SwapchainCreationRollback rollback{createInfo.logicalDevice, swapchain};
#if defined(BARRIEWW_PRESENTATION_RUNTIME_TEST_SEAM)
    testing::reachVulkanPresentationRuntimeCreationCheckpoint(
        testing::VulkanPresentationRuntimeCreationCheckpoint::AfterSwapchainCreation);
#endif

    std::uint32_t actualImageCount = 0u;
    vulkanResult = vkGetSwapchainImagesKHR(
        createInfo.logicalDevice, swapchain, &actualImageCount, nullptr);
    if (vulkanResult != VK_SUCCESS) {
        return std::unexpected(CreationFailure{
            vulkanResult,
            preservesExactCreationFailures ? false : actualImageCount == 0u});
    }
    if (actualImageCount == 0u) {
        return std::unexpected(CreationFailure{VK_SUCCESS, true});
    }
    rollback.m_images.resize(actualImageCount);
    vulkanResult = vkGetSwapchainImagesKHR(
        createInfo.logicalDevice, swapchain, &actualImageCount, rollback.m_images.data());
    if (vulkanResult != VK_SUCCESS) {
        return std::unexpected(CreationFailure{vulkanResult, false});
    }

    rollback.m_imageViews.resize(rollback.m_images.size(), VK_NULL_HANDLE);
    for (std::size_t imageIndex = 0u; imageIndex < rollback.m_images.size(); ++imageIndex) {
        VkImageViewCreateInfo imageViewCreateInfo{};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.image = rollback.m_images[imageIndex];
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = surfaceFormat.format;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.levelCount = 1u;
        imageViewCreateInfo.subresourceRange.layerCount = 1u;
        vulkanResult = vkCreateImageView(
            createInfo.logicalDevice, &imageViewCreateInfo, nullptr,
            &rollback.m_imageViews[imageIndex]);
        if (vulkanResult != VK_SUCCESS) {
            return std::unexpected(CreationFailure{vulkanResult, false});
        }
    }

    rollback.m_frameSlots.resize(createInfo.framesInFlightCount);
    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (std::size_t frameSlotIndex = 0u;
         frameSlotIndex < rollback.m_frameSlots.size(); ++frameSlotIndex) {
        FrameSlot& frameSlot = rollback.m_frameSlots[frameSlotIndex];
        vulkanResult = vkCreateSemaphore(createInfo.logicalDevice, &semaphoreCreateInfo,
                                         nullptr, &frameSlot.imageAvailableSemaphore);
        if (vulkanResult == VK_SUCCESS) {
            vulkanResult = vkCreateSemaphore(createInfo.logicalDevice, &semaphoreCreateInfo,
                                             nullptr, &frameSlot.renderFinishedSemaphore);
        }
        if (vulkanResult == VK_SUCCESS) {
            vulkanResult = vkCreateFence(createInfo.logicalDevice, &fenceCreateInfo, nullptr,
                                         &frameSlot.inFlightFence);
        }
        if (vulkanResult != VK_SUCCESS) {
            const VkResult reportedResult = preservesExactCreationFailures
                ? vulkanResult
                : VK_ERROR_OUT_OF_DEVICE_MEMORY;
            return std::unexpected(CreationFailure{reportedResult, false});
        }
#if defined(BARRIEWW_PRESENTATION_RUNTIME_TEST_SEAM)
        if (frameSlotIndex == 0u) {
            testing::reachVulkanPresentationRuntimeCreationCheckpoint(
                testing::VulkanPresentationRuntimeCreationCheckpoint::
                    AfterFirstFrameSlotCreation);
        }
#endif
    }

    VkCommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = createInfo.graphicsQueueFamilyIndex;
    vulkanResult = vkCreateCommandPool(createInfo.logicalDevice, &commandPoolCreateInfo,
                                       nullptr, &rollback.m_commandPool);
    if (vulkanResult != VK_SUCCESS) {
        const VkResult reportedResult = preservesExactCreationFailures
            ? vulkanResult
            : VK_ERROR_OUT_OF_DEVICE_MEMORY;
        return std::unexpected(CreationFailure{reportedResult, false});
    }
#if defined(BARRIEWW_PRESENTATION_RUNTIME_TEST_SEAM)
    testing::reachVulkanPresentationRuntimeCreationCheckpoint(
        testing::VulkanPresentationRuntimeCreationCheckpoint::AfterCommandPoolCreation);
#endif
    rollback.m_frameCommandBuffers.resize(createInfo.framesInFlightCount, VK_NULL_HANDLE);
    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = rollback.m_commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = createInfo.framesInFlightCount;
    vulkanResult = vkAllocateCommandBuffers(createInfo.logicalDevice,
                                            &commandBufferAllocateInfo,
                                            rollback.m_frameCommandBuffers.data());
    if (vulkanResult != VK_SUCCESS) {
        const VkResult reportedResult = preservesExactCreationFailures
            ? vulkanResult
            : VK_ERROR_OUT_OF_DEVICE_MEMORY;
        return std::unexpected(CreationFailure{reportedResult, false});
    }

    rollback.m_imageInFlightFences.resize(rollback.m_images.size(), VK_NULL_HANDLE);
    VulkanPresentationRuntime runtime{
        createInfo.logicalDevice, createInfo.graphicsQueue, createInfo.presentQueue,
        createInfo.surface, rollback.m_swapchain, rollback.m_commandPool, surfaceFormat,
        presentMode, sharingMode, std::move(rollback.m_images),
        std::move(rollback.m_imageViews), std::move(rollback.m_imageInFlightFences),
        std::move(rollback.m_frameCommandBuffers), std::move(rollback.m_frameSlots)};
    rollback.m_swapchain = VK_NULL_HANDLE;
    rollback.m_commandPool = VK_NULL_HANDLE;
    return runtime;
}

VulkanPresentationRuntime::SwapchainCreationRollback::SwapchainCreationRollback(
    VkDevice logicalDevice, VkSwapchainKHR swapchain) noexcept
    : m_logicalDevice(logicalDevice), m_swapchain(swapchain) {}

VulkanPresentationRuntime::SwapchainCreationRollback::~SwapchainCreationRollback() {
    if (m_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_logicalDevice, m_commandPool, nullptr);
    }
    for (auto frameSlotIterator = m_frameSlots.rbegin();
         frameSlotIterator != m_frameSlots.rend(); ++frameSlotIterator) {
        const FrameSlot& frameSlot = *frameSlotIterator;
        if (frameSlot.inFlightFence != VK_NULL_HANDLE) {
            vkDestroyFence(m_logicalDevice, frameSlot.inFlightFence, nullptr);
        }
        if (frameSlot.renderFinishedSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_logicalDevice, frameSlot.renderFinishedSemaphore, nullptr);
        }
        if (frameSlot.imageAvailableSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_logicalDevice, frameSlot.imageAvailableSemaphore, nullptr);
        }
    }
    for (auto imageViewIterator = m_imageViews.rbegin();
         imageViewIterator != m_imageViews.rend(); ++imageViewIterator) {
        const VkImageView imageView = *imageViewIterator;
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_logicalDevice, imageView, nullptr);
        }
    }
    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_logicalDevice, m_swapchain, nullptr);
    }
}

VulkanPresentationRuntime::VulkanPresentationRuntime(
    VkDevice logicalDevice, VkQueue graphicsQueue, VkQueue presentQueue,
    VkSurfaceKHR surface, VkSwapchainKHR swapchain, VkCommandPool commandPool,
    VkSurfaceFormatKHR surfaceFormat, VkPresentModeKHR presentMode,
    VkSharingMode sharingMode, std::vector<VkImage> images,
    std::vector<VkImageView> imageViews,
    std::vector<VkFence> imageInFlightFences,
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
    , m_imageInFlightFences(std::move(imageInFlightFences))
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
    , m_hostImagePresentationResources(
          std::move(movedFrom.m_hostImagePresentationResources))
    , m_swapchainGeneration(movedFrom.m_swapchainGeneration)
    , m_frameSequence(movedFrom.m_frameSequence)
    , m_currentFrameSlot(movedFrom.m_currentFrameSlot)
    , m_acquiredImageIndex(movedFrom.m_acquiredImageIndex)
    , m_isFrameOpen(movedFrom.m_isFrameOpen)
    , m_isRecreationRequired(movedFrom.m_isRecreationRequired)
    , m_recreationVulkanResult(movedFrom.m_recreationVulkanResult)
    , m_openFrameMetrics(movedFrom.m_openFrameMetrics)
    , m_frameStartTimePoint(movedFrom.m_frameStartTimePoint) {
    movedFrom.m_swapchain = VK_NULL_HANDLE;
    movedFrom.m_commandPool = VK_NULL_HANDLE;
    movedFrom.m_imageViews.clear();
    movedFrom.m_images.clear();
    movedFrom.m_frameSlots.clear();
    movedFrom.m_imageInFlightFences.clear();
    movedFrom.m_frameCommandBuffers.clear();
    movedFrom.m_hostImagePresentationResources.reset();
    movedFrom.m_isFrameOpen = false;
    movedFrom.m_isRecreationRequired = false;
    movedFrom.m_recreationVulkanResult = VK_SUCCESS;
}

void VulkanPresentationRuntime::destroyOwnedObjects() noexcept {
    if (m_swapchain == VK_NULL_HANDLE) {
        return;
    }
    vkQueueWaitIdle(m_graphicsQueue);
    if (m_presentQueue != m_graphicsQueue) {
        vkQueueWaitIdle(m_presentQueue);
    }
    m_hostImagePresentationResources.reset();
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
    if (m_isRecreationRequired) {
        beginResult.status = FrameStatus::RecreateRequired;
        beginResult.vulkanResult = m_recreationVulkanResult;
        return beginResult;
    }
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
    frameSlot.hasSubmittedHostImageResources = false;
    m_openFrameMetrics = FrameMetrics{};
    m_openFrameMetrics.fenceWaitNanoseconds = elapsedNanosecondsSince(fenceWaitStart);

    const auto acquireStart = std::chrono::steady_clock::now();
    std::uint32_t imageIndex = 0u;
    const VkResult acquireResult = vkAcquireNextImageKHR(
        m_logicalDevice, m_swapchain, UINT64_MAX, frameSlot.imageAvailableSemaphore,
        VK_NULL_HANDLE, &imageIndex);
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        beginResult.status = FrameStatus::RecreateRequired;
        beginResult.vulkanResult = acquireResult;
        return beginResult;
    }
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        beginResult.status = FrameStatus::RecreateRequired;
        beginResult.vulkanResult = acquireResult;
        return beginResult;
    }
    m_openFrameMetrics.acquireNanoseconds = elapsedNanosecondsSince(acquireStart);

    /**
     * @note ThreadSafety: Frame access is presentation-thread confined.
     * @brief Commits acquired-image fence ownership only after every prerequisite transition
     *        succeeds. The slot fence was already waited, proving its prior submission done.
     *        A prior image fence, when present, is then waited before the slot fence is reset.
     *        Only a successful reset permits publishing the new image-to-slot association and
     *        opening the frame. Every post-acquire failure leaves the old image association
     *        intact, closes the frame, and makes the runtime recreate-only because the binary
     *        acquire semaphore is signaled and the acquired image remains outstanding.
     *
     * Semantic pseudocode:
     * if acquiredImage has priorFence:
     *     wait priorFence; on failure enter recreate-only state and return first raw result
     * reset currentSlotFence; on failure enter recreate-only state and return first raw result
     * imageInFlightFences[acquiredImage] = currentSlotFence
     * acquiredImageIndex = acquiredImage
     * frameOpen = true
     */
    if (m_imageInFlightFences[imageIndex] != VK_NULL_HANDLE) {
        vulkanResult = vkWaitForFences(m_logicalDevice, 1u,
                                       &m_imageInFlightFences[imageIndex], VK_TRUE,
                                       UINT64_MAX);
        if (vulkanResult != VK_SUCCESS) {
            m_isRecreationRequired = true;
            m_recreationVulkanResult = vulkanResult;
            m_isFrameOpen = false;
            beginResult.status = FrameStatus::RecreateRequired;
            beginResult.vulkanResult = vulkanResult;
            return beginResult;
        }
    }
    vulkanResult = vkResetFences(m_logicalDevice, 1u, &frameSlot.inFlightFence);
    if (vulkanResult != VK_SUCCESS) {
        m_isRecreationRequired = true;
        m_recreationVulkanResult = vulkanResult;
        m_isFrameOpen = false;
        beginResult.status = FrameStatus::RecreateRequired;
        beginResult.vulkanResult = vulkanResult;
        return beginResult;
    }
    m_imageInFlightFences[imageIndex] = frameSlot.inFlightFence;

    m_acquiredImageIndex = imageIndex;
    m_isFrameOpen = true;
    m_frameStartTimePoint = std::chrono::steady_clock::now();

    beginResult.status = acquireResult == VK_SUBOPTIMAL_KHR
        ? FrameStatus::Suboptimal
        : FrameStatus::Success;
    beginResult.imageIndex = imageIndex;
    beginResult.frameSequence = m_frameSequence;
    beginResult.vulkanResult = acquireResult;
    return beginResult;
}

VulkanPresentationRuntime::SubmitFrameResult
VulkanPresentationRuntime::submitAndPresentFrame(VkCommandBuffer commandBuffer) {
    return submitAndPresentFrameWithResourceTracking(commandBuffer, false);
}

VulkanPresentationRuntime::SubmitFrameResult
VulkanPresentationRuntime::submitAndPresentFrameWithResourceTracking(
    VkCommandBuffer commandBuffer, bool usesHostImageResources) {
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
    if (usesHostImageResources) {
        frameSlot.hasSubmittedHostImageResources = true;
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
VulkanPresentationRuntime::submitAndPresentHostImageFrame() {
    if (!m_isFrameOpen || !m_hostImagePresentationResources.has_value()) {
        SubmitFrameResult submitResult{};
        submitResult.vulkanResult = VK_NOT_READY;
        return submitResult;
    }
    const VkCommandBuffer commandBuffer =
        m_hostImagePresentationResources->commandBuffer(m_currentFrameSlot,
                                                        m_acquiredImageIndex);
    return submitAndPresentFrameWithResourceTracking(commandBuffer, true);
}

std::expected<void, VkResult>
VulkanPresentationRuntime::detachHostImagePresentationResources() {
    if (!m_hostImagePresentationResources.has_value()) {
        return {};
    }

    /**
     * @note ThreadSafety: Detach is render-thread confined, so frame-slot submission flags
     *       cannot change while the retirement set is built or waited.
     * @brief Builds the minimal fence retirement set from slots whose last successful submit
     *        referenced host resources. The current open slot is excluded by index because
     *        beginFrame reset its fence but has not submitted it; excluding by handle value
     *        could incorrectly omit another slot in a mock or aliased-handle environment.
     *
     * Semantic pseudocode:
     * submittedFrameFences = empty
     * for each frameSlotIndex and frameSlot:
     *     isOpenUnsubmitted = frameOpen and frameSlotIndex equals currentFrameSlot
     *     if frameSlot references host resources and not isOpenUnsubmitted:
     *         append frameSlot.inFlightFence
     * wait all submittedFrameFences through the host-resource owner
     * if wait fails: retain optional owner and every submission flag
     * otherwise: destroy/reset owner and clear all submission flags
     *
     * Retirement is O(framesInFlightCount) time and storage on the detach slow path.
     */
    std::vector<VkFence> submittedFrameFences;
    submittedFrameFences.reserve(m_frameSlots.size());
    for (std::size_t frameSlotIndex = 0u; frameSlotIndex < m_frameSlots.size();
         ++frameSlotIndex) {
        const FrameSlot& frameSlot = m_frameSlots[frameSlotIndex];
        const bool isResetUnsubmittedOpenSlot =
            m_isFrameOpen && frameSlotIndex == m_currentFrameSlot;
        if (frameSlot.hasSubmittedHostImageResources && !isResetUnsubmittedOpenSlot) {
            submittedFrameFences.push_back(frameSlot.inFlightFence);
        }
    }
    auto destructionResult =
        m_hostImagePresentationResources->destroyAfterSubmittedFrames(
            submittedFrameFences, VK_NULL_HANDLE);
    if (!destructionResult.has_value()) {
        return destructionResult;
    }
    m_hostImagePresentationResources.reset();
    for (FrameSlot& frameSlot : m_frameSlots) {
        frameSlot.hasSubmittedHostImageResources = false;
    }
    return {};
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
