#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include <vulkan/vulkan.h>

#include "BarriEww/Interoperability/NativeHostImagePresentationRuntimeCreateInfoVersion1.hpp"
#include "BarriEww/Interoperability/NativePresentationDetachHostImageResourcesResultVersion1.hpp"
#include "BarriEww/Interoperability/NativePresentationRuntimeBoundary.hpp"
#include "BarriEww/Interoperability/PresentationImageFormat.hpp"
#include "BarriEww/Vulkan/VulkanPresentationImageFormatMapping.hpp"
#include "BarriEww/Vulkan/VulkanHostImagePresentationResources.hpp"
#include "BarriEww/Vulkan/VulkanPresentationRuntime.hpp"
#include "../src/Interoperability/NativePresentationRuntimeBoundaryImplementation.hpp"

using barrieww::NativePresentationRuntimeCreateInfoVersion1;
using barrieww::NativePresentationRuntimeCreateResultVersion1;
using barrieww::NativePresentationRuntimeOperationResult;

static_assert(static_cast<std::uint32_t>(barrieww::PresentationImageFormat::R8G8B8A8Unorm)
              == 1u);
static_assert(static_cast<std::uint32_t>(barrieww::PresentationImageFormat::B8G8R8A8Unorm)
              == 2u);
static_assert(barrieww::mapPresentationImageFormat(
                  barrieww::PresentationImageFormat::R8G8B8A8Unorm)
              == VK_FORMAT_R8G8B8A8_UNORM);
static_assert(barrieww::mapPresentationImageFormat(
                  barrieww::PresentationImageFormat::B8G8R8A8Unorm)
              == VK_FORMAT_B8G8R8A8_UNORM);
static_assert(barrieww::mapPresentationImageFormat(
                  static_cast<barrieww::PresentationImageFormat>(UINT32_MAX))
              == VK_FORMAT_UNDEFINED);

static_assert(sizeof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1) == 96u);
static_assert(alignof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1) == 8u);
static_assert(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       instanceHandle) == 0u);
static_assert(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       physicalDeviceHandle) == 8u);
static_assert(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       logicalDeviceHandle) == 16u);
static_assert(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       surfaceHandle) == 24u);
static_assert(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       graphicsQueueHandle) == 32u);
static_assert(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       presentQueueHandle) == 40u);
static_assert(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       graphicsQueueFamilyIndex) == 48u);
static_assert(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       presentQueueFamilyIndex) == 52u);
static_assert(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       framebufferWidth) == 56u);
static_assert(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       framebufferHeight) == 60u);
static_assert(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       framesInFlightCount) == 64u);
static_assert(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       reservedFlags) == 68u);
static_assert(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       hostImageHandle) == 72u);
static_assert(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       hostImageFormatValue) == 80u);
static_assert(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       requestedSurfaceFormatValue) == 84u);
static_assert(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       hostImageWidth) == 88u);
static_assert(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       hostImageHeight) == 92u);
static_assert(std::is_standard_layout_v<
              barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1>);
static_assert(std::is_trivially_copyable_v<
              barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1>);

static_assert(sizeof(barrieww::NativePresentationDetachHostImageResourcesResultVersion1)
              == 8u);
static_assert(alignof(barrieww::NativePresentationDetachHostImageResourcesResultVersion1)
              == 4u);
static_assert(offsetof(barrieww::NativePresentationDetachHostImageResourcesResultVersion1,
                       vulkanResult) == 0u);
static_assert(offsetof(barrieww::NativePresentationDetachHostImageResourcesResultVersion1,
                       reserved) == 4u);
static_assert(std::is_standard_layout_v<
              barrieww::NativePresentationDetachHostImageResourcesResultVersion1>);
static_assert(std::is_trivially_copyable_v<
              barrieww::NativePresentationDetachHostImageResourcesResultVersion1>);

static_assert(std::is_same_v<
              decltype(&barriEwwCreateHostImagePresentationRuntimeVersion1),
              barrieww::NativePresentationRuntimeOperationResult (*)(
                  const barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1*,
                  barrieww::NativePresentationRuntimeCreateResultVersion1*) noexcept>);
static_assert(std::is_same_v<
              decltype(&barriEwwDetachHostImagePresentationResourcesVersion1),
              barrieww::NativePresentationRuntimeOperationResult (*)(
                  std::uint64_t,
                  barrieww::NativePresentationDetachHostImageResourcesResultVersion1*)
                  noexcept>);
static_assert(std::is_same_v<
              decltype(&barriEwwSubmitAndPresentHostImageFrameVersion1),
              barrieww::NativePresentationRuntimeOperationResult (*)(
                  std::uint64_t,
                  barrieww::NativePresentationSubmitFrameResultVersion1*) noexcept>);

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

enum class HostResourceOperation {
    ImageView,
    Sampler,
    DescriptorSetLayout,
    DescriptorPool,
    DescriptorSet,
    PipelineLayout,
    VertexShaderModule,
    FragmentShaderModule,
    GraphicsPipeline,
    CommandPool,
    CommandBuffers,
    BeginCommandBuffer,
    EndCommandBuffer,
};

