#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include <vulkan/vulkan.h>

#include "BarriEww/Interoperability/NativePresentationRuntimeBoundary.hpp"
#include "BarriEww/Vulkan/VulkanPresentationRuntime.hpp"
#include "../src/Interoperability/NativePresentationRuntimeBoundaryImplementation.hpp"

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
VkResult g_acquireResult = VK_SUCCESS;
VkResult g_submitResult = VK_SUCCESS;
VkResult g_presentResult = VK_SUCCESS;
std::uint32_t g_acquiredImageIndex = 0u;
std::uint32_t g_submitCallCount = 0u;
std::uint32_t g_presentCallCount = 0u;
std::uint32_t g_clearImageCallCount = 0u;
std::uint32_t g_recordedCommandBufferCount = 0u;
std::array<float, 4> g_observedClearColor{};

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
    g_acquireResult = VK_SUCCESS;
    g_submitResult = VK_SUCCESS;
    g_presentResult = VK_SUCCESS;
    g_acquiredImageIndex = 0u;
    g_submitCallCount = 0u;
    g_presentCallCount = 0u;
    g_clearImageCallCount = 0u;
    g_recordedCommandBufferCount = 0u;
    g_observedClearColor = {};
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

extern "C" VkResult VKAPI_CALL vkCreateSemaphore(
    VkDevice device, const VkSemaphoreCreateInfo* createInfo,
    const VkAllocationCallbacks* allocationCallbacks, VkSemaphore* semaphore) {
    static_cast<void>(device);
    static_cast<void>(createInfo);
    static_cast<void>(allocationCallbacks);
    *semaphore = reinterpret_cast<VkSemaphore>(0x8000u);
    return VK_SUCCESS;
}

extern "C" void VKAPI_CALL vkDestroySemaphore(
    VkDevice device, VkSemaphore semaphore,
    const VkAllocationCallbacks* allocationCallbacks) {
    static_cast<void>(device);
    static_cast<void>(semaphore);
    static_cast<void>(allocationCallbacks);
}

extern "C" VkResult VKAPI_CALL vkCreateFence(
    VkDevice device, const VkFenceCreateInfo* createInfo,
    const VkAllocationCallbacks* allocationCallbacks, VkFence* fence) {
    static_cast<void>(device);
    static_cast<void>(createInfo);
    static_cast<void>(allocationCallbacks);
    *fence = reinterpret_cast<VkFence>(0x9000u);
    return VK_SUCCESS;
}

extern "C" void VKAPI_CALL vkDestroyFence(
    VkDevice device, VkFence fence, const VkAllocationCallbacks* allocationCallbacks) {
    static_cast<void>(device);
    static_cast<void>(fence);
    static_cast<void>(allocationCallbacks);
}

extern "C" VkResult VKAPI_CALL vkWaitForFences(
    VkDevice device, std::uint32_t fenceCount, const VkFence* fences, VkBool32 waitAll,
    std::uint64_t timeout) {
    static_cast<void>(device);
    static_cast<void>(fenceCount);
    static_cast<void>(fences);
    static_cast<void>(waitAll);
    static_cast<void>(timeout);
    return VK_SUCCESS;
}

extern "C" VkResult VKAPI_CALL vkResetFences(
    VkDevice device, std::uint32_t fenceCount, const VkFence* fences) {
    static_cast<void>(device);
    static_cast<void>(fenceCount);
    static_cast<void>(fences);
    return VK_SUCCESS;
}

extern "C" VkResult VKAPI_CALL vkAcquireNextImageKHR(
    VkDevice device, VkSwapchainKHR swapchain, std::uint64_t timeout,
    VkSemaphore semaphore, VkFence fence, std::uint32_t* imageIndex) {
    static_cast<void>(device);
    static_cast<void>(swapchain);
    static_cast<void>(timeout);
    static_cast<void>(semaphore);
    static_cast<void>(fence);
    *imageIndex = g_acquiredImageIndex;
    return g_acquireResult;
}

extern "C" VkResult VKAPI_CALL vkQueueSubmit(
    VkQueue queue, std::uint32_t submitCount, const VkSubmitInfo* submitInfo, VkFence fence) {
    static_cast<void>(queue);
    static_cast<void>(submitCount);
    static_cast<void>(submitInfo);
    static_cast<void>(fence);
    ++g_submitCallCount;
    return g_submitResult;
}

