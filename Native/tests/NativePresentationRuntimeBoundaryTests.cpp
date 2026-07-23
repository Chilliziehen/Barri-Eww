#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>

#include "BarriEww/Interoperability/NativePresentationRuntimeBoundary.hpp"

using barrieww::NativePresentationRuntimeCreateInfoVersion1;
using barrieww::NativePresentationRuntimeCreateResultVersion1;
using barrieww::NativePresentationRuntimeOperationResult;

namespace {

VkImageUsageFlags g_supportedUsageFlags = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
std::uint32_t g_minImageCount = 2u;
std::uint32_t g_maxImageCount = 0u;
bool g_offerMailboxPresentMode = true;
bool g_offerPreferredFormat = true;
std::uint32_t g_swapchainImageCount = 3u;
std::uint32_t g_createdImageViewCount = 0u;
std::uint32_t g_destroyedImageViewCount = 0u;
std::uint32_t g_destroyedSwapchainCount = 0u;
VkSharingMode g_observedSharingMode = VK_SHARING_MODE_EXCLUSIVE;
std::uint32_t g_observedQueueFamilyCount = 0u;

/** Resets deterministic WSI replacement state to the supported-surface baseline. */
void resetPresentationState() {
    g_supportedUsageFlags = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    g_minImageCount = 2u;
    g_maxImageCount = 0u;
    g_offerMailboxPresentMode = true;
    g_offerPreferredFormat = true;
    g_swapchainImageCount = 3u;
    g_createdImageViewCount = 0u;
    g_destroyedImageViewCount = 0u;
    g_destroyedSwapchainCount = 0u;
    g_observedSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    g_observedQueueFamilyCount = 0u;
}

/** Builds one valid same-family creation input over opaque non-null handles. */
NativePresentationRuntimeCreateInfoVersion1 makeCreateInfo() {
    NativePresentationRuntimeCreateInfoVersion1 createInfo{};
    createInfo.instanceHandle = 1u;
    createInfo.physicalDeviceHandle = 2u;
    createInfo.logicalDeviceHandle = 3u;
    createInfo.surfaceHandle = 4u;
    createInfo.graphicsQueueHandle = 5u;
    createInfo.presentQueueHandle = 5u;
    createInfo.graphicsQueueFamilyIndex = 0u;
    createInfo.presentQueueFamilyIndex = 0u;
    createInfo.framebufferWidth = 1280u;
    createInfo.framebufferHeight = 720u;
    createInfo.framesInFlightCount = 2u;
    createInfo.reservedFlags = 0u;
    return createInfo;
}

} // namespace

extern "C" VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
    VkSurfaceCapabilitiesKHR* surfaceCapabilities) {
    static_cast<void>(physicalDevice);
    static_cast<void>(surface);
    *surfaceCapabilities = {};
    surfaceCapabilities->minImageCount = g_minImageCount;
    surfaceCapabilities->maxImageCount = g_maxImageCount;
    surfaceCapabilities->currentExtent = VkExtent2D{1280u, 720u};
    surfaceCapabilities->minImageExtent = VkExtent2D{1u, 1u};
    surfaceCapabilities->maxImageExtent = VkExtent2D{4096u, 4096u};
    surfaceCapabilities->currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    surfaceCapabilities->supportedUsageFlags = g_supportedUsageFlags;
    return VK_SUCCESS;
}

extern "C" VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceFormatsKHR(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, std::uint32_t* formatCount,
    VkSurfaceFormatKHR* formats) {
    static_cast<void>(physicalDevice);
    static_cast<void>(surface);
    std::vector<VkSurfaceFormatKHR> availableFormats;
    if (g_offerPreferredFormat) {
        availableFormats.push_back(
            VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});
    }
    availableFormats.push_back(
        VkSurfaceFormatKHR{VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});
    if (formats == nullptr) {
        *formatCount = static_cast<std::uint32_t>(availableFormats.size());
        return VK_SUCCESS;
    }
    for (std::uint32_t formatIndex = 0u; formatIndex < *formatCount; ++formatIndex) {
        formats[formatIndex] = availableFormats[formatIndex];
    }
    return VK_SUCCESS;
}