std::optional<HostResourceOperation> g_failedHostResourceOperation;
VkResult g_injectedHostResourceResult = VK_ERROR_OUT_OF_HOST_MEMORY;
std::vector<HostResourceOperation> g_hostResourceCreationOrder;
std::vector<HostResourceOperation> g_hostResourceDestructionOrder;
VkImageViewCreateInfo g_observedHostImageViewCreateInfo{};
VkSamplerCreateInfo g_observedSamplerCreateInfo{};
VkDescriptorSetLayoutBinding g_observedDescriptorBinding{};
VkDescriptorPoolSize g_observedDescriptorPoolSize{};
VkDescriptorImageInfo g_observedDescriptorImageInfo{};
VkFormat g_observedPipelineColorFormat = VK_FORMAT_UNDEFINED;
VkPipelineVertexInputStateCreateInfo g_observedVertexInputState{};
VkPipelineInputAssemblyStateCreateInfo g_observedInputAssemblyState{};
VkPipelineViewportStateCreateInfo g_observedViewportState{};
VkViewport g_observedViewport{};
VkRect2D g_observedScissor{};
VkPipelineColorBlendAttachmentState g_observedColorBlendAttachment{};
VkSampleCountFlagBits g_observedRasterizationSamples = VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;
std::vector<VkImageMemoryBarrier> g_observedHostBarriers;
std::vector<VkImageMemoryBarrier> g_observedSwapchainBarriers;
std::vector<VkPipelineStageFlags> g_observedHostSourceStages;
std::vector<VkPipelineStageFlags> g_observedHostDestinationStages;
std::vector<VkPipelineStageFlags> g_observedSwapchainSourceStages;
std::vector<VkPipelineStageFlags> g_observedSwapchainDestinationStages;
std::vector<VkAttachmentLoadOp> g_observedLoadOperations;
std::vector<VkAttachmentStoreOp> g_observedStoreOperations;
std::vector<VkImageView> g_observedRenderingImageViews;
std::vector<VkCommandBuffer> g_boundPipelineCommandBuffers;
std::vector<VkCommandBuffer> g_boundDescriptorCommandBuffers;
std::vector<std::uint32_t> g_drawVertexCounts;
std::vector<VkFence> g_waitedHostResourceFences;
VkResult g_hostResourceWaitResult = VK_SUCCESS;
std::uint32_t g_hostResourceWaitCallCount = 0u;
std::uint32_t g_destroyedSamplerCount = 0u;
std::uint32_t g_destroyedDescriptorSetLayoutCount = 0u;
std::uint32_t g_destroyedDescriptorPoolCount = 0u;
std::uint32_t g_destroyedPipelineLayoutCount = 0u;
std::uint32_t g_destroyedPipelineCount = 0u;
std::uint32_t g_destroyedShaderModuleCount = 0u;
std::uint32_t g_destroyedCommandPoolCount = 0u;
std::uint32_t g_observedCommandPoolQueueFamilyIndex = 0u;

/** Resets deterministic host-resource Vulkan interception state. */
void resetHostResourceState() {
    g_failedHostResourceOperation.reset();
    g_injectedHostResourceResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    g_hostResourceCreationOrder.clear();
    g_hostResourceDestructionOrder.clear();
    g_observedHostImageViewCreateInfo = {};
    g_observedSamplerCreateInfo = {};
    g_observedDescriptorBinding = {};
    g_observedDescriptorPoolSize = {};
    g_observedDescriptorImageInfo = {};
    g_observedPipelineColorFormat = VK_FORMAT_UNDEFINED;
    g_observedVertexInputState = {};
    g_observedInputAssemblyState = {};
    g_observedViewportState = {};
    g_observedViewport = {};
    g_observedScissor = {};
    g_observedColorBlendAttachment = {};
    g_observedRasterizationSamples = VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;
    g_observedHostBarriers.clear();
    g_observedSwapchainBarriers.clear();
    g_observedHostSourceStages.clear();
    g_observedHostDestinationStages.clear();
    g_observedSwapchainSourceStages.clear();
    g_observedSwapchainDestinationStages.clear();
    g_observedLoadOperations.clear();
    g_observedStoreOperations.clear();
    g_observedRenderingImageViews.clear();
    g_boundPipelineCommandBuffers.clear();
    g_boundDescriptorCommandBuffers.clear();
    g_drawVertexCounts.clear();
    g_waitedHostResourceFences.clear();
    g_hostResourceWaitResult = VK_SUCCESS;
    g_hostResourceWaitCallCount = 0u;
    g_createdImageViewCount = 0u;
    g_destroyedImageViewCount = 0u;
    g_destroyedSamplerCount = 0u;
    g_destroyedDescriptorSetLayoutCount = 0u;
    g_destroyedDescriptorPoolCount = 0u;
    g_destroyedPipelineLayoutCount = 0u;
    g_destroyedPipelineCount = 0u;
    g_destroyedShaderModuleCount = 0u;
    g_destroyedCommandPoolCount = 0u;
    g_observedCommandPoolQueueFamilyIndex = 0u;
}

/** Returns whether the named host-resource Vulkan operation should fail. */
bool shouldFailHostResourceOperation(HostResourceOperation operation) {
    g_hostResourceCreationOrder.push_back(operation);
    return g_failedHostResourceOperation == operation;
}

const std::array<VkImage, 3> g_hostResourceSwapchainImages{
    reinterpret_cast<VkImage>(0x6100u), reinterpret_cast<VkImage>(0x6101u),
    reinterpret_cast<VkImage>(0x6102u)};
const std::array<VkImageView, 3> g_hostResourceSwapchainImageViews{
    reinterpret_cast<VkImageView>(0x7101u), reinterpret_cast<VkImageView>(0x7102u),
    reinterpret_cast<VkImageView>(0x7103u)};

/** Builds valid creation input for a two-slot by three-image command matrix. */
barrieww::VulkanHostImagePresentationResources::CreateInfo makeHostResourceCreateInfo() {
    return {
        reinterpret_cast<VkDevice>(0x3000u),
        7u,
        reinterpret_cast<VkImage>(0x4000u),
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8A8_UNORM,
        VkExtent2D{1280u, 720u},
        g_hostResourceSwapchainImages,
        g_hostResourceSwapchainImageViews,
        2u,
    };
}

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