extern "C" VkResult VKAPI_CALL vkQueuePresentKHR(
    VkQueue queue, const VkPresentInfoKHR* presentInfo) {
    static_cast<void>(queue);
    static_cast<void>(presentInfo);
    ++g_presentCallCount;
    return g_presentResult;
}

extern "C" VkResult VKAPI_CALL vkCreateCommandPool(
    VkDevice device, const VkCommandPoolCreateInfo* createInfo,
    const VkAllocationCallbacks* allocationCallbacks, VkCommandPool* commandPool) {
    static_cast<void>(device);
    static_cast<void>(createInfo);
    static_cast<void>(allocationCallbacks);
    *commandPool = reinterpret_cast<VkCommandPool>(0xA000u);
    return VK_SUCCESS;
}

extern "C" void VKAPI_CALL vkDestroyCommandPool(
    VkDevice device, VkCommandPool commandPool,
    const VkAllocationCallbacks* allocationCallbacks) {
    static_cast<void>(device);
    static_cast<void>(commandPool);
    static_cast<void>(allocationCallbacks);
}

extern "C" VkResult VKAPI_CALL vkAllocateCommandBuffers(
    VkDevice device, const VkCommandBufferAllocateInfo* allocateInfo,
    VkCommandBuffer* commandBuffers) {
    static_cast<void>(device);
    for (std::uint32_t bufferIndex = 0u; bufferIndex < allocateInfo->commandBufferCount;
         ++bufferIndex) {
        commandBuffers[bufferIndex] = reinterpret_cast<VkCommandBuffer>(
            static_cast<std::uintptr_t>(0xB000u + bufferIndex));
    }
    return VK_SUCCESS;
}

extern "C" VkResult VKAPI_CALL vkResetCommandBuffer(
    VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags) {
    static_cast<void>(commandBuffer);
    static_cast<void>(flags);
    return VK_SUCCESS;
}

extern "C" VkResult VKAPI_CALL vkBeginCommandBuffer(
    VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* beginInfo) {
    static_cast<void>(commandBuffer);
    static_cast<void>(beginInfo);
    return VK_SUCCESS;
}

extern "C" void VKAPI_CALL vkCmdPipelineBarrier(
    VkCommandBuffer commandBuffer, VkPipelineStageFlags sourceStageMask,
    VkPipelineStageFlags destinationStageMask, VkDependencyFlags dependencyFlags,
    std::uint32_t memoryBarrierCount, const VkMemoryBarrier* memoryBarriers,
    std::uint32_t bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier* bufferMemoryBarriers,
    std::uint32_t imageMemoryBarrierCount,
    const VkImageMemoryBarrier* imageMemoryBarriers) {
    static_cast<void>(commandBuffer);
    static_cast<void>(sourceStageMask);
    static_cast<void>(destinationStageMask);
    static_cast<void>(dependencyFlags);
    static_cast<void>(memoryBarrierCount);
    static_cast<void>(memoryBarriers);
    static_cast<void>(bufferMemoryBarrierCount);
    static_cast<void>(bufferMemoryBarriers);
    static_cast<void>(imageMemoryBarrierCount);
    static_cast<void>(imageMemoryBarriers);
}

extern "C" void VKAPI_CALL vkCmdClearColorImage(
    VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout,
    const VkClearColorValue* clearColor, std::uint32_t rangeCount,
    const VkImageSubresourceRange* ranges) {
    static_cast<void>(commandBuffer);
    static_cast<void>(image);
    static_cast<void>(imageLayout);
    static_cast<void>(rangeCount);
    static_cast<void>(ranges);
    g_observedClearColor = {clearColor->float32[0], clearColor->float32[1],
                            clearColor->float32[2], clearColor->float32[3]};
    ++g_clearImageCallCount;
}