extern "C" VkResult VKAPI_CALL vkGetPhysicalDeviceSurfacePresentModesKHR(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, std::uint32_t* presentModeCount,
    VkPresentModeKHR* presentModes) {
    static_cast<void>(physicalDevice);
    static_cast<void>(surface);
    std::vector<VkPresentModeKHR> availableModes{VK_PRESENT_MODE_FIFO_KHR};
    if (g_offerMailboxPresentMode) {
        availableModes.push_back(VK_PRESENT_MODE_MAILBOX_KHR);
    }
    if (presentModes == nullptr) {
        *presentModeCount = static_cast<std::uint32_t>(availableModes.size());
        return VK_SUCCESS;
    }
    for (std::uint32_t modeIndex = 0u; modeIndex < *presentModeCount; ++modeIndex) {
        presentModes[modeIndex] = availableModes[modeIndex];
    }
    return VK_SUCCESS;
}

extern "C" VkResult VKAPI_CALL vkCreateSwapchainKHR(
    VkDevice device, const VkSwapchainCreateInfoKHR* createInfo,
    const VkAllocationCallbacks* allocationCallbacks, VkSwapchainKHR* swapchain) {
    static_cast<void>(device);
    static_cast<void>(allocationCallbacks);
    g_observedSharingMode = createInfo->imageSharingMode;
    g_observedQueueFamilyCount = createInfo->queueFamilyIndexCount;
    *swapchain = reinterpret_cast<VkSwapchainKHR>(0x5000u);
    return VK_SUCCESS;
}

extern "C" void VKAPI_CALL vkDestroySwapchainKHR(
    VkDevice device, VkSwapchainKHR swapchain,
    const VkAllocationCallbacks* allocationCallbacks) {
    static_cast<void>(device);
    static_cast<void>(swapchain);
    static_cast<void>(allocationCallbacks);
    ++g_destroyedSwapchainCount;
}

extern "C" VkResult VKAPI_CALL vkGetSwapchainImagesKHR(
    VkDevice device, VkSwapchainKHR swapchain, std::uint32_t* imageCount,
    VkImage* images) {
    static_cast<void>(device);
    static_cast<void>(swapchain);
    if (images == nullptr) {
        *imageCount = g_swapchainImageCount;
        return VK_SUCCESS;
    }
    for (std::uint32_t imageIndex = 0u; imageIndex < *imageCount; ++imageIndex) {
        images[imageIndex] =
            reinterpret_cast<VkImage>(static_cast<std::uintptr_t>(0x6000u + imageIndex));
    }
    return VK_SUCCESS;
}

extern "C" VkResult VKAPI_CALL vkCreateImageView(
    VkDevice device, const VkImageViewCreateInfo* createInfo,
    const VkAllocationCallbacks* allocationCallbacks, VkImageView* imageView) {
    static_cast<void>(device);
    static_cast<void>(createInfo);
    static_cast<void>(allocationCallbacks);
    *imageView = reinterpret_cast<VkImageView>(
        static_cast<std::uintptr_t>(0x7000u + g_createdImageViewCount));
    ++g_createdImageViewCount;
    return VK_SUCCESS;
}

extern "C" void VKAPI_CALL vkDestroyImageView(
    VkDevice device, VkImageView imageView,
    const VkAllocationCallbacks* allocationCallbacks) {
    static_cast<void>(device);
    static_cast<void>(imageView);
    static_cast<void>(allocationCallbacks);
    ++g_destroyedImageViewCount;
}

extern "C" VkResult VKAPI_CALL vkQueueWaitIdle(VkQueue queue) {
    static_cast<void>(queue);
    return VK_SUCCESS;
}

TEST_CASE("Presentation runtime boundary rejects null arguments", "[presentationRuntime]") {
    NativePresentationRuntimeCreateResultVersion1 createResult{};
    REQUIRE(barriEwwCreatePresentationRuntimeVersion1(nullptr, &createResult)
            == NativePresentationRuntimeOperationResult::InvalidArgument);
    const NativePresentationRuntimeCreateInfoVersion1 createInfo = makeCreateInfo();
    REQUIRE(barriEwwCreatePresentationRuntimeVersion1(&createInfo, nullptr)
            == NativePresentationRuntimeOperationResult::InvalidArgument);
    REQUIRE(barriEwwDestroyPresentationRuntimeVersion1(0u)
            == NativePresentationRuntimeOperationResult::InvalidArgument);
}

TEST_CASE("Presentation runtime boundary rejects missing bootstrap handles",
          "[presentationRuntime]") {
    resetPresentationState();
    NativePresentationRuntimeCreateInfoVersion1 createInfo = makeCreateInfo();
    createInfo.surfaceHandle = 0u;
    NativePresentationRuntimeCreateResultVersion1 createResult{};
    REQUIRE(barriEwwCreatePresentationRuntimeVersion1(&createInfo, &createResult)
            == NativePresentationRuntimeOperationResult::InvalidArgument);
}