TEST_CASE("Presentation image format values map strictly to Vulkan formats",
          "[presentationRuntime][formatMapping]") {
    REQUIRE(static_cast<std::uint32_t>(barrieww::PresentationImageFormat::R8G8B8A8Unorm)
            == 1u);
    REQUIRE(static_cast<std::uint32_t>(barrieww::PresentationImageFormat::B8G8R8A8Unorm)
            == 2u);
    REQUIRE(barrieww::mapPresentationImageFormat(
                barrieww::PresentationImageFormat::R8G8B8A8Unorm)
            == VK_FORMAT_R8G8B8A8_UNORM);
    REQUIRE(barrieww::mapPresentationImageFormat(
                barrieww::PresentationImageFormat::B8G8R8A8Unorm)
            == VK_FORMAT_B8G8R8A8_UNORM);
    REQUIRE(barrieww::mapPresentationImageFormat(
                static_cast<barrieww::PresentationImageFormat>(UINT32_MAX))
            == VK_FORMAT_UNDEFINED);
}

TEST_CASE("Host image presentation boundary layouts remain fixed",
          "[presentationRuntime][layout]") {
    REQUIRE(sizeof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1) == 96u);
    REQUIRE(alignof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1) == 8u);
    REQUIRE(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                     instanceHandle) == 0u);
    REQUIRE(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                     physicalDeviceHandle) == 8u);
    REQUIRE(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                     logicalDeviceHandle) == 16u);
    REQUIRE(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                     surfaceHandle) == 24u);
    REQUIRE(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                     graphicsQueueHandle) == 32u);
    REQUIRE(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                     presentQueueHandle) == 40u);
    REQUIRE(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                     graphicsQueueFamilyIndex) == 48u);
    REQUIRE(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                     presentQueueFamilyIndex) == 52u);
    REQUIRE(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                     framebufferWidth) == 56u);
    REQUIRE(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                     framebufferHeight) == 60u);
    REQUIRE(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                     framesInFlightCount) == 64u);
    REQUIRE(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                     reservedFlags) == 68u);
    REQUIRE(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                     hostImageHandle) == 72u);
    REQUIRE(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                     hostImageFormatValue) == 80u);
    REQUIRE(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                     requestedSurfaceFormatValue) == 84u);
    REQUIRE(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                     hostImageWidth) == 88u);
    REQUIRE(offsetof(barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1,
                     hostImageHeight) == 92u);
    REQUIRE(sizeof(barrieww::NativePresentationDetachHostImageResourcesResultVersion1)
            == 8u);
    REQUIRE(alignof(barrieww::NativePresentationDetachHostImageResourcesResultVersion1)
            == 4u);
    REQUIRE(offsetof(barrieww::NativePresentationDetachHostImageResourcesResultVersion1,
                     vulkanResult) == 0u);
    REQUIRE(offsetof(barrieww::NativePresentationDetachHostImageResourcesResultVersion1,
                     reserved) == 4u);
}

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
    static_cast<void>(allocationCallbacks);
    if (shouldFailHostResourceOperation(HostResourceOperation::ImageView)) {
        return g_injectedHostResourceResult;
    }
    g_observedHostImageViewCreateInfo = *createInfo;
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
    g_hostResourceDestructionOrder.push_back(HostResourceOperation::ImageView);
    ++g_destroyedImageViewCount;
}

extern "C" VkResult VKAPI_CALL vkCreateSampler(
    VkDevice device, const VkSamplerCreateInfo* createInfo,
    const VkAllocationCallbacks* allocationCallbacks, VkSampler* sampler) {
    static_cast<void>(device);
    static_cast<void>(allocationCallbacks);
    if (shouldFailHostResourceOperation(HostResourceOperation::Sampler)) {
        return g_injectedHostResourceResult;
    }
    g_observedSamplerCreateInfo = *createInfo;
    *sampler = reinterpret_cast<VkSampler>(0x7100u);
    return VK_SUCCESS;
}

extern "C" void VKAPI_CALL vkDestroySampler(
    VkDevice device, VkSampler sampler, const VkAllocationCallbacks* allocationCallbacks) {
    static_cast<void>(device);
    static_cast<void>(sampler);
    static_cast<void>(allocationCallbacks);
    g_hostResourceDestructionOrder.push_back(HostResourceOperation::Sampler);
    ++g_destroyedSamplerCount;
}

extern "C" VkResult VKAPI_CALL vkCreateDescriptorSetLayout(
    VkDevice device, const VkDescriptorSetLayoutCreateInfo* createInfo,
    const VkAllocationCallbacks* allocationCallbacks,
    VkDescriptorSetLayout* descriptorSetLayout) {
    static_cast<void>(device);
    static_cast<void>(allocationCallbacks);
    if (shouldFailHostResourceOperation(HostResourceOperation::DescriptorSetLayout)) {
        return g_injectedHostResourceResult;
    }
    g_observedDescriptorBinding = createInfo->pBindings[0];
    *descriptorSetLayout = reinterpret_cast<VkDescriptorSetLayout>(0x7200u);
    return VK_SUCCESS;
}

extern "C" void VKAPI_CALL vkDestroyDescriptorSetLayout(
    VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
    const VkAllocationCallbacks* allocationCallbacks) {
    static_cast<void>(device);
    static_cast<void>(descriptorSetLayout);
    static_cast<void>(allocationCallbacks);
    g_hostResourceDestructionOrder.push_back(HostResourceOperation::DescriptorSetLayout);
    ++g_destroyedDescriptorSetLayoutCount;
}