extern "C" VkResult VKAPI_CALL vkEndCommandBuffer(VkCommandBuffer commandBuffer) {
    static_cast<void>(commandBuffer);
    ++g_recordedCommandBufferCount;
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

namespace {

/** Creates a runtime through the boundary; caller destroys the returned address. */
std::uint64_t openRuntime() {
    const NativePresentationRuntimeCreateInfoVersion1 createInfo = makeCreateInfo();
    NativePresentationRuntimeCreateResultVersion1 createResult{};
    REQUIRE(barriEwwCreatePresentationRuntimeVersion1(&createInfo, &createResult)
            == NativePresentationRuntimeOperationResult::Success);
    return createResult.runtimeAddress;
}

/** Builds a nonzero submit result that exposes whether a boundary cleared it. */
barrieww::NativePresentationSubmitFrameResultVersion1 makeSentinelSubmitResult() {
    return {std::numeric_limits<std::uint32_t>::max(), VK_ERROR_UNKNOWN};
}

} // namespace

TEST_CASE("Presentation frame reports surface unavailable on zero extent",
          "[presentationRuntime]") {
    resetPresentationState();
    const std::uint64_t runtimeAddress = openRuntime();
    barrieww::NativePresentationBeginFrameResultVersion1 beginResult{};
    barrieww::NativePresentationFrameMetricsVersion1 priorMetrics{};

    REQUIRE(barriEwwBeginPresentationFrameVersion1(runtimeAddress, 0u, 720u, &beginResult,
                                                   &priorMetrics)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(beginResult.frameStatusValue
            == static_cast<std::uint32_t>(
                barrieww::VulkanPresentationRuntime::FrameStatus::SurfaceUnavailable));
    REQUIRE(beginResult.priorMetricsValid == 0u);

    REQUIRE(barriEwwDestroyPresentationRuntimeVersion1(runtimeAddress)
            == NativePresentationRuntimeOperationResult::Success);
}

TEST_CASE("Presentation frame rejects submit without an open frame and null results",
           "[presentationRuntime]") {
    resetPresentationState();
    const std::uint64_t runtimeAddress = openRuntime();
    barrieww::NativePresentationSubmitFrameResultVersion1 submitResult =
        makeSentinelSubmitResult();

    REQUIRE(barriEwwSubmitPresentationFrameVersion1(runtimeAddress, 0u, &submitResult)
            == NativePresentationRuntimeOperationResult::InvalidArgument);
    REQUIRE(barriEwwSubmitPresentationFrameVersion1(runtimeAddress, 0u, nullptr)
            == NativePresentationRuntimeOperationResult::InvalidArgument);
    submitResult = makeSentinelSubmitResult();
    REQUIRE(barriEwwSubmitAndPresentClearFrameVersion1(runtimeAddress, 0.1f, 0.3f, 0.7f,
                                                       &submitResult)
            == NativePresentationRuntimeOperationResult::InvalidArgument);
    REQUIRE(submitResult.frameStatusValue == 0u);
    REQUIRE(submitResult.vulkanResult == 0);
    REQUIRE(barriEwwSubmitAndPresentClearFrameVersion1(runtimeAddress, 0.1f, 0.3f, 0.7f,
                                                       nullptr)
            == NativePresentationRuntimeOperationResult::InvalidArgument);
    submitResult = makeSentinelSubmitResult();
    REQUIRE(barriEwwSubmitAndPresentClearFrameVersion1(0u, 0.1f, 0.3f, 0.7f,
                                                       &submitResult)
            == NativePresentationRuntimeOperationResult::InvalidArgument);
    REQUIRE(submitResult.frameStatusValue == 0u);
    REQUIRE(submitResult.vulkanResult == 0);
    REQUIRE(barriEwwBeginPresentationFrameVersion1(0u, 8u, 8u, nullptr, nullptr)
            == NativePresentationRuntimeOperationResult::InvalidArgument);

    REQUIRE(barriEwwDestroyPresentationRuntimeVersion1(runtimeAddress)
            == NativePresentationRuntimeOperationResult::Success);
}

TEST_CASE("Presentation clear submission rejects non-finite and out-of-range colors",
          "[presentationRuntime]") {
    resetPresentationState();
    const std::uint64_t runtimeAddress = openRuntime();
    barrieww::NativePresentationBeginFrameResultVersion1 beginResult{};
    barrieww::NativePresentationFrameMetricsVersion1 priorMetrics{};
    constexpr float validChannel = 0.5f;
    const float notANumber = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const std::array<std::array<float, 3>, 12> invalidColors{{
        {notANumber, validChannel, validChannel},
        {validChannel, notANumber, validChannel},
        {validChannel, validChannel, notANumber},
        {infinity, validChannel, validChannel},
        {validChannel, infinity, validChannel},
        {validChannel, validChannel, infinity},
        {-0.1f, validChannel, validChannel},
        {validChannel, -0.1f, validChannel},
        {validChannel, validChannel, -0.1f},
        {1.1f, validChannel, validChannel},
        {validChannel, 1.1f, validChannel},
        {validChannel, validChannel, 1.1f},
    }};

    for (std::size_t invalidColorIndex = 0u; invalidColorIndex < invalidColors.size();
         ++invalidColorIndex) {
        CAPTURE(invalidColorIndex);
        REQUIRE(barriEwwBeginPresentationFrameVersion1(runtimeAddress, 1280u, 720u,
                                                       &beginResult, &priorMetrics)
                == NativePresentationRuntimeOperationResult::Success);
        barrieww::NativePresentationSubmitFrameResultVersion1 submitResult =
            makeSentinelSubmitResult();
        const auto& invalidColor = invalidColors[invalidColorIndex];
        REQUIRE(barriEwwSubmitAndPresentClearFrameVersion1(
                    runtimeAddress, invalidColor[0], invalidColor[1], invalidColor[2],
                    &submitResult)
                == NativePresentationRuntimeOperationResult::InvalidArgument);
        REQUIRE(submitResult.frameStatusValue == 0u);
        REQUIRE(submitResult.vulkanResult == 0);
        REQUIRE(g_clearImageCallCount == 0u);
        REQUIRE(g_submitCallCount == invalidColorIndex);
        REQUIRE(g_presentCallCount == invalidColorIndex);

        submitResult = makeSentinelSubmitResult();
        REQUIRE(barriEwwSubmitPresentationFrameVersion1(runtimeAddress, 0u, &submitResult)
                == NativePresentationRuntimeOperationResult::Success);
        REQUIRE(submitResult.frameStatusValue
                == static_cast<std::uint32_t>(
                    barrieww::VulkanPresentationRuntime::FrameStatus::Success));
        REQUIRE(submitResult.vulkanResult == VK_SUCCESS);
    }

    REQUIRE(barriEwwDestroyPresentationRuntimeVersion1(runtimeAddress)
            == NativePresentationRuntimeOperationResult::Success);
}

TEST_CASE("Presentation clear submission records the requested color for an open frame",
          "[presentationRuntime]") {
    resetPresentationState();
    const std::uint64_t runtimeAddress = openRuntime();
    barrieww::NativePresentationBeginFrameResultVersion1 beginResult{};
    barrieww::NativePresentationFrameMetricsVersion1 priorMetrics{};
    barrieww::NativePresentationSubmitFrameResultVersion1 submitResult{};

    REQUIRE(barriEwwBeginPresentationFrameVersion1(runtimeAddress, 1280u, 720u,
                                                   &beginResult, &priorMetrics)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(barriEwwSubmitAndPresentClearFrameVersion1(runtimeAddress, 0.1f, 0.3f, 0.7f,
                                                       &submitResult)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(submitResult.frameStatusValue
            == static_cast<std::uint32_t>(
                barrieww::VulkanPresentationRuntime::FrameStatus::Success));
    REQUIRE(g_observedClearColor == std::array{0.1f, 0.3f, 0.7f, 1.0f});
    REQUIRE(g_clearImageCallCount == 1u);
    REQUIRE(g_submitCallCount == 1u);
    REQUIRE(g_presentCallCount == 1u);

    REQUIRE(barriEwwDestroyPresentationRuntimeVersion1(runtimeAddress)
            == NativePresentationRuntimeOperationResult::Success);
}

TEST_CASE("Presentation clear submission propagates submit and present results",
          "[presentationRuntime]") {
    resetPresentationState();
    const std::uint64_t runtimeAddress = openRuntime();
    barrieww::NativePresentationBeginFrameResultVersion1 beginResult{};
    barrieww::NativePresentationFrameMetricsVersion1 priorMetrics{};
    barrieww::NativePresentationSubmitFrameResultVersion1 submitResult =
        makeSentinelSubmitResult();

    SECTION("submit failure requests recreation and preserves the raw result") {
        g_submitResult = VK_ERROR_DEVICE_LOST;
        REQUIRE(barriEwwBeginPresentationFrameVersion1(runtimeAddress, 1280u, 720u,
                                                       &beginResult, &priorMetrics)
                == NativePresentationRuntimeOperationResult::Success);
        REQUIRE(barriEwwSubmitAndPresentClearFrameVersion1(runtimeAddress, 0.1f, 0.3f, 0.7f,
                                                           &submitResult)
                == NativePresentationRuntimeOperationResult::Success);
        REQUIRE(submitResult.frameStatusValue
                == static_cast<std::uint32_t>(
                    barrieww::VulkanPresentationRuntime::FrameStatus::RecreateRequired));
        REQUIRE(submitResult.vulkanResult == VK_ERROR_DEVICE_LOST);
        REQUIRE(g_clearImageCallCount == 1u);
        REQUIRE(g_submitCallCount == 1u);
        REQUIRE(g_presentCallCount == 0u);
    }

    SECTION("suboptimal present preserves the normal status and raw result") {
        g_presentResult = VK_SUBOPTIMAL_KHR;
        REQUIRE(barriEwwBeginPresentationFrameVersion1(runtimeAddress, 1280u, 720u,
                                                       &beginResult, &priorMetrics)
                == NativePresentationRuntimeOperationResult::Success);
        REQUIRE(barriEwwSubmitAndPresentClearFrameVersion1(runtimeAddress, 0.1f, 0.3f, 0.7f,
                                                           &submitResult)
                == NativePresentationRuntimeOperationResult::Success);
        REQUIRE(submitResult.frameStatusValue
                == static_cast<std::uint32_t>(
                    barrieww::VulkanPresentationRuntime::FrameStatus::Suboptimal));
        REQUIRE(submitResult.vulkanResult == VK_SUBOPTIMAL_KHR);
        REQUIRE(g_clearImageCallCount == 1u);
        REQUIRE(g_submitCallCount == 1u);
        REQUIRE(g_presentCallCount == 1u);
    }

    REQUIRE(barriEwwDestroyPresentationRuntimeVersion1(runtimeAddress)
            == NativePresentationRuntimeOperationResult::Success);
}

TEST_CASE("Presentation clear submission contains clear exceptions and clears output",
          "[presentationRuntime]") {
    barrieww::NativePresentationSubmitFrameResultVersion1 submitResult =
        makeSentinelSubmitResult();
    const auto throwingClearSubmission =
        []() -> barrieww::VulkanPresentationRuntime::SubmitFrameResult {
        throw std::runtime_error{"Injected clear failure"};
    };

    REQUIRE(barrieww::interoperability::executeNativePresentationSubmitFrameOperation(
                &submitResult, throwingClearSubmission)
            == NativePresentationRuntimeOperationResult::InternalFailure);
    REQUIRE(submitResult.frameStatusValue == 0u);
    REQUIRE(submitResult.vulkanResult == 0);
}

TEST_CASE("Presentation frame cycles begin/submit and reuses a slot with prior metrics",
          "[presentationRuntime]") {
    resetPresentationState();
    const std::uint64_t runtimeAddress = openRuntime();
    barrieww::NativePresentationBeginFrameResultVersion1 beginResult{};
    barrieww::NativePresentationFrameMetricsVersion1 priorMetrics{};
    barrieww::NativePresentationSubmitFrameResultVersion1 submitResult{};

    // Frames-in-flight is 2, so the third frame reuses slot 0 and returns its metrics.
    for (std::uint32_t frameIndex = 0u; frameIndex < 3u; ++frameIndex) {
        g_acquiredImageIndex = frameIndex % 3u;
        REQUIRE(barriEwwBeginPresentationFrameVersion1(runtimeAddress, 1280u, 720u,
                                                       &beginResult, &priorMetrics)
                == NativePresentationRuntimeOperationResult::Success);
        REQUIRE(beginResult.frameStatusValue
                == static_cast<std::uint32_t>(
                    barrieww::VulkanPresentationRuntime::FrameStatus::Success));
        REQUIRE(beginResult.frameSlotIndex == frameIndex % 2u);
        REQUIRE(beginResult.frameSequence == frameIndex);
        if (frameIndex == 2u) {
            REQUIRE(beginResult.priorMetricsValid == 1u);
            REQUIRE(priorMetrics.frameSequence == 0u);
            REQUIRE(priorMetrics.frameSlotIndex == 0u);
            REQUIRE((priorMetrics.validFlags
                     & barrieww::VulkanPresentationRuntime::s_cpuMetricsValidFlag) != 0u);
            REQUIRE(priorMetrics.presentModeValue
                    == static_cast<std::uint32_t>(VK_PRESENT_MODE_MAILBOX_KHR));
        }
        REQUIRE(barriEwwSubmitPresentationFrameVersion1(runtimeAddress, 0u, &submitResult)
                == NativePresentationRuntimeOperationResult::Success);
        REQUIRE(submitResult.frameStatusValue
                == static_cast<std::uint32_t>(
                    barrieww::VulkanPresentationRuntime::FrameStatus::Success));
    }
    REQUIRE(g_submitCallCount == 3u);
    REQUIRE(g_presentCallCount == 3u);

    REQUIRE(barriEwwDestroyPresentationRuntimeVersion1(runtimeAddress)
            == NativePresentationRuntimeOperationResult::Success);
}

TEST_CASE("Presentation clear frame records a clear, submits and presents",
          "[presentationRuntime]") {
    resetPresentationState();
    const std::uint64_t runtimeAddress = openRuntime();
    barrieww::NativePresentationBeginFrameResultVersion1 beginResult{};
    barrieww::NativePresentationFrameMetricsVersion1 priorMetrics{};
    barrieww::NativePresentationSubmitFrameResultVersion1 submitResult{};

    REQUIRE(barriEwwPresentClearFrameVersion1(runtimeAddress, 1280u, 720u, 0.2f, 0.4f, 0.6f,
                                              &beginResult, &priorMetrics, &submitResult)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(beginResult.frameStatusValue
            == static_cast<std::uint32_t>(
                barrieww::VulkanPresentationRuntime::FrameStatus::Success));
    REQUIRE(submitResult.frameStatusValue
            == static_cast<std::uint32_t>(
                barrieww::VulkanPresentationRuntime::FrameStatus::Success));
    REQUIRE(g_clearImageCallCount == 1u);
    REQUIRE(g_recordedCommandBufferCount == 1u);
    REQUIRE(g_submitCallCount == 1u);
    REQUIRE(g_presentCallCount == 1u);

    // A zero framebuffer extent short-circuits before any recording or submit.
    REQUIRE(barriEwwPresentClearFrameVersion1(runtimeAddress, 0u, 720u, 0.0f, 0.0f, 0.0f,
                                              &beginResult, &priorMetrics, &submitResult)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(submitResult.frameStatusValue
            == static_cast<std::uint32_t>(
                barrieww::VulkanPresentationRuntime::FrameStatus::SurfaceUnavailable));
    REQUIRE(g_clearImageCallCount == 1u);

    REQUIRE(barriEwwPresentClearFrameVersion1(0u, 8u, 8u, 0.0f, 0.0f, 0.0f, nullptr, nullptr,
                                              nullptr)
            == NativePresentationRuntimeOperationResult::InvalidArgument);

    REQUIRE(barriEwwDestroyPresentationRuntimeVersion1(runtimeAddress)
            == NativePresentationRuntimeOperationResult::Success);
}

TEST_CASE("Presentation frame maps out-of-date and suboptimal to recreate/suboptimal",
          "[presentationRuntime]") {
    resetPresentationState();
    const std::uint64_t runtimeAddress = openRuntime();
    barrieww::NativePresentationBeginFrameResultVersion1 beginResult{};
    barrieww::NativePresentationFrameMetricsVersion1 priorMetrics{};
    barrieww::NativePresentationSubmitFrameResultVersion1 submitResult{};

    SECTION("out-of-date at acquire requests recreation without submitting") {
        g_acquireResult = VK_ERROR_OUT_OF_DATE_KHR;
        REQUIRE(barriEwwBeginPresentationFrameVersion1(runtimeAddress, 1280u, 720u,
                                                       &beginResult, &priorMetrics)
                == NativePresentationRuntimeOperationResult::Success);
        REQUIRE(beginResult.frameStatusValue
                == static_cast<std::uint32_t>(
                    barrieww::VulkanPresentationRuntime::FrameStatus::RecreateRequired));
        REQUIRE(barriEwwSubmitPresentationFrameVersion1(runtimeAddress, 0u, &submitResult)
                == NativePresentationRuntimeOperationResult::InvalidArgument);
    }

    SECTION("suboptimal at present completes the frame") {
        g_presentResult = VK_SUBOPTIMAL_KHR;
        REQUIRE(barriEwwBeginPresentationFrameVersion1(runtimeAddress, 1280u, 720u,
                                                       &beginResult, &priorMetrics)
                == NativePresentationRuntimeOperationResult::Success);
        REQUIRE(barriEwwSubmitPresentationFrameVersion1(runtimeAddress, 0u, &submitResult)
                == NativePresentationRuntimeOperationResult::Success);
        REQUIRE(submitResult.frameStatusValue
                == static_cast<std::uint32_t>(
                    barrieww::VulkanPresentationRuntime::FrameStatus::Suboptimal));
        REQUIRE(g_presentCallCount == 1u);
    }

    REQUIRE(barriEwwDestroyPresentationRuntimeVersion1(runtimeAddress)
            == NativePresentationRuntimeOperationResult::Success);
}