TEST_CASE("Presentation runtime boundary rejects an unsupported surface",
          "[presentationRuntime]") {
    NativePresentationRuntimeCreateResultVersion1 createResult{};

    SECTION("zero framebuffer extent") {
        resetPresentationState();
        NativePresentationRuntimeCreateInfoVersion1 createInfo = makeCreateInfo();
        createInfo.framebufferWidth = 0u;
        REQUIRE(barriEwwCreatePresentationRuntimeVersion1(&createInfo, &createResult)
                == NativePresentationRuntimeOperationResult::UnsupportedSurface);
    }

    SECTION("surface without transfer-destination usage") {
        resetPresentationState();
        g_supportedUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        const NativePresentationRuntimeCreateInfoVersion1 createInfo = makeCreateInfo();
        REQUIRE(barriEwwCreatePresentationRuntimeVersion1(&createInfo, &createResult)
                == NativePresentationRuntimeOperationResult::UnsupportedSurface);
    }
}

TEST_CASE("Presentation runtime boundary selects MAILBOX and preferred format",
          "[presentationRuntime]") {
    resetPresentationState();
    const NativePresentationRuntimeCreateInfoVersion1 createInfo = makeCreateInfo();
    NativePresentationRuntimeCreateResultVersion1 createResult{};

    REQUIRE(barriEwwCreatePresentationRuntimeVersion1(&createInfo, &createResult)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(createResult.runtimeAddress != 0u);
    REQUIRE(createResult.selectedFormatValue == 2u); // B8G8R8A8
    REQUIRE(createResult.selectedPresentModeValue
            == static_cast<std::uint32_t>(VK_PRESENT_MODE_MAILBOX_KHR));
    REQUIRE(createResult.selectedSharingModeValue
            == static_cast<std::uint32_t>(VK_SHARING_MODE_EXCLUSIVE));
    REQUIRE(createResult.swapchainImageCount == 3u);
    REQUIRE(g_createdImageViewCount == 3u);
    REQUIRE(g_observedQueueFamilyCount == 0u);

    REQUIRE(barriEwwDestroyPresentationRuntimeVersion1(createResult.runtimeAddress)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(g_destroyedImageViewCount == 3u);
    REQUIRE(g_destroyedSwapchainCount == 1u);
}

TEST_CASE("Presentation runtime boundary falls back to FIFO and concurrent sharing",
          "[presentationRuntime]") {
    resetPresentationState();
    g_offerMailboxPresentMode = false;
    g_offerPreferredFormat = false;
    NativePresentationRuntimeCreateInfoVersion1 createInfo = makeCreateInfo();
    createInfo.presentQueueHandle = 6u;
    createInfo.presentQueueFamilyIndex = 1u;
    NativePresentationRuntimeCreateResultVersion1 createResult{};

    REQUIRE(barriEwwCreatePresentationRuntimeVersion1(&createInfo, &createResult)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(createResult.selectedFormatValue == 1u); // R8G8B8A8 fallback
    REQUIRE(createResult.selectedPresentModeValue
            == static_cast<std::uint32_t>(VK_PRESENT_MODE_FIFO_KHR));
    REQUIRE(createResult.selectedSharingModeValue
            == static_cast<std::uint32_t>(VK_SHARING_MODE_CONCURRENT));
    REQUIRE(g_observedSharingMode == VK_SHARING_MODE_CONCURRENT);
    REQUIRE(g_observedQueueFamilyCount == 2u);

    REQUIRE(barriEwwDestroyPresentationRuntimeVersion1(createResult.runtimeAddress)
            == NativePresentationRuntimeOperationResult::Success);
}

TEST_CASE("Presentation runtime boundary clamps image count to the surface maximum",
          "[presentationRuntime]") {
    resetPresentationState();
    g_minImageCount = 3u;
    g_maxImageCount = 3u;
    g_swapchainImageCount = 3u;
    const NativePresentationRuntimeCreateInfoVersion1 createInfo = makeCreateInfo();
    NativePresentationRuntimeCreateResultVersion1 createResult{};

    REQUIRE(barriEwwCreatePresentationRuntimeVersion1(&createInfo, &createResult)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(createResult.swapchainImageCount == 3u);
    REQUIRE(barriEwwDestroyPresentationRuntimeVersion1(createResult.runtimeAddress)
            == NativePresentationRuntimeOperationResult::Success);
}