extern "C" VkResult VKAPI_CALL vkCreateDescriptorPool(
    VkDevice device, const VkDescriptorPoolCreateInfo* createInfo,
    const VkAllocationCallbacks* allocationCallbacks, VkDescriptorPool* descriptorPool) {
    static_cast<void>(device);
    static_cast<void>(allocationCallbacks);
    if (shouldFailHostResourceOperation(HostResourceOperation::DescriptorPool)) {
        return g_injectedHostResourceResult;
    }
    g_observedDescriptorPoolSize = createInfo->pPoolSizes[0];
    *descriptorPool = reinterpret_cast<VkDescriptorPool>(0x7300u);
    return VK_SUCCESS;
}

extern "C" void VKAPI_CALL vkDestroyDescriptorPool(
    VkDevice device, VkDescriptorPool descriptorPool,
    const VkAllocationCallbacks* allocationCallbacks) {
    static_cast<void>(device);
    static_cast<void>(descriptorPool);
    static_cast<void>(allocationCallbacks);
    g_hostResourceDestructionOrder.push_back(HostResourceOperation::DescriptorPool);
    ++g_destroyedDescriptorPoolCount;
}

extern "C" VkResult VKAPI_CALL vkAllocateDescriptorSets(
    VkDevice device, const VkDescriptorSetAllocateInfo* allocateInfo,
    VkDescriptorSet* descriptorSets) {
    static_cast<void>(device);
    static_cast<void>(allocateInfo);
    if (shouldFailHostResourceOperation(HostResourceOperation::DescriptorSet)) {
        return g_injectedHostResourceResult;
    }
    descriptorSets[0] = reinterpret_cast<VkDescriptorSet>(0x7400u);
    return VK_SUCCESS;
}

