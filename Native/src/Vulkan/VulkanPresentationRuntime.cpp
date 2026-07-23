#include "BarriEww/Vulkan/VulkanPresentationRuntime.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace barrieww {

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

    std::uint32_t actualImageCount = 0u;
    vulkanResult = vkGetSwapchainImagesKHR(
        createInfo.logicalDevice, swapchain, &actualImageCount, nullptr);
    if (vulkanResult != VK_SUCCESS || actualImageCount == 0u) {
        vkDestroySwapchainKHR(createInfo.logicalDevice, swapchain, nullptr);
        return std::unexpected(CreationFailure{vulkanResult, actualImageCount == 0u});
    }
    std::vector<VkImage> images(actualImageCount);
    vulkanResult = vkGetSwapchainImagesKHR(
        createInfo.logicalDevice, swapchain, &actualImageCount, images.data());
    if (vulkanResult != VK_SUCCESS) {
        vkDestroySwapchainKHR(createInfo.logicalDevice, swapchain, nullptr);
        return std::unexpected(CreationFailure{vulkanResult, false});
    }

    std::vector<VkImageView> imageViews;
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
            for (VkImageView createdView : imageViews) {
                vkDestroyImageView(createInfo.logicalDevice, createdView, nullptr);
            }
            vkDestroySwapchainKHR(createInfo.logicalDevice, swapchain, nullptr);
            return std::unexpected(CreationFailure{vulkanResult, false});
        }
        imageViews.push_back(imageView);
    }

    return VulkanPresentationRuntime{
        createInfo.logicalDevice, createInfo.graphicsQueue, createInfo.presentQueue,
        createInfo.surface, swapchain, surfaceFormat, presentMode, sharingMode,
        std::move(images), std::move(imageViews)};
}

VulkanPresentationRuntime::VulkanPresentationRuntime(
    VkDevice logicalDevice, VkQueue graphicsQueue, VkQueue presentQueue,
    VkSurfaceKHR surface, VkSwapchainKHR swapchain, VkSurfaceFormatKHR surfaceFormat,
    VkPresentModeKHR presentMode, VkSharingMode sharingMode,
    std::vector<VkImage> images, std::vector<VkImageView> imageViews) noexcept
    : m_logicalDevice(logicalDevice)
    , m_graphicsQueue(graphicsQueue)
    , m_presentQueue(presentQueue)
    , m_surface(surface)
    , m_swapchain(swapchain)
    , m_surfaceFormat(surfaceFormat)
    , m_presentMode(presentMode)
    , m_sharingMode(sharingMode)
    , m_images(std::move(images))
    , m_imageViews(std::move(imageViews)) {}

VulkanPresentationRuntime::VulkanPresentationRuntime(
    VulkanPresentationRuntime&& movedFrom) noexcept
    : m_logicalDevice(movedFrom.m_logicalDevice)
    , m_graphicsQueue(movedFrom.m_graphicsQueue)
    , m_presentQueue(movedFrom.m_presentQueue)
    , m_surface(movedFrom.m_surface)
    , m_swapchain(movedFrom.m_swapchain)
    , m_surfaceFormat(movedFrom.m_surfaceFormat)
    , m_presentMode(movedFrom.m_presentMode)
    , m_sharingMode(movedFrom.m_sharingMode)
    , m_images(std::move(movedFrom.m_images))
    , m_imageViews(std::move(movedFrom.m_imageViews)) {
    movedFrom.m_swapchain = VK_NULL_HANDLE;
    movedFrom.m_imageViews.clear();
    movedFrom.m_images.clear();
}

VulkanPresentationRuntime::~VulkanPresentationRuntime() {
    if (m_swapchain == VK_NULL_HANDLE) {
        return;
    }
    vkQueueWaitIdle(m_graphicsQueue);
    if (m_presentQueue != m_graphicsQueue) {
        vkQueueWaitIdle(m_presentQueue);
    }
    for (VkImageView imageView : m_imageViews) {
        vkDestroyImageView(m_logicalDevice, imageView, nullptr);
    }
    vkDestroySwapchainKHR(m_logicalDevice, m_swapchain, nullptr);
}

} // namespace barrieww