extern "C" void VKAPI_CALL vkUpdateDescriptorSets(
    VkDevice device, std::uint32_t descriptorWriteCount,
    const VkWriteDescriptorSet* descriptorWrites, std::uint32_t descriptorCopyCount,
    const VkCopyDescriptorSet* descriptorCopies) {
    static_cast<void>(device);
    static_cast<void>(descriptorWriteCount);
    static_cast<void>(descriptorCopyCount);
    static_cast<void>(descriptorCopies);
    g_observedDescriptorImageInfo = descriptorWrites[0].pImageInfo[0];
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
    static_cast<void>(waitAll);
    static_cast<void>(timeout);
    ++g_hostResourceWaitCallCount;
    g_waitedHostResourceFences.assign(fences, fences + fenceCount);
    return g_hostResourceWaitResult;
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

extern "C" VkResult VKAPI_CALL vkCreatePipelineLayout(
    VkDevice device, const VkPipelineLayoutCreateInfo* createInfo,
    const VkAllocationCallbacks* allocationCallbacks, VkPipelineLayout* pipelineLayout) {
    static_cast<void>(device);
    static_cast<void>(createInfo);
    static_cast<void>(allocationCallbacks);
    if (shouldFailHostResourceOperation(HostResourceOperation::PipelineLayout)) {
        return g_injectedHostResourceResult;
    }
    *pipelineLayout = reinterpret_cast<VkPipelineLayout>(0x7500u);
    return VK_SUCCESS;
}

extern "C" void VKAPI_CALL vkDestroyPipelineLayout(
    VkDevice device, VkPipelineLayout pipelineLayout,
    const VkAllocationCallbacks* allocationCallbacks) {
    static_cast<void>(device);
    static_cast<void>(pipelineLayout);
    static_cast<void>(allocationCallbacks);
    g_hostResourceDestructionOrder.push_back(HostResourceOperation::PipelineLayout);
    ++g_destroyedPipelineLayoutCount;
}

extern "C" VkResult VKAPI_CALL vkCreateShaderModule(
    VkDevice device, const VkShaderModuleCreateInfo* createInfo,
    const VkAllocationCallbacks* allocationCallbacks, VkShaderModule* shaderModule) {
    static_cast<void>(device);
    static_cast<void>(allocationCallbacks);
    REQUIRE(createInfo->codeSize > 0u);
    REQUIRE(reinterpret_cast<std::uintptr_t>(createInfo->pCode) % alignof(std::uint32_t) == 0u);
    const HostResourceOperation operation =
        std::find(g_hostResourceCreationOrder.begin(), g_hostResourceCreationOrder.end(),
                  HostResourceOperation::VertexShaderModule)
                == g_hostResourceCreationOrder.end()
            ? HostResourceOperation::VertexShaderModule
            : HostResourceOperation::FragmentShaderModule;
    if (shouldFailHostResourceOperation(operation)) {
        return g_injectedHostResourceResult;
    }
    *shaderModule = reinterpret_cast<VkShaderModule>(static_cast<std::uintptr_t>(
        operation == HostResourceOperation::VertexShaderModule ? 0x7600u : 0x7601u));
    return VK_SUCCESS;
}

extern "C" void VKAPI_CALL vkDestroyShaderModule(
    VkDevice device, VkShaderModule shaderModule,
    const VkAllocationCallbacks* allocationCallbacks) {
    static_cast<void>(device);
    static_cast<void>(shaderModule);
    static_cast<void>(allocationCallbacks);
    ++g_destroyedShaderModuleCount;
}

extern "C" VkResult VKAPI_CALL vkCreateGraphicsPipelines(
    VkDevice device, VkPipelineCache pipelineCache, std::uint32_t createInfoCount,
    const VkGraphicsPipelineCreateInfo* createInfos,
    const VkAllocationCallbacks* allocationCallbacks, VkPipeline* pipelines) {
    static_cast<void>(device);
    static_cast<void>(pipelineCache);
    static_cast<void>(createInfoCount);
    static_cast<void>(allocationCallbacks);
    if (shouldFailHostResourceOperation(HostResourceOperation::GraphicsPipeline)) {
        return g_injectedHostResourceResult;
    }
    const auto* renderingCreateInfo =
        static_cast<const VkPipelineRenderingCreateInfo*>(createInfos[0].pNext);
    g_observedPipelineColorFormat = renderingCreateInfo->pColorAttachmentFormats[0];
    g_observedVertexInputState = *createInfos[0].pVertexInputState;
    g_observedInputAssemblyState = *createInfos[0].pInputAssemblyState;
    g_observedViewportState = *createInfos[0].pViewportState;
    g_observedViewport = createInfos[0].pViewportState->pViewports[0];
    g_observedScissor = createInfos[0].pViewportState->pScissors[0];
    g_observedRasterizationSamples = createInfos[0].pMultisampleState->rasterizationSamples;
    g_observedColorBlendAttachment = createInfos[0].pColorBlendState->pAttachments[0];
    pipelines[0] = reinterpret_cast<VkPipeline>(0x7700u);
    return VK_SUCCESS;
}

extern "C" void VKAPI_CALL vkDestroyPipeline(
    VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* allocationCallbacks) {
    static_cast<void>(device);
    static_cast<void>(pipeline);
    static_cast<void>(allocationCallbacks);
    g_hostResourceDestructionOrder.push_back(HostResourceOperation::GraphicsPipeline);
    ++g_destroyedPipelineCount;
}

extern "C" VkResult VKAPI_CALL vkCreateCommandPool(
    VkDevice device, const VkCommandPoolCreateInfo* createInfo,
    const VkAllocationCallbacks* allocationCallbacks, VkCommandPool* commandPool) {
    static_cast<void>(device);
    static_cast<void>(allocationCallbacks);
    if (shouldFailHostResourceOperation(HostResourceOperation::CommandPool)) {
        return g_injectedHostResourceResult;
    }
    g_observedCommandPoolQueueFamilyIndex = createInfo->queueFamilyIndex;
    *commandPool = reinterpret_cast<VkCommandPool>(0xA000u);
    return VK_SUCCESS;
}

extern "C" void VKAPI_CALL vkDestroyCommandPool(
    VkDevice device, VkCommandPool commandPool,
    const VkAllocationCallbacks* allocationCallbacks) {
    static_cast<void>(device);
    static_cast<void>(commandPool);
    static_cast<void>(allocationCallbacks);
    g_hostResourceDestructionOrder.push_back(HostResourceOperation::CommandPool);
    ++g_destroyedCommandPoolCount;
}

extern "C" VkResult VKAPI_CALL vkAllocateCommandBuffers(
    VkDevice device, const VkCommandBufferAllocateInfo* allocateInfo,
    VkCommandBuffer* commandBuffers) {
    static_cast<void>(device);
    if (shouldFailHostResourceOperation(HostResourceOperation::CommandBuffers)) {
        return g_injectedHostResourceResult;
    }
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
    if (shouldFailHostResourceOperation(HostResourceOperation::BeginCommandBuffer)) {
        return g_injectedHostResourceResult;
    }
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
    static_cast<void>(dependencyFlags);
    static_cast<void>(memoryBarrierCount);
    static_cast<void>(memoryBarriers);
    static_cast<void>(bufferMemoryBarrierCount);
    static_cast<void>(bufferMemoryBarriers);
    if (imageMemoryBarrierCount == 1u
        && imageMemoryBarriers[0].image == reinterpret_cast<VkImage>(0x4000u)) {
        g_observedHostBarriers.push_back(imageMemoryBarriers[0]);
        g_observedHostSourceStages.push_back(sourceStageMask);
        g_observedHostDestinationStages.push_back(destinationStageMask);
    } else if (imageMemoryBarrierCount == 1u) {
        g_observedSwapchainBarriers.push_back(imageMemoryBarriers[0]);
        g_observedSwapchainSourceStages.push_back(sourceStageMask);
        g_observedSwapchainDestinationStages.push_back(destinationStageMask);
    }
}

extern "C" void VKAPI_CALL vkCmdBeginRenderingKHR(
    VkCommandBuffer commandBuffer, const VkRenderingInfo* renderingInfo) {
    static_cast<void>(commandBuffer);
    g_observedLoadOperations.push_back(renderingInfo->pColorAttachments[0].loadOp);
    g_observedStoreOperations.push_back(renderingInfo->pColorAttachments[0].storeOp);
    g_observedRenderingImageViews.push_back(renderingInfo->pColorAttachments[0].imageView);
}

extern "C" void VKAPI_CALL vkCmdEndRenderingKHR(VkCommandBuffer commandBuffer) {
    static_cast<void>(commandBuffer);
}

extern "C" void VKAPI_CALL vkCmdBindPipeline(
    VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
    VkPipeline pipeline) {
    static_cast<void>(pipelineBindPoint);
    static_cast<void>(pipeline);
    g_boundPipelineCommandBuffers.push_back(commandBuffer);
}

extern "C" void VKAPI_CALL vkCmdBindDescriptorSets(
    VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
    VkPipelineLayout layout, std::uint32_t firstSet, std::uint32_t descriptorSetCount,
    const VkDescriptorSet* descriptorSets, std::uint32_t dynamicOffsetCount,
    const std::uint32_t* dynamicOffsets) {
    static_cast<void>(pipelineBindPoint);
    static_cast<void>(layout);
    static_cast<void>(firstSet);
    static_cast<void>(descriptorSetCount);
    static_cast<void>(descriptorSets);
    static_cast<void>(dynamicOffsetCount);
    static_cast<void>(dynamicOffsets);
    g_boundDescriptorCommandBuffers.push_back(commandBuffer);
}

extern "C" void VKAPI_CALL vkCmdDraw(
    VkCommandBuffer commandBuffer, std::uint32_t vertexCount,
    std::uint32_t instanceCount, std::uint32_t firstVertex,
    std::uint32_t firstInstance) {
    static_cast<void>(commandBuffer);
    REQUIRE(instanceCount == 1u);
    REQUIRE(firstVertex == 0u);
    REQUIRE(firstInstance == 0u);
    g_drawVertexCounts.push_back(vertexCount);
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
    if (shouldFailHostResourceOperation(HostResourceOperation::EndCommandBuffer)) {
        return g_injectedHostResourceResult;
    }
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

TEST_CASE("Host image resources reject invalid borrowed inputs and matrix overflow",
          "[presentationRuntime][hostResources]") {
    resetHostResourceState();
    auto createInfo = makeHostResourceCreateInfo();

    SECTION("null logical device") {
        createInfo.logicalDevice = VK_NULL_HANDLE;
    }
    SECTION("null host image") {
        createInfo.hostImage = VK_NULL_HANDLE;
    }
    SECTION("undefined format") {
        createInfo.hostImageFormat = VK_FORMAT_UNDEFINED;
    }
    SECTION("zero extent") {
        createInfo.extent.width = 0u;
    }
    SECTION("zero frame slots") {
        createInfo.frameSlotCount = 0u;
    }
    SECTION("null swapchain image") {
        static const std::array<VkImage, 1> invalidImages{VK_NULL_HANDLE};
        createInfo.swapchainImages = invalidImages;
    }
    SECTION("mismatched swapchain views") {
        createInfo.swapchainImageViews = createInfo.swapchainImageViews.first(2u);
    }
    SECTION("matrix size overflow") {
        createInfo.swapchainImages = std::span<const VkImage>{
            reinterpret_cast<const VkImage*>(0x1000u),
            std::numeric_limits<std::uint32_t>::max()};
        createInfo.swapchainImageViews = std::span<const VkImageView>{
            reinterpret_cast<const VkImageView*>(0x2000u),
            std::numeric_limits<std::uint32_t>::max()};
        createInfo.frameSlotCount = 2u;
    }

    const auto result = barrieww::VulkanHostImagePresentationResources::create(createInfo);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().vulkanResult == VK_ERROR_INITIALIZATION_FAILED);
    REQUIRE(g_hostResourceCreationOrder.empty());
}

TEST_CASE("Host image resources create the exact pipeline and prerecorded matrix",
          "[presentationRuntime][hostResources]") {
    STATIC_REQUIRE(
        barrieww::VulkanHostImagePresentationResources::s_requiredSwapchainImageUsageFlags
        == (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT));
    resetHostResourceState();
    auto result = barrieww::VulkanHostImagePresentationResources::create(
        makeHostResourceCreateInfo());
    REQUIRE(result.has_value());
    auto resources = std::move(result.value());

    const std::vector<HostResourceOperation> expectedCreationPrefix{
        HostResourceOperation::ImageView,
        HostResourceOperation::Sampler,
        HostResourceOperation::DescriptorSetLayout,
        HostResourceOperation::DescriptorPool,
        HostResourceOperation::DescriptorSet,
        HostResourceOperation::PipelineLayout,
        HostResourceOperation::VertexShaderModule,
        HostResourceOperation::FragmentShaderModule,
        HostResourceOperation::GraphicsPipeline,
        HostResourceOperation::CommandPool,
        HostResourceOperation::CommandBuffers,
    };
    REQUIRE(g_hostResourceCreationOrder.size() >= expectedCreationPrefix.size());
    REQUIRE(std::equal(expectedCreationPrefix.begin(), expectedCreationPrefix.end(),
                       g_hostResourceCreationOrder.begin()));
    REQUIRE(g_observedHostImageViewCreateInfo.image == reinterpret_cast<VkImage>(0x4000u));
    REQUIRE(g_observedHostImageViewCreateInfo.format == VK_FORMAT_R8G8B8A8_UNORM);
    REQUIRE(g_observedHostImageViewCreateInfo.subresourceRange.aspectMask
            == VK_IMAGE_ASPECT_COLOR_BIT);
    REQUIRE(g_observedHostImageViewCreateInfo.subresourceRange.levelCount == 1u);
    REQUIRE(g_observedHostImageViewCreateInfo.subresourceRange.layerCount == 1u);
    REQUIRE(g_observedSamplerCreateInfo.magFilter == VK_FILTER_NEAREST);
    REQUIRE(g_observedSamplerCreateInfo.minFilter == VK_FILTER_NEAREST);
    REQUIRE(g_observedSamplerCreateInfo.addressModeU == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    REQUIRE(g_observedSamplerCreateInfo.addressModeV == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    REQUIRE(g_observedSamplerCreateInfo.addressModeW == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    REQUIRE(g_observedDescriptorBinding.binding == 0u);
    REQUIRE(g_observedDescriptorBinding.descriptorType
            == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    REQUIRE(g_observedDescriptorBinding.descriptorCount == 1u);
    REQUIRE(g_observedDescriptorBinding.stageFlags == VK_SHADER_STAGE_FRAGMENT_BIT);
    REQUIRE(g_observedDescriptorImageInfo.imageLayout == VK_IMAGE_LAYOUT_GENERAL);
    REQUIRE(g_observedPipelineColorFormat == VK_FORMAT_B8G8R8A8_UNORM);
    REQUIRE(g_observedVertexInputState.vertexBindingDescriptionCount == 0u);
    REQUIRE(g_observedVertexInputState.vertexAttributeDescriptionCount == 0u);
    REQUIRE(g_observedInputAssemblyState.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    REQUIRE(g_observedViewport.width == 1280.0f);
    REQUIRE(g_observedViewport.height == 720.0f);
    REQUIRE(g_observedScissor.extent.width == 1280u);
    REQUIRE(g_observedScissor.extent.height == 720u);
    REQUIRE(g_observedRasterizationSamples == VK_SAMPLE_COUNT_1_BIT);
    REQUIRE(g_observedColorBlendAttachment.blendEnable == VK_FALSE);
    REQUIRE(g_destroyedShaderModuleCount == 2u);
    REQUIRE(g_observedCommandPoolQueueFamilyIndex == 7u);

    for (std::uint32_t frameSlotIndex = 0u; frameSlotIndex < 2u; ++frameSlotIndex) {
        for (std::uint32_t imageIndex = 0u; imageIndex < 3u; ++imageIndex) {
            const std::size_t matrixIndex = frameSlotIndex * 3u + imageIndex;
            REQUIRE(resources.commandBuffer(frameSlotIndex, imageIndex)
                    == reinterpret_cast<VkCommandBuffer>(0xB000u + matrixIndex));
        }
    }
    REQUIRE(g_observedHostBarriers.size() == 6u);
    REQUIRE(g_observedSwapchainBarriers.size() == 12u);
    REQUIRE(g_boundPipelineCommandBuffers.size() == 6u);
    REQUIRE(g_boundDescriptorCommandBuffers.size() == 6u);
    REQUIRE(g_drawVertexCounts == std::vector<std::uint32_t>(6u, 3u));
    REQUIRE(g_observedLoadOperations == std::vector<VkAttachmentLoadOp>(6u,
                                                                       VK_ATTACHMENT_LOAD_OP_DONT_CARE));
    REQUIRE(g_observedStoreOperations == std::vector<VkAttachmentStoreOp>(6u,
                                                                          VK_ATTACHMENT_STORE_OP_STORE));
    REQUIRE(g_observedRenderingImageViews
            == std::vector<VkImageView>{
                g_hostResourceSwapchainImageViews[0],
                g_hostResourceSwapchainImageViews[1],
                g_hostResourceSwapchainImageViews[2],
                g_hostResourceSwapchainImageViews[0],
                g_hostResourceSwapchainImageViews[1],
                g_hostResourceSwapchainImageViews[2]});
    for (std::size_t matrixIndex = 0u; matrixIndex < 6u; ++matrixIndex) {
        const auto& hostBarrier = g_observedHostBarriers[matrixIndex];
        REQUIRE(hostBarrier.oldLayout == VK_IMAGE_LAYOUT_GENERAL);
        REQUIRE(hostBarrier.newLayout == VK_IMAGE_LAYOUT_GENERAL);
        REQUIRE(hostBarrier.srcAccessMask == VK_ACCESS_MEMORY_WRITE_BIT);
        REQUIRE(hostBarrier.dstAccessMask == VK_ACCESS_SHADER_READ_BIT);
        REQUIRE(hostBarrier.subresourceRange.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT);
        REQUIRE(hostBarrier.subresourceRange.levelCount == 1u);
        REQUIRE(hostBarrier.subresourceRange.layerCount == 1u);
        REQUIRE(g_observedHostSourceStages[matrixIndex]
                == (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                    | VK_PIPELINE_STAGE_TRANSFER_BIT));
        REQUIRE(g_observedHostDestinationStages[matrixIndex]
                == VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        REQUIRE(g_observedSwapchainBarriers[matrixIndex * 2u].oldLayout
                == VK_IMAGE_LAYOUT_UNDEFINED);
        REQUIRE(g_observedSwapchainBarriers[matrixIndex * 2u].newLayout
                == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        REQUIRE(g_observedSwapchainBarriers[matrixIndex * 2u].srcAccessMask == 0u);
        REQUIRE(g_observedSwapchainBarriers[matrixIndex * 2u].dstAccessMask
                == VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
        REQUIRE(g_observedSwapchainSourceStages[matrixIndex * 2u]
                == VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
        REQUIRE(g_observedSwapchainDestinationStages[matrixIndex * 2u]
                == VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        REQUIRE(g_observedSwapchainBarriers[matrixIndex * 2u + 1u].oldLayout
                == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        REQUIRE(g_observedSwapchainBarriers[matrixIndex * 2u + 1u].newLayout
                == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        REQUIRE(g_observedSwapchainBarriers[matrixIndex * 2u + 1u].srcAccessMask
                == VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
        REQUIRE(g_observedSwapchainBarriers[matrixIndex * 2u + 1u].dstAccessMask == 0u);
        REQUIRE(g_observedSwapchainSourceStages[matrixIndex * 2u + 1u]
                == VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        REQUIRE(g_observedSwapchainDestinationStages[matrixIndex * 2u + 1u]
                == VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    }
}

TEST_CASE("Host image resource creation preserves every practical Vulkan failure",
          "[presentationRuntime][hostResources]") {
    const std::array failedOperations{
        HostResourceOperation::ImageView,
        HostResourceOperation::Sampler,
        HostResourceOperation::DescriptorSetLayout,
        HostResourceOperation::DescriptorPool,
        HostResourceOperation::DescriptorSet,
        HostResourceOperation::PipelineLayout,
        HostResourceOperation::VertexShaderModule,
        HostResourceOperation::FragmentShaderModule,
        HostResourceOperation::GraphicsPipeline,
        HostResourceOperation::CommandPool,
        HostResourceOperation::CommandBuffers,
        HostResourceOperation::BeginCommandBuffer,
        HostResourceOperation::EndCommandBuffer,
    };

    for (const HostResourceOperation failedOperation : failedOperations) {
        DYNAMIC_SECTION("operation " << static_cast<int>(failedOperation)) {
            resetHostResourceState();
            g_failedHostResourceOperation = failedOperation;
            g_injectedHostResourceResult = VK_ERROR_DEVICE_LOST;
            const auto result = barrieww::VulkanHostImagePresentationResources::create(
                makeHostResourceCreateInfo());
            REQUIRE_FALSE(result.has_value());
            REQUIRE(result.error().vulkanResult == VK_ERROR_DEVICE_LOST);
            REQUIRE(g_destroyedImageViewCount
                    == (failedOperation == HostResourceOperation::ImageView ? 0u : 1u));
            REQUIRE(g_destroyedSamplerCount
                    == (failedOperation <= HostResourceOperation::Sampler ? 0u : 1u));
            REQUIRE(g_destroyedDescriptorSetLayoutCount
                    == (failedOperation <= HostResourceOperation::DescriptorSetLayout ? 0u : 1u));
            REQUIRE(g_destroyedDescriptorPoolCount
                    == (failedOperation <= HostResourceOperation::DescriptorPool ? 0u : 1u));
            REQUIRE(g_destroyedPipelineLayoutCount
                    == (failedOperation <= HostResourceOperation::PipelineLayout ? 0u : 1u));
            REQUIRE(g_destroyedPipelineCount
                    == (failedOperation <= HostResourceOperation::GraphicsPipeline ? 0u : 1u));
            REQUIRE(g_destroyedCommandPoolCount
                    == (failedOperation <= HostResourceOperation::CommandPool ? 0u : 1u));
            std::vector<HostResourceOperation> expectedDestructionOrder;
            if (g_destroyedCommandPoolCount != 0u) {
                expectedDestructionOrder.push_back(HostResourceOperation::CommandPool);
            }
            if (g_destroyedPipelineCount != 0u) {
                expectedDestructionOrder.push_back(HostResourceOperation::GraphicsPipeline);
            }
            if (g_destroyedPipelineLayoutCount != 0u) {
                expectedDestructionOrder.push_back(HostResourceOperation::PipelineLayout);
            }
            if (g_destroyedDescriptorPoolCount != 0u) {
                expectedDestructionOrder.push_back(HostResourceOperation::DescriptorPool);
            }
            if (g_destroyedDescriptorSetLayoutCount != 0u) {
                expectedDestructionOrder.push_back(HostResourceOperation::DescriptorSetLayout);
            }
            if (g_destroyedSamplerCount != 0u) {
                expectedDestructionOrder.push_back(HostResourceOperation::Sampler);
            }
            if (g_destroyedImageViewCount != 0u) {
                expectedDestructionOrder.push_back(HostResourceOperation::ImageView);
            }
            REQUIRE(g_hostResourceDestructionOrder == expectedDestructionOrder);
            const std::uint32_t expectedDestroyedShaderModuleCount =
                failedOperation < HostResourceOperation::FragmentShaderModule
                    ? 0u
                    : (failedOperation == HostResourceOperation::FragmentShaderModule ? 1u : 2u);
            REQUIRE(g_destroyedShaderModuleCount == expectedDestroyedShaderModuleCount);
        }
    }
}

TEST_CASE("Host image resource detach waits submitted fences and is transactional",
          "[presentationRuntime][hostResources]") {
    resetHostResourceState();
    auto creationResult = barrieww::VulkanHostImagePresentationResources::create(
        makeHostResourceCreateInfo());
    REQUIRE(creationResult.has_value());
    auto resources = std::move(creationResult.value());
    const std::array<VkFence, 3> submittedFences{
        reinterpret_cast<VkFence>(0x9100u), reinterpret_cast<VkFence>(0x9101u),
        reinterpret_cast<VkFence>(0x9102u)};

    g_hostResourceWaitResult = VK_ERROR_DEVICE_LOST;
    auto detachResult = resources.destroyAfterSubmittedFrames(
        submittedFences, submittedFences[1]);
    REQUIRE_FALSE(detachResult.has_value());
    REQUIRE(detachResult.error() == VK_ERROR_DEVICE_LOST);
    REQUIRE(g_waitedHostResourceFences
            == std::vector<VkFence>{submittedFences[0], submittedFences[2]});
    REQUIRE(g_destroyedCommandPoolCount == 0u);
    REQUIRE(g_destroyedImageViewCount == 0u);

    g_hostResourceWaitResult = VK_SUCCESS;
    detachResult = resources.destroyAfterSubmittedFrames(submittedFences,
                                                         submittedFences[1]);
    REQUIRE(detachResult.has_value());
    REQUIRE(g_destroyedCommandPoolCount == 1u);
    REQUIRE(g_destroyedPipelineCount == 1u);
    REQUIRE(g_destroyedImageViewCount == 1u);
    REQUIRE(g_hostResourceDestructionOrder
            == std::vector<HostResourceOperation>{
                HostResourceOperation::CommandPool,
                HostResourceOperation::GraphicsPipeline,
                HostResourceOperation::PipelineLayout,
                HostResourceOperation::DescriptorPool,
                HostResourceOperation::DescriptorSetLayout,
                HostResourceOperation::Sampler,
                HostResourceOperation::ImageView});
    const std::uint32_t waitCallCount = g_hostResourceWaitCallCount;
    REQUIRE(resources.destroyAfterSubmittedFrames(submittedFences, submittedFences[1])
            .has_value());
    REQUIRE(g_hostResourceWaitCallCount == waitCallCount);
    REQUIRE(g_destroyedCommandPoolCount == 1u);
}
