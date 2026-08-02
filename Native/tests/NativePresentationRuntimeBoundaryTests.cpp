#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

#include <vulkan/vulkan.h>

#include "BarriEww/Interoperability/NativeHostImagePresentationRuntimeCreateInfoVersion1.hpp"
#include "BarriEww/Interoperability/NativePresentationDetachHostImageResourcesResultVersion1.hpp"
#include "BarriEww/Interoperability/NativePresentationRuntimeBoundary.hpp"
#include "BarriEww/Interoperability/PresentationImageFormat.hpp"
#include "BarriEww/Vulkan/VulkanPresentationImageFormatMapping.hpp"
#include "BarriEww/Vulkan/VulkanHostImageCompositionResources.hpp"
#include "BarriEww/Vulkan/VulkanHostImagePresentationResources.hpp"
#include "BarriEww/Vulkan/VulkanPresentationRuntime.hpp"
#include "../src/Interoperability/NativePresentationRuntimeBoundaryImplementation.hpp"
#include "../src/Vulkan/VulkanPresentationRuntimeTestSeam.hpp"

using barrieww::NativePresentationRuntimeCreateInfoVersion1;
using barrieww::NativePresentationRuntimeCreateResultVersion1;
using barrieww::NativePresentationRuntimeOperationResult;
using barrieww::NativeHostImagePresentationRuntimeCreateInfoVersion1;

template <typename ValueType>
concept HasPresentationCreationCheckpointOperation = requires(ValueType value) {
    value.creationCheckpointOperation;
};

static_assert(!HasPresentationCreationCheckpointOperation<
              barrieww::VulkanPresentationRuntime::CreateInfo>);

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
VkCompositeAlphaFlagsKHR g_supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
VkExtent2D g_surfaceCurrentExtent{1280u, 720u};
VkResult g_surfaceFormatCountResult = VK_SUCCESS;
std::uint32_t g_minImageCount = 2u;
std::uint32_t g_maxImageCount = 0u;
bool g_offerMailboxPresentMode = true;
bool g_offerPreferredFormat = true;
bool g_offerRequestedHostFormat = true;
VkColorSpaceKHR g_requestedHostColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
std::uint32_t g_swapchainImageCount = 3u;
std::uint32_t g_createdImageViewCount = 0u;
std::uint32_t g_destroyedImageViewCount = 0u;
std::vector<VkImageView> g_destroyedImageViews;
std::uint32_t g_destroyedSwapchainCount = 0u;
VkSharingMode g_observedSharingMode = VK_SHARING_MODE_EXCLUSIVE;
std::uint32_t g_observedQueueFamilyCount = 0u;
VkImageUsageFlags g_observedSwapchainImageUsage = 0u;
VkFormat g_observedSwapchainImageFormat = VK_FORMAT_UNDEFINED;
VkColorSpaceKHR g_observedSwapchainImageColorSpace = VK_COLOR_SPACE_MAX_ENUM_KHR;
VkExtent2D g_observedSwapchainExtent{};
VkCompositeAlphaFlagBitsKHR g_observedCompositeAlpha =
    VK_COMPOSITE_ALPHA_FLAG_BITS_MAX_ENUM_KHR;
VkResult g_acquireResult = VK_SUCCESS;
std::uint32_t g_acquireCallCount = 0u;
std::vector<VkSemaphore> g_acquireSemaphores;
VkResult g_submitResult = VK_SUCCESS;
VkResult g_presentResult = VK_SUCCESS;
std::uint32_t g_acquiredImageIndex = 0u;
std::uint32_t g_submitCallCount = 0u;
std::uint32_t g_presentCallCount = 0u;
std::uint32_t g_clearImageCallCount = 0u;
std::uint32_t g_recordedCommandBufferCount = 0u;
VkCommandBuffer g_observedSubmittedCommandBuffer = VK_NULL_HANDLE;
std::array<float, 4> g_observedClearColor{};
std::uint32_t g_createdFenceCount = 0u;
std::uint32_t g_destroyedFenceCount = 0u;
std::uint32_t g_createdSemaphoreCount = 0u;
std::uint32_t g_destroyedSemaphoreCount = 0u;
std::vector<VkFence> g_signaledFences;
std::vector<std::vector<VkFence>> g_fenceWaitCalls;
std::vector<VkResult> g_injectedFenceWaitResults;
std::size_t g_injectedFenceWaitResultIndex = 0u;
VkResult g_resetFenceResult = VK_SUCCESS;
std::vector<VkFence> g_resetFences;
bool g_offerDynamicRenderingFunctions = true;
std::optional<barrieww::testing::VulkanPresentationRuntimeCreationCheckpoint>
    g_throwingPresentationCreationCheckpoint;

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

enum class HostCommandEventType {
    BeginCommandBuffer,
    HostBarrier,
    SwapchainToColorBarrier,
    BeginRendering,
    BindPipeline,
    BindDescriptors,
    Draw,
    EndRendering,
    SwapchainToPresentBarrier,
    EndCommandBuffer,
};

/** Ordered command event with barrier, rendering, attachment, and scalar payload slots. */
using HostCommandEvent =
    std::tuple<VkCommandBuffer, HostCommandEventType, VkPipelineStageFlags,
               VkPipelineStageFlags, VkImageMemoryBarrier, VkRenderingInfo,
               VkRenderingAttachmentInfo, std::uint32_t>;

std::optional<HostResourceOperation> g_failedHostResourceOperation;
std::uint32_t g_failedHostResourceInvocationOrdinal = 1u;
std::array<std::uint32_t,
           static_cast<std::size_t>(HostResourceOperation::EndCommandBuffer) + 1u>
    g_hostResourceOperationInvocationCounts{};
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
std::vector<HostCommandEvent> g_hostCommandEvents;
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

/**
 * @note ThreadSafety: Test-thread confined; callers serialize access to global mock state.
 * @brief Resets deterministic host-resource Vulkan interception state.
 * @warning MemoryOwnership: Clears test-owned containers and does not destroy Vulkan objects.
 */
void resetHostResourceState() {
    g_failedHostResourceOperation.reset();
    g_failedHostResourceInvocationOrdinal = 1u;
    g_hostResourceOperationInvocationCounts.fill(0u);
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
    g_hostCommandEvents.clear();
    g_waitedHostResourceFences.clear();
    g_hostResourceWaitResult = VK_SUCCESS;
    g_hostResourceWaitCallCount = 0u;
    g_createdImageViewCount = 0u;
    g_destroyedImageViewCount = 0u;
    g_destroyedImageViews.clear();
    g_destroyedSamplerCount = 0u;
    g_destroyedDescriptorSetLayoutCount = 0u;
    g_destroyedDescriptorPoolCount = 0u;
    g_destroyedPipelineLayoutCount = 0u;
    g_destroyedPipelineCount = 0u;
    g_destroyedShaderModuleCount = 0u;
    g_destroyedCommandPoolCount = 0u;
    g_observedCommandPoolQueueFamilyIndex = 0u;
    g_offerDynamicRenderingFunctions = true;
}

/**
 * @note ThreadSafety: Test-thread confined; mutates global mock creation history.
 * @brief Records an attempted host-resource operation and applies deterministic injection
 *        when its one-based invocation ordinal matches the configured ordinal.
 * @param HostResourceOperation operation Vulkan operation being intercepted
 * @return bool True when the selected operation must return the injected failure
 * @warning MemoryOwnership: Does not acquire, release, or transfer Vulkan object ownership.
 */
bool shouldFailHostResourceOperation(HostResourceOperation operation) {
    g_hostResourceCreationOrder.push_back(operation);
    const std::size_t operationIndex = static_cast<std::size_t>(operation);
    ++g_hostResourceOperationInvocationCounts[operationIndex];
    return g_failedHostResourceOperation == operation
        && g_hostResourceOperationInvocationCounts[operationIndex]
            == g_failedHostResourceInvocationOrdinal;
}

const std::array<VkImage, 3> g_hostResourceSwapchainImages{
    reinterpret_cast<VkImage>(0x6100u), reinterpret_cast<VkImage>(0x6101u),
    reinterpret_cast<VkImage>(0x6102u)};
const std::array<VkImageView, 3> g_hostResourceSwapchainImageViews{
    reinterpret_cast<VkImageView>(0x7101u), reinterpret_cast<VkImageView>(0x7102u),
    reinterpret_cast<VkImageView>(0x7103u)};

/**
 * @note ThreadSafety: Read-only and concurrency-safe after static initialization.
 * @brief Builds valid creation input for a two-slot by three-image command matrix.
 * @return barrieww::VulkanHostImagePresentationResources::CreateInfo Borrowed test handles
 * @warning MemoryOwnership: Returned spans borrow static test arrays and remain valid for
 *          the process lifetime; no Vulkan object ownership is transferred.
 */
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
    g_supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    g_surfaceCurrentExtent = VkExtent2D{1280u, 720u};
    g_surfaceFormatCountResult = VK_SUCCESS;
    g_minImageCount = 2u;
    g_maxImageCount = 0u;
    g_offerMailboxPresentMode = true;
    g_offerPreferredFormat = true;
    g_offerRequestedHostFormat = true;
    g_requestedHostColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    g_swapchainImageCount = 3u;
    g_createdImageViewCount = 0u;
    g_destroyedImageViewCount = 0u;
    g_destroyedImageViews.clear();
    g_destroyedSwapchainCount = 0u;
    g_observedSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    g_observedQueueFamilyCount = 0u;
    g_observedSwapchainImageUsage = 0u;
    g_observedSwapchainImageFormat = VK_FORMAT_UNDEFINED;
    g_observedSwapchainImageColorSpace = VK_COLOR_SPACE_MAX_ENUM_KHR;
    g_observedSwapchainExtent = {};
    g_observedCompositeAlpha = VK_COMPOSITE_ALPHA_FLAG_BITS_MAX_ENUM_KHR;
    g_acquireResult = VK_SUCCESS;
    g_acquireCallCount = 0u;
    g_acquireSemaphores.clear();
    g_submitResult = VK_SUCCESS;
    g_presentResult = VK_SUCCESS;
    g_acquiredImageIndex = 0u;
    g_submitCallCount = 0u;
    g_presentCallCount = 0u;
    g_clearImageCallCount = 0u;
    g_recordedCommandBufferCount = 0u;
    g_observedSubmittedCommandBuffer = VK_NULL_HANDLE;
    g_observedClearColor = {};
    g_createdFenceCount = 0u;
    g_destroyedFenceCount = 0u;
    g_createdSemaphoreCount = 0u;
    g_destroyedSemaphoreCount = 0u;
    g_signaledFences.clear();
    g_fenceWaitCalls.clear();
    g_injectedFenceWaitResults.clear();
    g_injectedFenceWaitResultIndex = 0u;
    g_resetFenceResult = VK_SUCCESS;
    g_resetFences.clear();
    g_offerDynamicRenderingFunctions = true;
    g_throwingPresentationCreationCheckpoint.reset();
    barrieww::testing::g_vulkanPresentationRuntimeCreationCheckpointOperation = nullptr;
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

/**
 * @note ThreadSafety: Test-thread confined through deterministic global mock state.
 * @brief Throws only at the configured swapchain creation checkpoint.
 * @param barrieww::testing::VulkanPresentationRuntimeCreationCheckpoint creationCheckpoint
 *        Current creation checkpoint
 * @warning MemoryOwnership: Does not acquire or release any Vulkan object.
 */
void injectPresentationCreationException(
    barrieww::testing::VulkanPresentationRuntimeCreationCheckpoint creationCheckpoint) {
    if (g_throwingPresentationCreationCheckpoint == creationCheckpoint) {
        throw std::runtime_error{"Injected presentation creation failure"};
    }
}

namespace {

/**
 * @note ThreadSafety: Returns only local scalar state and is fully thread-safe.
 * @brief Builds valid host-image creation input with exact BGRA UNORM surface output.
 * @return NativeHostImagePresentationRuntimeCreateInfoVersion1 Valid borrowed-handle input
 * @warning MemoryOwnership: Contains opaque borrowed handles and transfers no ownership.
 */
NativeHostImagePresentationRuntimeCreateInfoVersion1 makeHostImageCreateInfo() {
    NativeHostImagePresentationRuntimeCreateInfoVersion1 createInfo{};
    createInfo.instanceHandle = 1u;
    createInfo.physicalDeviceHandle = 2u;
    createInfo.logicalDeviceHandle = 3u;
    createInfo.surfaceHandle = 4u;
    createInfo.graphicsQueueHandle = 5u;
    createInfo.presentQueueHandle = 5u;
    createInfo.graphicsQueueFamilyIndex = 7u;
    createInfo.presentQueueFamilyIndex = 7u;
    createInfo.framebufferWidth = 1280u;
    createInfo.framebufferHeight = 720u;
    createInfo.framesInFlightCount = 2u;
    createInfo.hostImageHandle = 0x4000u;
    createInfo.hostImageFormatValue = 1u;
    createInfo.requestedSurfaceFormatValue = 2u;
    createInfo.hostImageWidth = 1280u;
    createInfo.hostImageHeight = 720u;
    return createInfo;
}

} // namespace

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
    surfaceCapabilities->currentExtent = g_surfaceCurrentExtent;
    surfaceCapabilities->minImageExtent = VkExtent2D{1u, 1u};
    surfaceCapabilities->maxImageExtent = VkExtent2D{4096u, 4096u};
    surfaceCapabilities->currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    surfaceCapabilities->supportedUsageFlags = g_supportedUsageFlags;
    surfaceCapabilities->supportedCompositeAlpha = g_supportedCompositeAlpha;
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
    if (g_offerRequestedHostFormat) {
        availableFormats.push_back(
            VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, g_requestedHostColorSpace});
    }
    if (formats == nullptr) {
        if (g_surfaceFormatCountResult != VK_SUCCESS) {
            return g_surfaceFormatCountResult;
        }
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
    g_observedSwapchainImageUsage = createInfo->imageUsage;
    g_observedSwapchainImageFormat = createInfo->imageFormat;
    g_observedSwapchainImageColorSpace = createInfo->imageColorSpace;
    g_observedSwapchainExtent = createInfo->imageExtent;
    g_observedCompositeAlpha = createInfo->compositeAlpha;
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
    g_destroyedImageViews.push_back(imageView);
    ++g_destroyedImageViewCount;
}

/**
 * @note ThreadSafety: Test-thread confined through global mock state.
 * @brief Intercepts sampler creation and captures filtering and addressing policy.
 * @param VkDevice device Borrowed logical-device handle
 * @param const VkSamplerCreateInfo* createInfo Borrowed sampler creation structure
 * @param const VkAllocationCallbacks* allocationCallbacks Borrowed callbacks, expected null
 * @param VkSampler* sampler Receives an opaque test sampler handle on success
 * @return VkResult Injected failure or VK_SUCCESS
 * @warning MemoryOwnership: Returns a test handle tracked until vkDestroySampler; all input
 *          pointers and the logical device remain borrowed.
 */
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

/**
 * @note ThreadSafety: Test-thread confined through global mock state.
 * @brief Records destruction of one intercepted sampler handle.
 * @param VkDevice device Borrowed logical-device handle
 * @param VkSampler sampler Opaque test sampler handle being released
 * @param const VkAllocationCallbacks* allocationCallbacks Borrowed callbacks, expected null
 * @warning MemoryOwnership: Marks the test sampler released without touching borrowed inputs.
 */
extern "C" void VKAPI_CALL vkDestroySampler(
    VkDevice device, VkSampler sampler, const VkAllocationCallbacks* allocationCallbacks) {
    static_cast<void>(device);
    static_cast<void>(sampler);
    static_cast<void>(allocationCallbacks);
    g_hostResourceDestructionOrder.push_back(HostResourceOperation::Sampler);
    ++g_destroyedSamplerCount;
}

/**
 * @note ThreadSafety: Test-thread confined through global mock state.
 * @brief Intercepts descriptor-set-layout creation and captures its fixed binding.
 * @param VkDevice device Borrowed logical-device handle
 * @param const VkDescriptorSetLayoutCreateInfo* createInfo Borrowed creation structure
 * @param const VkAllocationCallbacks* allocationCallbacks Borrowed callbacks, expected null
 * @param VkDescriptorSetLayout* descriptorSetLayout Receives an opaque test layout handle
 * @return VkResult Injected failure or VK_SUCCESS
 * @warning MemoryOwnership: Returns a tracked test handle; all inputs remain borrowed.
 */
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

/**
 * @note ThreadSafety: Test-thread confined through global mock state.
 * @brief Records destruction of one intercepted descriptor-set-layout handle.
 * @param VkDevice device Borrowed logical-device handle
 * @param VkDescriptorSetLayout descriptorSetLayout Opaque test layout handle being released
 * @param const VkAllocationCallbacks* allocationCallbacks Borrowed callbacks, expected null
 * @warning MemoryOwnership: Marks the test handle released without touching borrowed inputs.
 */
extern "C" void VKAPI_CALL vkDestroyDescriptorSetLayout(
    VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
    const VkAllocationCallbacks* allocationCallbacks) {
    static_cast<void>(device);
    static_cast<void>(descriptorSetLayout);
    static_cast<void>(allocationCallbacks);
    g_hostResourceDestructionOrder.push_back(HostResourceOperation::DescriptorSetLayout);
    ++g_destroyedDescriptorSetLayoutCount;
}

/**
 * @note ThreadSafety: Test-thread confined through global mock state.
 * @brief Intercepts descriptor-pool creation and captures the fixed pool size.
 * @param VkDevice device Borrowed logical-device handle
 * @param const VkDescriptorPoolCreateInfo* createInfo Borrowed creation structure
 * @param const VkAllocationCallbacks* allocationCallbacks Borrowed callbacks, expected null
 * @param VkDescriptorPool* descriptorPool Receives an opaque test pool handle
 * @return VkResult Injected failure or VK_SUCCESS
 * @warning MemoryOwnership: Returns a tracked test handle; all inputs remain borrowed.
 */
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

/**
 * @note ThreadSafety: Test-thread confined through global mock state.
 * @brief Records destruction of one intercepted descriptor-pool handle.
 * @param VkDevice device Borrowed logical-device handle
 * @param VkDescriptorPool descriptorPool Opaque test pool handle being released
 * @param const VkAllocationCallbacks* allocationCallbacks Borrowed callbacks, expected null
 * @warning MemoryOwnership: Marks the pool and its test descriptor set released.
 */
extern "C" void VKAPI_CALL vkDestroyDescriptorPool(
    VkDevice device, VkDescriptorPool descriptorPool,
    const VkAllocationCallbacks* allocationCallbacks) {
    static_cast<void>(device);
    static_cast<void>(descriptorPool);
    static_cast<void>(allocationCallbacks);
    g_hostResourceDestructionOrder.push_back(HostResourceOperation::DescriptorPool);
    ++g_destroyedDescriptorPoolCount;
}

/**
 * @note ThreadSafety: Test-thread confined through global mock state.
 * @brief Intercepts allocation of the immutable host-image descriptor set.
 * @param VkDevice device Borrowed logical-device handle
 * @param const VkDescriptorSetAllocateInfo* allocateInfo Borrowed allocation structure
 * @param VkDescriptorSet* descriptorSets Receives one opaque test descriptor-set handle
 * @return VkResult Injected failure or VK_SUCCESS
 * @warning MemoryOwnership: The returned test set follows intercepted descriptor-pool lifetime.
 */
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

/**
 * @note ThreadSafety: Test-thread confined through global mock state.
 * @brief Captures the immutable host sampled-image descriptor update.
 * @param VkDevice device Borrowed logical-device handle
 * @param std::uint32_t descriptorWriteCount Number of borrowed write structures
 * @param const VkWriteDescriptorSet* descriptorWrites Borrowed descriptor writes
 * @param std::uint32_t descriptorCopyCount Number of borrowed copy structures
 * @param const VkCopyDescriptorSet* descriptorCopies Borrowed descriptor copies
 * @warning MemoryOwnership: Copies observed scalar fields and retains no borrowed pointer.
 */
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

/**
 * @note ThreadSafety: Test-thread confined through deterministic global mock state.
 * @brief Resolves only the dynamic-rendering device procedures used by host resources.
 * @param VkDevice device Borrowed logical-device handle
 * @param const char* procedureName Null-terminated Vulkan procedure name
 * @return PFN_vkVoidFunction Mock procedure address, or null when unavailable
 * @warning MemoryOwnership: Returns a static function address and retains no input pointer.
 */
extern "C" PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(
    VkDevice device, const char* procedureName) {
    static_cast<void>(device);
    if (!g_offerDynamicRenderingFunctions) {
        return nullptr;
    }
    const std::string_view requestedProcedure{procedureName};
    if (requestedProcedure == "vkCmdBeginRenderingKHR") {
        return reinterpret_cast<PFN_vkVoidFunction>(&vkCmdBeginRenderingKHR);
    }
    if (requestedProcedure == "vkCmdEndRenderingKHR") {
        return reinterpret_cast<PFN_vkVoidFunction>(&vkCmdEndRenderingKHR);
    }
    return nullptr;
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
    *semaphore = reinterpret_cast<VkSemaphore>(
        static_cast<std::uintptr_t>(0x8000u + g_createdSemaphoreCount));
    ++g_createdSemaphoreCount;
    return VK_SUCCESS;
}

extern "C" void VKAPI_CALL vkDestroySemaphore(
    VkDevice device, VkSemaphore semaphore,
    const VkAllocationCallbacks* allocationCallbacks) {
    static_cast<void>(device);
    static_cast<void>(semaphore);
    static_cast<void>(allocationCallbacks);
    ++g_destroyedSemaphoreCount;
}

extern "C" VkResult VKAPI_CALL vkCreateFence(
    VkDevice device, const VkFenceCreateInfo* createInfo,
    const VkAllocationCallbacks* allocationCallbacks, VkFence* fence) {
    static_cast<void>(device);
    static_cast<void>(createInfo);
    static_cast<void>(allocationCallbacks);
    *fence = reinterpret_cast<VkFence>(
        static_cast<std::uintptr_t>(0x9000u + g_createdFenceCount));
    g_signaledFences.push_back(*fence);
    ++g_createdFenceCount;
    return VK_SUCCESS;
}

extern "C" void VKAPI_CALL vkDestroyFence(
    VkDevice device, VkFence fence, const VkAllocationCallbacks* allocationCallbacks) {
    static_cast<void>(device);
    static_cast<void>(allocationCallbacks);
    std::erase(g_signaledFences, fence);
    ++g_destroyedFenceCount;
}

extern "C" VkResult VKAPI_CALL vkWaitForFences(
    VkDevice device, std::uint32_t fenceCount, const VkFence* fences, VkBool32 waitAll,
    std::uint64_t timeout) {
    static_cast<void>(device);
    static_cast<void>(waitAll);
    static_cast<void>(timeout);
    ++g_hostResourceWaitCallCount;
    g_waitedHostResourceFences.assign(fences, fences + fenceCount);
    g_fenceWaitCalls.emplace_back(fences, fences + fenceCount);
    if (g_injectedFenceWaitResultIndex < g_injectedFenceWaitResults.size()) {
        return g_injectedFenceWaitResults[g_injectedFenceWaitResultIndex++];
    }
    return g_hostResourceWaitResult;
}

extern "C" VkResult VKAPI_CALL vkResetFences(
    VkDevice device, std::uint32_t fenceCount, const VkFence* fences) {
    static_cast<void>(device);
    g_resetFences.insert(g_resetFences.end(), fences, fences + fenceCount);
    if (g_resetFenceResult != VK_SUCCESS) {
        return g_resetFenceResult;
    }
    for (std::uint32_t fenceIndex = 0u; fenceIndex < fenceCount; ++fenceIndex) {
        std::erase(g_signaledFences, fences[fenceIndex]);
    }
    return g_resetFenceResult;
}

extern "C" VkResult VKAPI_CALL vkAcquireNextImageKHR(
    VkDevice device, VkSwapchainKHR swapchain, std::uint64_t timeout,
    VkSemaphore semaphore, VkFence fence, std::uint32_t* imageIndex) {
    static_cast<void>(device);
    static_cast<void>(swapchain);
    static_cast<void>(timeout);
    static_cast<void>(fence);
    ++g_acquireCallCount;
    g_acquireSemaphores.push_back(semaphore);
    *imageIndex = g_acquiredImageIndex;
    return g_acquireResult;
}

extern "C" VkResult VKAPI_CALL vkQueueSubmit(
    VkQueue queue, std::uint32_t submitCount, const VkSubmitInfo* submitInfo, VkFence fence) {
    static_cast<void>(queue);
    static_cast<void>(submitCount);
    g_observedSubmittedCommandBuffer = submitInfo->commandBufferCount == 0u
        ? VK_NULL_HANDLE
        : submitInfo->pCommandBuffers[0];
    static_cast<void>(fence);
    ++g_submitCallCount;
    if (g_submitResult == VK_SUCCESS
        && std::ranges::find(g_signaledFences, fence) == g_signaledFences.end()) {
        g_signaledFences.push_back(fence);
    }
    return g_submitResult;
}

extern "C" VkResult VKAPI_CALL vkQueuePresentKHR(
    VkQueue queue, const VkPresentInfoKHR* presentInfo) {
    static_cast<void>(queue);
    static_cast<void>(presentInfo);
    ++g_presentCallCount;
    return g_presentResult;
}

/**
 * @note ThreadSafety: Test-thread confined through global mock state.
 * @brief Intercepts pipeline-layout creation for deterministic failure coverage.
 * @param VkDevice device Borrowed logical-device handle
 * @param const VkPipelineLayoutCreateInfo* createInfo Borrowed creation structure
 * @param const VkAllocationCallbacks* allocationCallbacks Borrowed callbacks, expected null
 * @param VkPipelineLayout* pipelineLayout Receives an opaque test pipeline-layout handle
 * @return VkResult Injected failure or VK_SUCCESS
 * @warning MemoryOwnership: Returns a tracked test handle; all inputs remain borrowed.
 */
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

/**
 * @note ThreadSafety: Test-thread confined through global mock state.
 * @brief Records destruction of one intercepted pipeline-layout handle.
 * @param VkDevice device Borrowed logical-device handle
 * @param VkPipelineLayout pipelineLayout Opaque test layout handle being released
 * @param const VkAllocationCallbacks* allocationCallbacks Borrowed callbacks, expected null
 * @warning MemoryOwnership: Marks the test handle released without touching borrowed inputs.
 */
extern "C" void VKAPI_CALL vkDestroyPipelineLayout(
    VkDevice device, VkPipelineLayout pipelineLayout,
    const VkAllocationCallbacks* allocationCallbacks) {
    static_cast<void>(device);
    static_cast<void>(pipelineLayout);
    static_cast<void>(allocationCallbacks);
    g_hostResourceDestructionOrder.push_back(HostResourceOperation::PipelineLayout);
    ++g_destroyedPipelineLayoutCount;
}

/**
 * @note ThreadSafety: Test-thread confined through global mock state.
 * @brief Intercepts shader-module creation and verifies embedded bytecode alignment.
 * @param VkDevice device Borrowed logical-device handle
 * @param const VkShaderModuleCreateInfo* createInfo Borrowed shader creation structure
 * @param const VkAllocationCallbacks* allocationCallbacks Borrowed callbacks, expected null
 * @param VkShaderModule* shaderModule Receives an opaque test shader-module handle
 * @return VkResult Injected failure or VK_SUCCESS
 * @warning MemoryOwnership: Returns a tracked test handle and retains no bytecode pointer.
 */
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

/**
 * @note ThreadSafety: Test-thread confined through global mock state.
 * @brief Records destruction of one temporary intercepted shader-module handle.
 * @param VkDevice device Borrowed logical-device handle
 * @param VkShaderModule shaderModule Opaque test shader-module handle being released
 * @param const VkAllocationCallbacks* allocationCallbacks Borrowed callbacks, expected null
 * @warning MemoryOwnership: Marks the test handle released without touching borrowed inputs.
 */
extern "C" void VKAPI_CALL vkDestroyShaderModule(
    VkDevice device, VkShaderModule shaderModule,
    const VkAllocationCallbacks* allocationCallbacks) {
    static_cast<void>(device);
    static_cast<void>(shaderModule);
    static_cast<void>(allocationCallbacks);
    ++g_destroyedShaderModuleCount;
}

/**
 * @note ThreadSafety: Test-thread confined through global mock state.
 * @brief Intercepts graphics-pipeline creation and captures exact fixed rendering state.
 * @param VkDevice device Borrowed logical-device handle
 * @param VkPipelineCache pipelineCache Borrowed cache handle, expected null
 * @param std::uint32_t createInfoCount Number of borrowed pipeline creation structures
 * @param const VkGraphicsPipelineCreateInfo* createInfos Borrowed pipeline structures
 * @param const VkAllocationCallbacks* allocationCallbacks Borrowed callbacks, expected null
 * @param VkPipeline* pipelines Receives one opaque test pipeline handle
 * @return VkResult Injected failure or VK_SUCCESS
 * @warning MemoryOwnership: Returns a tracked test handle and retains no input pointer.
 */
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

/**
 * @note ThreadSafety: Test-thread confined through global mock state.
 * @brief Records destruction of one intercepted graphics-pipeline handle.
 * @param VkDevice device Borrowed logical-device handle
 * @param VkPipeline pipeline Opaque test pipeline handle being released
 * @param const VkAllocationCallbacks* allocationCallbacks Borrowed callbacks, expected null
 * @warning MemoryOwnership: Marks the test handle released without touching borrowed inputs.
 */
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
    static_cast<void>(beginInfo);
    g_hostCommandEvents.emplace_back(
        commandBuffer, HostCommandEventType::BeginCommandBuffer, 0u, 0u,
        VkImageMemoryBarrier{}, VkRenderingInfo{}, VkRenderingAttachmentInfo{}, 0u);
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
    static_cast<void>(dependencyFlags);
    static_cast<void>(memoryBarrierCount);
    static_cast<void>(memoryBarriers);
    static_cast<void>(bufferMemoryBarrierCount);
    static_cast<void>(bufferMemoryBarriers);
    if (imageMemoryBarrierCount == 1u
        && imageMemoryBarriers[0].image == reinterpret_cast<VkImage>(0x4000u)) {
        g_hostCommandEvents.emplace_back(
            commandBuffer, HostCommandEventType::HostBarrier, sourceStageMask,
            destinationStageMask, imageMemoryBarriers[0], VkRenderingInfo{},
            VkRenderingAttachmentInfo{}, 0u);
    } else if (imageMemoryBarrierCount == 1u) {
        const HostCommandEventType eventType =
            imageMemoryBarriers[0].newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                ? HostCommandEventType::SwapchainToPresentBarrier
                : HostCommandEventType::SwapchainToColorBarrier;
        g_hostCommandEvents.emplace_back(
            commandBuffer, eventType, sourceStageMask, destinationStageMask,
            imageMemoryBarriers[0], VkRenderingInfo{}, VkRenderingAttachmentInfo{}, 0u);
    }
}

/**
 * @note ThreadSafety: Test-thread confined through global mock state.
 * @brief Captures complete dynamic-rendering and color-attachment structures while
 *        replacing their borrowed pointers with explicit presence flags.
 * @param VkCommandBuffer commandBuffer Borrowed recording command-buffer handle
 * @param const VkRenderingInfo* renderingInfo Borrowed dynamic-rendering structure
 * @warning MemoryOwnership: Copies observed structures and retains no input pointer.
 */
extern "C" void VKAPI_CALL vkCmdBeginRenderingKHR(
    VkCommandBuffer commandBuffer, const VkRenderingInfo* renderingInfo) {
    VkRenderingInfo capturedRenderingInfo = *renderingInfo;
    const std::uint32_t attachmentPointerFlags =
        (renderingInfo->pColorAttachments != nullptr ? 0x1u : 0u)
        | (renderingInfo->pDepthAttachment != nullptr ? 0x2u : 0u)
        | (renderingInfo->pStencilAttachment != nullptr ? 0x4u : 0u);
    capturedRenderingInfo.pColorAttachments = nullptr;
    capturedRenderingInfo.pDepthAttachment = nullptr;
    capturedRenderingInfo.pStencilAttachment = nullptr;
    g_hostCommandEvents.emplace_back(
        commandBuffer, HostCommandEventType::BeginRendering, 0u, 0u,
        VkImageMemoryBarrier{}, capturedRenderingInfo,
        renderingInfo->pColorAttachments[0], attachmentPointerFlags);
}

/**
 * @note ThreadSafety: Test-thread confined; does not mutate shared ownership state.
 * @brief Records the end of one dynamic-rendering scope in command order.
 * @param VkCommandBuffer commandBuffer Borrowed recording command-buffer handle
 * @warning MemoryOwnership: Does not acquire, release, or retain the command buffer.
 */
extern "C" void VKAPI_CALL vkCmdEndRenderingKHR(VkCommandBuffer commandBuffer) {
    g_hostCommandEvents.emplace_back(
        commandBuffer, HostCommandEventType::EndRendering, 0u, 0u,
        VkImageMemoryBarrier{}, VkRenderingInfo{}, VkRenderingAttachmentInfo{}, 0u);
}

/**
 * @note ThreadSafety: Test-thread confined through global mock state.
 * @brief Records the command buffer receiving the fixed graphics-pipeline bind.
 * @param VkCommandBuffer commandBuffer Borrowed recording command-buffer handle
 * @param VkPipelineBindPoint pipelineBindPoint Pipeline binding domain
 * @param VkPipeline pipeline Borrowed intercepted graphics-pipeline handle
 * @warning MemoryOwnership: Copies handles for assertions and retains no Vulkan ownership.
 */
extern "C" void VKAPI_CALL vkCmdBindPipeline(
    VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
    VkPipeline pipeline) {
    static_cast<void>(pipelineBindPoint);
    static_cast<void>(pipeline);
    g_hostCommandEvents.emplace_back(
        commandBuffer, HostCommandEventType::BindPipeline, 0u, 0u,
        VkImageMemoryBarrier{}, VkRenderingInfo{}, VkRenderingAttachmentInfo{}, 0u);
}

/**
 * @note ThreadSafety: Test-thread confined through global mock state.
 * @brief Records the command buffer receiving the host descriptor-set bind.
 * @param VkCommandBuffer commandBuffer Borrowed recording command-buffer handle
 * @param VkPipelineBindPoint pipelineBindPoint Pipeline binding domain
 * @param VkPipelineLayout layout Borrowed pipeline-layout handle
 * @param std::uint32_t firstSet First descriptor-set index
 * @param std::uint32_t descriptorSetCount Number of borrowed descriptor-set handles
 * @param const VkDescriptorSet* descriptorSets Borrowed descriptor-set handles
 * @param std::uint32_t dynamicOffsetCount Number of dynamic offsets
 * @param const std::uint32_t* dynamicOffsets Borrowed dynamic offsets
 * @warning MemoryOwnership: Copies the command-buffer handle and retains no input pointer.
 */
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
    g_hostCommandEvents.emplace_back(
        commandBuffer, HostCommandEventType::BindDescriptors, 0u, 0u,
        VkImageMemoryBarrier{}, VkRenderingInfo{}, VkRenderingAttachmentInfo{}, 0u);
}

/**
 * @note ThreadSafety: Test-thread confined through global mock state.
 * @brief Verifies and records one fullscreen-triangle draw command.
 * @param VkCommandBuffer commandBuffer Borrowed recording command-buffer handle
 * @param std::uint32_t vertexCount Number of vertices
 * @param std::uint32_t instanceCount Number of instances
 * @param std::uint32_t firstVertex First vertex index
 * @param std::uint32_t firstInstance First instance index
 * @warning MemoryOwnership: Does not acquire, release, or retain Vulkan object ownership.
 */
extern "C" void VKAPI_CALL vkCmdDraw(
    VkCommandBuffer commandBuffer, std::uint32_t vertexCount,
    std::uint32_t instanceCount, std::uint32_t firstVertex,
    std::uint32_t firstInstance) {
    REQUIRE(instanceCount == 1u);
    REQUIRE(firstVertex == 0u);
    REQUIRE(firstInstance == 0u);
    g_hostCommandEvents.emplace_back(
        commandBuffer, HostCommandEventType::Draw, 0u, 0u, VkImageMemoryBarrier{},
        VkRenderingInfo{}, VkRenderingAttachmentInfo{}, vertexCount);
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
    g_hostCommandEvents.emplace_back(
        commandBuffer, HostCommandEventType::EndCommandBuffer, 0u, 0u,
        VkImageMemoryBarrier{}, VkRenderingInfo{}, VkRenderingAttachmentInfo{}, 0u);
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
    REQUIRE(g_observedCompositeAlpha == VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR);

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

        const std::uint32_t acquireCallCountAfterFailure = g_acquireCallCount;
        const std::size_t fenceWaitCallCountAfterFailure = g_fenceWaitCalls.size();
        const std::size_t resetFenceCountAfterFailure = g_resetFences.size();
        const std::uint32_t submitCallCountAfterFailure = g_submitCallCount;
        const std::uint32_t presentCallCountAfterFailure = g_presentCallCount;
        for (std::uint32_t subsequentBeginIndex = 0u; subsequentBeginIndex < 2u;
             ++subsequentBeginIndex) {
            REQUIRE(barriEwwBeginPresentationFrameVersion1(runtimeAddress, 1280u, 720u,
                                                           &beginResult, &priorMetrics)
                    == NativePresentationRuntimeOperationResult::Success);
            REQUIRE(beginResult.frameStatusValue
                    == static_cast<std::uint32_t>(
                        barrieww::VulkanPresentationRuntime::FrameStatus::RecreateRequired));
            REQUIRE(beginResult.vulkanResult == VK_ERROR_DEVICE_LOST);
            REQUIRE(g_acquireCallCount == acquireCallCountAfterFailure);
            REQUIRE(g_fenceWaitCalls.size() == fenceWaitCallCountAfterFailure);
            REQUIRE(g_resetFences.size() == resetFenceCountAfterFailure);
            REQUIRE(g_submitCallCount == submitCallCountAfterFailure);
            REQUIRE(g_presentCallCount == presentCallCountAfterFailure);
        }
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

    SECTION("suboptimal acquisition remains open after successful fence transitions") {
        g_acquireResult = VK_SUBOPTIMAL_KHR;
        g_acquiredImageIndex = 2u;
        REQUIRE(barriEwwBeginPresentationFrameVersion1(runtimeAddress, 1280u, 720u,
                                                       &beginResult, &priorMetrics)
                == NativePresentationRuntimeOperationResult::Success);
        REQUIRE(beginResult.frameStatusValue
                == static_cast<std::uint32_t>(
                    barrieww::VulkanPresentationRuntime::FrameStatus::Suboptimal));
        REQUIRE(beginResult.vulkanResult == VK_SUBOPTIMAL_KHR);
        REQUIRE(beginResult.imageIndex == 2u);
        REQUIRE(barriEwwSubmitPresentationFrameVersion1(runtimeAddress, 0u, &submitResult)
                == NativePresentationRuntimeOperationResult::Success);
        REQUIRE(g_submitCallCount == 1u);
    }

    REQUIRE(barriEwwDestroyPresentationRuntimeVersion1(runtimeAddress)
            == NativePresentationRuntimeOperationResult::Success);
}

TEST_CASE("Presentation frame preserves image ownership when its prior fence wait fails",
          "[presentationRuntime][fenceFailure]") {
    resetPresentationState();
    const std::uint64_t runtimeAddress = openRuntime();
    barrieww::NativePresentationBeginFrameResultVersion1 beginResult{};
    barrieww::NativePresentationFrameMetricsVersion1 priorMetrics{};
    barrieww::NativePresentationSubmitFrameResultVersion1 submitResult{};

    g_acquiredImageIndex = 0u;
    REQUIRE(barriEwwBeginPresentationFrameVersion1(runtimeAddress, 1280u, 720u,
                                                   &beginResult, &priorMetrics)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(barriEwwSubmitPresentationFrameVersion1(runtimeAddress, 0u, &submitResult)
            == NativePresentationRuntimeOperationResult::Success);
    const std::size_t resetFenceCountBeforeFailure = g_resetFences.size();

    g_fenceWaitCalls.clear();
    g_injectedFenceWaitResults = {VK_SUCCESS, VK_ERROR_DEVICE_LOST};
    g_injectedFenceWaitResultIndex = 0u;
    REQUIRE(barriEwwBeginPresentationFrameVersion1(runtimeAddress, 1280u, 720u,
                                                   &beginResult, &priorMetrics)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(beginResult.frameStatusValue
            == static_cast<std::uint32_t>(
                barrieww::VulkanPresentationRuntime::FrameStatus::RecreateRequired));
    REQUIRE(beginResult.vulkanResult == VK_ERROR_DEVICE_LOST);
    REQUIRE(g_fenceWaitCalls
            == std::vector<std::vector<VkFence>>{
                {reinterpret_cast<VkFence>(0x9001u)},
                {reinterpret_cast<VkFence>(0x9000u)}});
    REQUIRE(g_resetFences.size() == resetFenceCountBeforeFailure);
    REQUIRE(barriEwwSubmitPresentationFrameVersion1(runtimeAddress, 0u, &submitResult)
            == NativePresentationRuntimeOperationResult::InvalidArgument);
    REQUIRE(g_submitCallCount == 1u);

    const std::uint32_t acquireCallCountAfterFailure = g_acquireCallCount;
    const std::size_t fenceWaitCallCountAfterFailure = g_fenceWaitCalls.size();
    const std::size_t resetFenceCountAfterFailure = g_resetFences.size();
    g_injectedFenceWaitResults.clear();
    REQUIRE(barriEwwBeginPresentationFrameVersion1(runtimeAddress, 1280u, 720u,
                                                   &beginResult, &priorMetrics)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(beginResult.frameStatusValue
            == static_cast<std::uint32_t>(
                barrieww::VulkanPresentationRuntime::FrameStatus::RecreateRequired));
    REQUIRE(beginResult.vulkanResult == VK_ERROR_DEVICE_LOST);
    REQUIRE(g_acquireCallCount == acquireCallCountAfterFailure);
    REQUIRE(g_fenceWaitCalls.size() == fenceWaitCallCountAfterFailure);
    REQUIRE(g_resetFences.size() == resetFenceCountAfterFailure);
    REQUIRE(g_acquireSemaphores
            == std::vector<VkSemaphore>{reinterpret_cast<VkSemaphore>(0x8000u),
                                        reinterpret_cast<VkSemaphore>(0x8002u)});

    REQUIRE(barriEwwDestroyPresentationRuntimeVersion1(runtimeAddress)
            == NativePresentationRuntimeOperationResult::Success);
}

TEST_CASE("Presentation frame does not publish image ownership when fence reset fails",
          "[presentationRuntime][fenceFailure]") {
    resetPresentationState();
    const std::uint64_t runtimeAddress = openRuntime();
    barrieww::NativePresentationBeginFrameResultVersion1 beginResult{};
    barrieww::NativePresentationFrameMetricsVersion1 priorMetrics{};
    barrieww::NativePresentationSubmitFrameResultVersion1 submitResult{};
    g_acquiredImageIndex = 1u;
    g_resetFenceResult = VK_ERROR_DEVICE_LOST;

    REQUIRE(barriEwwBeginPresentationFrameVersion1(runtimeAddress, 1280u, 720u,
                                                   &beginResult, &priorMetrics)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(beginResult.frameStatusValue
            == static_cast<std::uint32_t>(
                barrieww::VulkanPresentationRuntime::FrameStatus::RecreateRequired));
    REQUIRE(beginResult.vulkanResult == VK_ERROR_DEVICE_LOST);
    REQUIRE(g_resetFences
            == std::vector<VkFence>{reinterpret_cast<VkFence>(0x9000u)});
    REQUIRE(std::ranges::find(g_signaledFences, reinterpret_cast<VkFence>(0x9000u))
            != g_signaledFences.end());
    REQUIRE(barriEwwSubmitPresentationFrameVersion1(runtimeAddress, 0u, &submitResult)
            == NativePresentationRuntimeOperationResult::InvalidArgument);
    REQUIRE(g_submitCallCount == 0u);

    const std::uint32_t acquireCallCountAfterFailure = g_acquireCallCount;
    const std::size_t fenceWaitCallCountAfterFailure = g_fenceWaitCalls.size();
    const std::size_t resetFenceCountAfterFailure = g_resetFences.size();
    g_resetFenceResult = VK_SUCCESS;
    REQUIRE(barriEwwBeginPresentationFrameVersion1(runtimeAddress, 1280u, 720u,
                                                   &beginResult, &priorMetrics)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(beginResult.frameStatusValue
            == static_cast<std::uint32_t>(
                barrieww::VulkanPresentationRuntime::FrameStatus::RecreateRequired));
    REQUIRE(beginResult.vulkanResult == VK_ERROR_DEVICE_LOST);
    REQUIRE(g_acquireCallCount == acquireCallCountAfterFailure);
    REQUIRE(g_fenceWaitCalls.size() == fenceWaitCallCountAfterFailure);
    REQUIRE(g_resetFences.size() == resetFenceCountAfterFailure);
    REQUIRE(g_acquireSemaphores
            == std::vector<VkSemaphore>{reinterpret_cast<VkSemaphore>(0x8000u)});
    REQUIRE(barriEwwSubmitPresentationFrameVersion1(runtimeAddress, 0u, &submitResult)
            == NativePresentationRuntimeOperationResult::InvalidArgument);

    REQUIRE(barriEwwDestroyPresentationRuntimeVersion1(runtimeAddress)
            == NativePresentationRuntimeOperationResult::Success);
}

TEST_CASE("Presentation runtime selects a supported composite alpha deterministically",
          "[presentationRuntime][compositeAlpha]") {
    resetPresentationState();
    g_supportedCompositeAlpha =
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR | VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    const auto createInfo = makeCreateInfo();
    NativePresentationRuntimeCreateResultVersion1 createResult{};

    REQUIRE(barriEwwCreatePresentationRuntimeVersion1(&createInfo, &createResult)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(g_observedCompositeAlpha == VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR);
    REQUIRE(barriEwwDestroyPresentationRuntimeVersion1(createResult.runtimeAddress)
            == NativePresentationRuntimeOperationResult::Success);
}

TEST_CASE("Presentation runtime rejects a surface without composite alpha support",
          "[presentationRuntime][compositeAlpha]") {
    resetPresentationState();
    g_supportedCompositeAlpha = 0u;
    const auto createInfo = makeCreateInfo();
    NativePresentationRuntimeCreateResultVersion1 createResult{};

    REQUIRE(barriEwwCreatePresentationRuntimeVersion1(&createInfo, &createResult)
            == NativePresentationRuntimeOperationResult::UnsupportedSurface);
    REQUIRE(createResult.vulkanResult == VK_SUCCESS);
    REQUIRE(createResult.runtimeAddress == 0u);
    REQUIRE(g_destroyedSwapchainCount == 0u);
}

TEST_CASE("Presentation runtime creation rolls back Vulkan ownership on exceptions",
          "[presentationRuntime][creationRollback]") {
    resetPresentationState();
    resetHostResourceState();
    barrieww::VulkanPresentationRuntime::CreateInfo createInfo{
        reinterpret_cast<VkPhysicalDevice>(2u),
        reinterpret_cast<VkDevice>(3u),
        reinterpret_cast<VkSurfaceKHR>(4u),
        reinterpret_cast<VkQueue>(5u),
        reinterpret_cast<VkQueue>(5u),
        0u,
        0u,
        1280u,
        720u,
        2u,
    };
    barrieww::testing::g_vulkanPresentationRuntimeCreationCheckpointOperation =
        &injectPresentationCreationException;

    SECTION("immediately after swapchain creation") {
        g_throwingPresentationCreationCheckpoint =
            barrieww::testing::VulkanPresentationRuntimeCreationCheckpoint::
                AfterSwapchainCreation;
        REQUIRE_THROWS_AS(barrieww::VulkanPresentationRuntime::create(createInfo),
                          std::runtime_error);
        REQUIRE(g_destroyedSwapchainCount == 1u);
        REQUIRE(g_destroyedImageViewCount == 0u);
        REQUIRE(g_destroyedSemaphoreCount == 0u);
        REQUIRE(g_destroyedFenceCount == 0u);
        REQUIRE(g_destroyedCommandPoolCount == 0u);
    }

    SECTION("after image views and one frame slot") {
        g_throwingPresentationCreationCheckpoint =
            barrieww::testing::VulkanPresentationRuntimeCreationCheckpoint::
                AfterFirstFrameSlotCreation;
        REQUIRE_THROWS_AS(barrieww::VulkanPresentationRuntime::create(createInfo),
                          std::runtime_error);
        REQUIRE(g_destroyedSwapchainCount == 1u);
        REQUIRE(g_destroyedImageViewCount == 3u);
        REQUIRE(g_destroyedSemaphoreCount == 2u);
        REQUIRE(g_destroyedFenceCount == 1u);
        REQUIRE(g_destroyedCommandPoolCount == 0u);
    }

    SECTION("after command pool creation") {
        g_throwingPresentationCreationCheckpoint =
            barrieww::testing::VulkanPresentationRuntimeCreationCheckpoint::
                AfterCommandPoolCreation;
        REQUIRE_THROWS_AS(barrieww::VulkanPresentationRuntime::create(createInfo),
                          std::runtime_error);
        REQUIRE(g_destroyedSwapchainCount == 1u);
        REQUIRE(g_destroyedImageViewCount == 3u);
        REQUIRE(g_destroyedSemaphoreCount == 4u);
        REQUIRE(g_destroyedFenceCount == 2u);
        REQUIRE(g_destroyedCommandPoolCount == 1u);
    }
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
    SECTION("command-buffer count overflow") {
        createInfo.frameSlotCount = std::numeric_limits<std::uint32_t>::max();
    }

    const auto result = barrieww::VulkanHostImagePresentationResources::create(createInfo);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().vulkanResult == VK_ERROR_INITIALIZATION_FAILED);
    REQUIRE(g_hostResourceCreationOrder.empty());
}

TEST_CASE("Host image composition resources own the common draw path",
          "[presentationRuntime][hostComposition]") {
    resetHostResourceState();
    {
        auto result = barrieww::VulkanHostImageCompositionResources::create({
            reinterpret_cast<VkDevice>(0x3000u),
            reinterpret_cast<VkImage>(0x4000u),
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_B8G8R8A8_UNORM,
            VkExtent2D{1280u, 720u},
        });
        REQUIRE(result.has_value());
        auto resources = std::move(result.value());
        resources.recordCommands(
            reinterpret_cast<VkCommandBuffer>(0xB100u),
            reinterpret_cast<VkImageView>(0x7101u));

        const std::vector<HostResourceOperation> expectedCreationOrder{
            HostResourceOperation::ImageView,
            HostResourceOperation::Sampler,
            HostResourceOperation::DescriptorSetLayout,
            HostResourceOperation::DescriptorPool,
            HostResourceOperation::DescriptorSet,
            HostResourceOperation::PipelineLayout,
            HostResourceOperation::VertexShaderModule,
            HostResourceOperation::FragmentShaderModule,
            HostResourceOperation::GraphicsPipeline,
        };
        REQUIRE(g_hostResourceCreationOrder == expectedCreationOrder);
        const std::array expectedEventTypes{
            HostCommandEventType::BeginRendering,
            HostCommandEventType::BindPipeline,
            HostCommandEventType::BindDescriptors,
            HostCommandEventType::Draw,
            HostCommandEventType::EndRendering,
        };
        REQUIRE(g_hostCommandEvents.size() == expectedEventTypes.size());
        for (std::size_t eventIndex = 0u; eventIndex < expectedEventTypes.size();
             ++eventIndex) {
            REQUIRE(std::get<0>(g_hostCommandEvents[eventIndex])
                    == reinterpret_cast<VkCommandBuffer>(0xB100u));
            REQUIRE(std::get<1>(g_hostCommandEvents[eventIndex])
                    == expectedEventTypes[eventIndex]);
        }
    }
    REQUIRE(g_destroyedPipelineCount == 1u);
    REQUIRE(g_destroyedPipelineLayoutCount == 1u);
    REQUIRE(g_destroyedDescriptorPoolCount == 1u);
    REQUIRE(g_destroyedDescriptorSetLayoutCount == 1u);
    REQUIRE(g_destroyedSamplerCount == 1u);
    REQUIRE(g_destroyedImageViewCount == 1u);
}

TEST_CASE("Host image resources create the exact pipeline and prerecorded matrix",
          "[presentationRuntime][hostResources]") {
    STATIC_REQUIRE(
        barrieww::VulkanHostImagePresentationResources::s_requiredSwapchainImageUsageFlags
        == (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT));
    resetHostResourceState();
    {
        auto result = barrieww::VulkanHostImagePresentationResources::create(
            makeHostResourceCreateInfo());
        REQUIRE(result.has_value());
        {
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
            REQUIRE(g_observedHostImageViewCreateInfo.image
                    == reinterpret_cast<VkImage>(0x4000u));
            REQUIRE(g_observedHostImageViewCreateInfo.format == VK_FORMAT_R8G8B8A8_UNORM);
            REQUIRE(g_observedHostImageViewCreateInfo.subresourceRange.aspectMask
                    == VK_IMAGE_ASPECT_COLOR_BIT);
            REQUIRE(g_observedHostImageViewCreateInfo.subresourceRange.levelCount == 1u);
            REQUIRE(g_observedHostImageViewCreateInfo.subresourceRange.layerCount == 1u);
            REQUIRE(g_observedSamplerCreateInfo.magFilter == VK_FILTER_NEAREST);
            REQUIRE(g_observedSamplerCreateInfo.minFilter == VK_FILTER_NEAREST);
            REQUIRE(g_observedSamplerCreateInfo.addressModeU
                    == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
            REQUIRE(g_observedSamplerCreateInfo.addressModeV
                    == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
            REQUIRE(g_observedSamplerCreateInfo.addressModeW
                    == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
            REQUIRE(g_observedDescriptorBinding.binding == 0u);
            REQUIRE(g_observedDescriptorBinding.descriptorType
                    == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            REQUIRE(g_observedDescriptorBinding.descriptorCount == 1u);
            REQUIRE(g_observedDescriptorBinding.stageFlags == VK_SHADER_STAGE_FRAGMENT_BIT);
            REQUIRE(g_observedDescriptorImageInfo.imageLayout == VK_IMAGE_LAYOUT_GENERAL);
            REQUIRE(g_observedPipelineColorFormat == VK_FORMAT_B8G8R8A8_UNORM);
            REQUIRE(g_observedVertexInputState.vertexBindingDescriptionCount == 0u);
            REQUIRE(g_observedVertexInputState.vertexAttributeDescriptionCount == 0u);
            REQUIRE(g_observedInputAssemblyState.topology
                    == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
            REQUIRE(g_observedViewport.width == 1280.0f);
            REQUIRE(g_observedViewport.height == 720.0f);
            REQUIRE(g_observedScissor.extent.width == 1280u);
            REQUIRE(g_observedScissor.extent.height == 720u);
            REQUIRE(g_observedRasterizationSamples == VK_SAMPLE_COUNT_1_BIT);
            REQUIRE(g_observedColorBlendAttachment.blendEnable == VK_FALSE);
            REQUIRE(g_destroyedShaderModuleCount == 2u);
            REQUIRE(g_observedCommandPoolQueueFamilyIndex == 7u);
            REQUIRE(g_hostCommandEvents.size() == 60u);

            const std::array expectedEventTypes{
                HostCommandEventType::BeginCommandBuffer,
                HostCommandEventType::HostBarrier,
                HostCommandEventType::SwapchainToColorBarrier,
                HostCommandEventType::BeginRendering,
                HostCommandEventType::BindPipeline,
                HostCommandEventType::BindDescriptors,
                HostCommandEventType::Draw,
                HostCommandEventType::EndRendering,
                HostCommandEventType::SwapchainToPresentBarrier,
                HostCommandEventType::EndCommandBuffer,
            };
            for (std::uint32_t frameSlotIndex = 0u; frameSlotIndex < 2u;
                 ++frameSlotIndex) {
                for (std::uint32_t imageIndex = 0u; imageIndex < 3u; ++imageIndex) {
                    const std::size_t matrixIndex = frameSlotIndex * 3u + imageIndex;
                    const VkCommandBuffer expectedCommandBuffer =
                        reinterpret_cast<VkCommandBuffer>(0xB000u + matrixIndex);
                    REQUIRE(resources.commandBuffer(frameSlotIndex, imageIndex)
                            == expectedCommandBuffer);
                    const std::size_t firstEventIndex =
                        matrixIndex * expectedEventTypes.size();
                    for (std::size_t eventOffset = 0u;
                         eventOffset < expectedEventTypes.size(); ++eventOffset) {
                        const auto& event =
                            g_hostCommandEvents[firstEventIndex + eventOffset];
                        REQUIRE(std::get<0>(event) == expectedCommandBuffer);
                        REQUIRE(std::get<1>(event) == expectedEventTypes[eventOffset]);
                    }

                    const auto& hostBarrierEvent =
                        g_hostCommandEvents[firstEventIndex + 1u];
                    const auto& hostBarrier = std::get<4>(hostBarrierEvent);
                    REQUIRE(std::get<2>(hostBarrierEvent)
                            == (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                                | VK_PIPELINE_STAGE_TRANSFER_BIT));
                    REQUIRE(std::get<3>(hostBarrierEvent)
                            == VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
                    REQUIRE(hostBarrier.oldLayout == VK_IMAGE_LAYOUT_GENERAL);
                    REQUIRE(hostBarrier.newLayout == VK_IMAGE_LAYOUT_GENERAL);
                    REQUIRE(hostBarrier.srcAccessMask == VK_ACCESS_MEMORY_WRITE_BIT);
                    REQUIRE(hostBarrier.dstAccessMask == VK_ACCESS_SHADER_READ_BIT);
                    REQUIRE(hostBarrier.subresourceRange.aspectMask
                            == VK_IMAGE_ASPECT_COLOR_BIT);
                    REQUIRE(hostBarrier.subresourceRange.levelCount == 1u);
                    REQUIRE(hostBarrier.subresourceRange.layerCount == 1u);

                    const auto& colorBarrierEvent =
                        g_hostCommandEvents[firstEventIndex + 2u];
                    const auto& colorBarrier = std::get<4>(colorBarrierEvent);
                    REQUIRE(std::get<2>(colorBarrierEvent)
                            == VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
                    REQUIRE(std::get<3>(colorBarrierEvent)
                            == VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
                    REQUIRE(colorBarrier.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED);
                    REQUIRE(colorBarrier.newLayout
                            == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                    REQUIRE(colorBarrier.srcAccessMask == 0u);
                    REQUIRE(colorBarrier.dstAccessMask
                            == VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

                    const auto& renderingEvent =
                        g_hostCommandEvents[firstEventIndex + 3u];
                    const auto& renderingInfo = std::get<5>(renderingEvent);
                    const auto& colorAttachment = std::get<6>(renderingEvent);
                    REQUIRE(renderingInfo.sType == VK_STRUCTURE_TYPE_RENDERING_INFO);
                    REQUIRE(renderingInfo.pNext == nullptr);
                    REQUIRE(renderingInfo.flags == 0u);
                    REQUIRE(renderingInfo.renderArea.offset.x == 0);
                    REQUIRE(renderingInfo.renderArea.offset.y == 0);
                    REQUIRE(renderingInfo.renderArea.extent.width == 1280u);
                    REQUIRE(renderingInfo.renderArea.extent.height == 720u);
                    REQUIRE(renderingInfo.layerCount == 1u);
                    REQUIRE(renderingInfo.viewMask == 0u);
                    REQUIRE(renderingInfo.colorAttachmentCount == 1u);
                    REQUIRE((std::get<7>(renderingEvent) & 0x1u) != 0u);
                    REQUIRE((std::get<7>(renderingEvent) & 0x2u) == 0u);
                    REQUIRE((std::get<7>(renderingEvent) & 0x4u) == 0u);
                    REQUIRE(renderingInfo.pColorAttachments == nullptr);
                    REQUIRE(renderingInfo.pDepthAttachment == nullptr);
                    REQUIRE(renderingInfo.pStencilAttachment == nullptr);
                    REQUIRE(colorAttachment.sType
                            == VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);
                    REQUIRE(colorAttachment.pNext == nullptr);
                    REQUIRE(colorAttachment.imageView
                            == g_hostResourceSwapchainImageViews[imageIndex]);
                    REQUIRE(colorAttachment.imageLayout
                            == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                    REQUIRE(colorAttachment.resolveMode == VK_RESOLVE_MODE_NONE);
                    REQUIRE(colorAttachment.resolveImageView == VK_NULL_HANDLE);
                    REQUIRE(colorAttachment.resolveImageLayout
                            == VK_IMAGE_LAYOUT_UNDEFINED);
                    REQUIRE(colorAttachment.loadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE);
                    REQUIRE(colorAttachment.storeOp == VK_ATTACHMENT_STORE_OP_STORE);
                    REQUIRE(std::get<7>(g_hostCommandEvents[firstEventIndex + 6u]) == 3u);

                    const auto& presentBarrierEvent =
                        g_hostCommandEvents[firstEventIndex + 8u];
                    const auto& presentBarrier = std::get<4>(presentBarrierEvent);
                    REQUIRE(std::get<2>(presentBarrierEvent)
                            == VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
                    REQUIRE(std::get<3>(presentBarrierEvent)
                            == VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
                    REQUIRE(presentBarrier.oldLayout
                            == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                    REQUIRE(presentBarrier.newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
                    REQUIRE(presentBarrier.srcAccessMask
                            == VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
                    REQUIRE(presentBarrier.dstAccessMask == 0u);
                }
            }
        }
        REQUIRE(g_destroyedCommandPoolCount == 1u);
        REQUIRE(g_destroyedPipelineCount == 1u);
        REQUIRE(g_destroyedPipelineLayoutCount == 1u);
        REQUIRE(g_destroyedDescriptorPoolCount == 1u);
        REQUIRE(g_destroyedDescriptorSetLayoutCount == 1u);
        REQUIRE(g_destroyedSamplerCount == 1u);
        REQUIRE(g_destroyedImageViewCount == 1u);
        REQUIRE(g_destroyedShaderModuleCount == 2u);
    }
    REQUIRE(g_destroyedCommandPoolCount == 1u);
    REQUIRE(g_destroyedPipelineCount == 1u);
    REQUIRE(g_destroyedPipelineLayoutCount == 1u);
    REQUIRE(g_destroyedDescriptorPoolCount == 1u);
    REQUIRE(g_destroyedDescriptorSetLayoutCount == 1u);
    REQUIRE(g_destroyedSamplerCount == 1u);
    REQUIRE(g_destroyedImageViewCount == 1u);
    REQUIRE(g_destroyedShaderModuleCount == 2u);
}

TEST_CASE("Host image resources reject unavailable dynamic rendering procedures",
          "[presentationRuntime][hostResources]") {
    resetHostResourceState();
    g_offerDynamicRenderingFunctions = false;

    const auto result = barrieww::VulkanHostImagePresentationResources::create(
        makeHostResourceCreateInfo());

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().vulkanResult == VK_ERROR_EXTENSION_NOT_PRESENT);
    REQUIRE(g_hostResourceCreationOrder.empty());
    REQUIRE(g_createdImageViewCount == 0u);
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

    const std::array lateRecordingFailures{
        HostResourceOperation::BeginCommandBuffer,
        HostResourceOperation::EndCommandBuffer,
    };
    for (const HostResourceOperation failedOperation : lateRecordingFailures) {
        DYNAMIC_SECTION("fourth invocation of operation "
                        << static_cast<int>(failedOperation)) {
            resetHostResourceState();
            g_failedHostResourceOperation = failedOperation;
            g_failedHostResourceInvocationOrdinal = 4u;
            g_injectedHostResourceResult = VK_ERROR_DEVICE_LOST;

            const auto result = barrieww::VulkanHostImagePresentationResources::create(
                makeHostResourceCreateInfo());

            REQUIRE_FALSE(result.has_value());
            REQUIRE(result.error().vulkanResult == VK_ERROR_DEVICE_LOST);
            REQUIRE(g_hostResourceOperationInvocationCounts[
                        static_cast<std::size_t>(failedOperation)] == 4u);
            REQUIRE(g_destroyedCommandPoolCount == 1u);
            REQUIRE(g_destroyedPipelineCount == 1u);
            REQUIRE(g_destroyedPipelineLayoutCount == 1u);
            REQUIRE(g_destroyedDescriptorPoolCount == 1u);
            REQUIRE(g_destroyedDescriptorSetLayoutCount == 1u);
            REQUIRE(g_destroyedSamplerCount == 1u);
            REQUIRE(g_destroyedImageViewCount == 1u);
            REQUIRE(g_destroyedShaderModuleCount == 2u);
            REQUIRE(std::get<0>(g_hostCommandEvents.back())
                    == reinterpret_cast<VkCommandBuffer>(0xB003u));
            REQUIRE(std::get<1>(g_hostCommandEvents.back())
                    == (failedOperation == HostResourceOperation::BeginCommandBuffer
                            ? HostCommandEventType::BeginCommandBuffer
                            : HostCommandEventType::EndCommandBuffer));
            REQUIRE(g_hostCommandEvents.size()
                    == (failedOperation == HostResourceOperation::BeginCommandBuffer
                            ? 31u
                            : 40u));
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

namespace {

/**
 * @note ThreadSafety: Test-thread confined through deterministic global Vulkan mock state.
 * @brief Creates a host-image presentation runtime from valid exact-format input.
 * @return std::uint64_t Nonzero Native-owned runtime address
 * @warning MemoryOwnership: Transfers the returned runtime ownership to the caller, which
 *          must invoke barriEwwDestroyPresentationRuntimeVersion1 exactly once.
 */
std::uint64_t openHostImageRuntime() {
    g_supportedUsageFlags =
        barrieww::VulkanHostImagePresentationResources::s_requiredSwapchainImageUsageFlags;
    const NativeHostImagePresentationRuntimeCreateInfoVersion1 createInfo =
        makeHostImageCreateInfo();
    NativePresentationRuntimeCreateResultVersion1 createResult{};
    REQUIRE(barriEwwCreateHostImagePresentationRuntimeVersion1(&createInfo, &createResult)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(createResult.runtimeAddress != 0u);
    return createResult.runtimeAddress;
}

} // namespace

TEST_CASE("Host image runtime boundary clears outputs and rejects null or invalid scalars",
          "[presentationRuntime][hostRuntime]") {
    resetPresentationState();
    resetHostResourceState();
    NativePresentationRuntimeCreateResultVersion1 createResult{
        UINT64_MAX, VK_ERROR_UNKNOWN, UINT32_MAX, UINT32_MAX,
        UINT32_MAX, UINT32_MAX, UINT32_MAX};

    REQUIRE(barriEwwCreateHostImagePresentationRuntimeVersion1(nullptr, &createResult)
            == NativePresentationRuntimeOperationResult::InvalidArgument);
    REQUIRE(createResult.runtimeAddress == 0u);
    REQUIRE(createResult.vulkanResult == VK_SUCCESS);
    const NativeHostImagePresentationRuntimeCreateInfoVersion1 validCreateInfo =
        makeHostImageCreateInfo();
    REQUIRE(barriEwwCreateHostImagePresentationRuntimeVersion1(&validCreateInfo, nullptr)
            == NativePresentationRuntimeOperationResult::InvalidArgument);

    const std::array<std::uint64_t NativeHostImagePresentationRuntimeCreateInfoVersion1::*,
                     7>
        handleMembers{
            &NativeHostImagePresentationRuntimeCreateInfoVersion1::instanceHandle,
            &NativeHostImagePresentationRuntimeCreateInfoVersion1::physicalDeviceHandle,
            &NativeHostImagePresentationRuntimeCreateInfoVersion1::logicalDeviceHandle,
            &NativeHostImagePresentationRuntimeCreateInfoVersion1::surfaceHandle,
            &NativeHostImagePresentationRuntimeCreateInfoVersion1::graphicsQueueHandle,
            &NativeHostImagePresentationRuntimeCreateInfoVersion1::presentQueueHandle,
            &NativeHostImagePresentationRuntimeCreateInfoVersion1::hostImageHandle,
        };
    for (const auto handleMember : handleMembers) {
        auto createInfo = makeHostImageCreateInfo();
        createInfo.*handleMember = 0u;
        createResult.runtimeAddress = UINT64_MAX;
        REQUIRE(barriEwwCreateHostImagePresentationRuntimeVersion1(&createInfo, &createResult)
                == NativePresentationRuntimeOperationResult::InvalidArgument);
        REQUIRE(createResult.runtimeAddress == 0u);
    }

    SECTION("reserved flags") {
        auto createInfo = makeHostImageCreateInfo();
        createInfo.reservedFlags = 1u;
        REQUIRE(barriEwwCreateHostImagePresentationRuntimeVersion1(&createInfo, &createResult)
                == NativePresentationRuntimeOperationResult::InvalidArgument);
    }
    SECTION("unknown host format") {
        auto createInfo = makeHostImageCreateInfo();
        createInfo.hostImageFormatValue = UINT32_MAX;
        REQUIRE(barriEwwCreateHostImagePresentationRuntimeVersion1(&createInfo, &createResult)
                == NativePresentationRuntimeOperationResult::InvalidArgument);
    }
    SECTION("unknown requested surface format") {
        auto createInfo = makeHostImageCreateInfo();
        createInfo.requestedSurfaceFormatValue = UINT32_MAX;
        REQUIRE(barriEwwCreateHostImagePresentationRuntimeVersion1(&createInfo, &createResult)
                == NativePresentationRuntimeOperationResult::InvalidArgument);
    }
    SECTION("zero framebuffer extent") {
        auto createInfo = makeHostImageCreateInfo();
        createInfo.framebufferWidth = 0u;
        REQUIRE(barriEwwCreateHostImagePresentationRuntimeVersion1(&createInfo, &createResult)
                == NativePresentationRuntimeOperationResult::InvalidArgument);
    }
    SECTION("zero host extent") {
        auto createInfo = makeHostImageCreateInfo();
        createInfo.hostImageHeight = 0u;
        REQUIRE(barriEwwCreateHostImagePresentationRuntimeVersion1(&createInfo, &createResult)
                == NativePresentationRuntimeOperationResult::InvalidArgument);
    }
    SECTION("zero frame slots") {
        auto createInfo = makeHostImageCreateInfo();
        createInfo.framesInFlightCount = 0u;
        REQUIRE(barriEwwCreateHostImagePresentationRuntimeVersion1(&createInfo, &createResult)
                == NativePresentationRuntimeOperationResult::InvalidArgument);
    }
    REQUIRE(g_destroyedSwapchainCount == 0u);
}

TEST_CASE("Host image runtime requires exact extents format color space and combined usage",
          "[presentationRuntime][hostRuntime]") {
    resetPresentationState();
    resetHostResourceState();
    g_supportedUsageFlags =
        barrieww::VulkanHostImagePresentationResources::s_requiredSwapchainImageUsageFlags;
    NativePresentationRuntimeCreateResultVersion1 createResult{};

    SECTION("host and framebuffer mismatch") {
        auto createInfo = makeHostImageCreateInfo();
        createInfo.hostImageWidth = 1279u;
        REQUIRE(barriEwwCreateHostImagePresentationRuntimeVersion1(&createInfo, &createResult)
                == NativePresentationRuntimeOperationResult::UnsupportedSurface);
    }
    SECTION("selected swapchain extent mismatch") {
        g_surfaceCurrentExtent = VkExtent2D{1024u, 720u};
        const auto createInfo = makeHostImageCreateInfo();
        REQUIRE(barriEwwCreateHostImagePresentationRuntimeVersion1(&createInfo, &createResult)
                == NativePresentationRuntimeOperationResult::UnsupportedSurface);
    }
    SECTION("requested format absent") {
        g_offerRequestedHostFormat = false;
        const auto createInfo = makeHostImageCreateInfo();
        REQUIRE(barriEwwCreateHostImagePresentationRuntimeVersion1(&createInfo, &createResult)
                == NativePresentationRuntimeOperationResult::UnsupportedSurface);
    }
    SECTION("requested format has a different color space") {
        g_requestedHostColorSpace = VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT;
        const auto createInfo = makeHostImageCreateInfo();
        REQUIRE(barriEwwCreateHostImagePresentationRuntimeVersion1(&createInfo, &createResult)
                == NativePresentationRuntimeOperationResult::UnsupportedSurface);
    }
    SECTION("surface lacks color attachment usage") {
        g_supportedUsageFlags = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        const auto createInfo = makeHostImageCreateInfo();
        REQUIRE(barriEwwCreateHostImagePresentationRuntimeVersion1(&createInfo, &createResult)
                == NativePresentationRuntimeOperationResult::UnsupportedSurface);
    }
    REQUIRE(createResult.runtimeAddress == 0u);
    REQUIRE(createResult.vulkanResult == VK_SUCCESS);
    REQUIRE(g_destroyedSwapchainCount == 0u);
}

TEST_CASE("Host image runtime creates exact swapchain and owns attached resources",
          "[presentationRuntime][hostRuntime]") {
    resetPresentationState();
    resetHostResourceState();
    g_supportedUsageFlags =
        barrieww::VulkanHostImagePresentationResources::s_requiredSwapchainImageUsageFlags;
    const auto createInfo = makeHostImageCreateInfo();
    NativePresentationRuntimeCreateResultVersion1 createResult{};
    REQUIRE(barriEwwCreateHostImagePresentationRuntimeVersion1(&createInfo, &createResult)
            == NativePresentationRuntimeOperationResult::Success);
    const std::uint64_t runtimeAddress = createResult.runtimeAddress;

    REQUIRE(runtimeAddress != 0u);
    REQUIRE(createResult.vulkanResult == VK_SUCCESS);
    REQUIRE(createResult.selectedFormatValue == 2u);
    REQUIRE(createResult.selectedPresentModeValue
            == static_cast<std::uint32_t>(VK_PRESENT_MODE_MAILBOX_KHR));
    REQUIRE(createResult.selectedSharingModeValue
            == static_cast<std::uint32_t>(VK_SHARING_MODE_EXCLUSIVE));
    REQUIRE(createResult.swapchainImageCount == 3u);
    REQUIRE(g_observedSwapchainImageUsage
            == (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT));
    REQUIRE(g_observedSwapchainImageFormat == VK_FORMAT_B8G8R8A8_UNORM);
    REQUIRE(g_observedSwapchainImageColorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
    REQUIRE(g_observedSwapchainExtent.width == 1280u);
    REQUIRE(g_observedSwapchainExtent.height == 720u);
    REQUIRE(g_observedPipelineColorFormat == VK_FORMAT_B8G8R8A8_UNORM);
    REQUIRE(g_observedHostImageViewCreateInfo.image == reinterpret_cast<VkImage>(0x4000u));
    REQUIRE(g_observedHostImageViewCreateInfo.format == VK_FORMAT_R8G8B8A8_UNORM);
    REQUIRE(g_recordedCommandBufferCount == 6u);

    REQUIRE(barriEwwDestroyPresentationRuntimeVersion1(runtimeAddress)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(g_destroyedSwapchainCount == 1u);
    REQUIRE(g_destroyedImageViewCount == 4u);
    REQUIRE(g_destroyedImageViews
            == std::vector<VkImageView>{
                reinterpret_cast<VkImageView>(0x7003u),
                reinterpret_cast<VkImageView>(0x7000u),
                reinterpret_cast<VkImageView>(0x7001u),
                reinterpret_cast<VkImageView>(0x7002u)});
}

TEST_CASE("Host image runtime creation preserves resource failure and cleans swapchain",
          "[presentationRuntime][hostRuntime]") {
    resetPresentationState();
    resetHostResourceState();
    g_supportedUsageFlags =
        barrieww::VulkanHostImagePresentationResources::s_requiredSwapchainImageUsageFlags;
    g_failedHostResourceOperation = HostResourceOperation::ImageView;
    g_failedHostResourceInvocationOrdinal = g_swapchainImageCount + 1u;
    g_injectedHostResourceResult = VK_ERROR_DEVICE_LOST;
    const auto createInfo = makeHostImageCreateInfo();
    NativePresentationRuntimeCreateResultVersion1 createResult{
        UINT64_MAX, VK_ERROR_UNKNOWN, UINT32_MAX, UINT32_MAX,
        UINT32_MAX, UINT32_MAX, UINT32_MAX};

    REQUIRE(barriEwwCreateHostImagePresentationRuntimeVersion1(&createInfo, &createResult)
            == NativePresentationRuntimeOperationResult::VulkanFailure);
    REQUIRE(createResult.runtimeAddress == 0u);
    REQUIRE(createResult.vulkanResult == VK_ERROR_DEVICE_LOST);
    REQUIRE(g_destroyedImageViewCount == 3u);
    REQUIRE(g_destroyedSwapchainCount == 1u);
}

TEST_CASE("Host image runtime preserves WSI query failures as Vulkan failures",
          "[presentationRuntime][hostRuntime]") {
    resetPresentationState();
    resetHostResourceState();
    g_supportedUsageFlags =
        barrieww::VulkanHostImagePresentationResources::s_requiredSwapchainImageUsageFlags;
    g_surfaceFormatCountResult = VK_ERROR_SURFACE_LOST_KHR;
    const auto createInfo = makeHostImageCreateInfo();
    NativePresentationRuntimeCreateResultVersion1 createResult{};

    REQUIRE(barriEwwCreateHostImagePresentationRuntimeVersion1(&createInfo, &createResult)
            == NativePresentationRuntimeOperationResult::VulkanFailure);
    REQUIRE(createResult.vulkanResult == VK_ERROR_SURFACE_LOST_KHR);
    REQUIRE(createResult.runtimeAddress == 0u);
}

TEST_CASE("Host image submit selects the fixed frame slot and image matrix entry",
          "[presentationRuntime][hostRuntime]") {
    resetPresentationState();
    resetHostResourceState();
    const std::uint64_t runtimeAddress = openHostImageRuntime();
    barrieww::NativePresentationBeginFrameResultVersion1 beginResult{};
    barrieww::NativePresentationFrameMetricsVersion1 priorMetrics{};
    barrieww::NativePresentationSubmitFrameResultVersion1 submitResult =
        makeSentinelSubmitResult();
    const std::uint32_t creationRecordingCount = g_recordedCommandBufferCount;

    REQUIRE(barriEwwSubmitAndPresentHostImageFrameVersion1(runtimeAddress, &submitResult)
            == NativePresentationRuntimeOperationResult::InvalidArgument);
    REQUIRE(submitResult.frameStatusValue == 0u);
    REQUIRE(submitResult.vulkanResult == VK_SUCCESS);

    g_acquiredImageIndex = 2u;
    REQUIRE(barriEwwBeginPresentationFrameVersion1(runtimeAddress, 1280u, 720u,
                                                   &beginResult, &priorMetrics)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(barriEwwSubmitAndPresentHostImageFrameVersion1(runtimeAddress, &submitResult)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(g_observedSubmittedCommandBuffer == reinterpret_cast<VkCommandBuffer>(0xB002u));

    g_acquiredImageIndex = 1u;
    REQUIRE(barriEwwBeginPresentationFrameVersion1(runtimeAddress, 1280u, 720u,
                                                   &beginResult, &priorMetrics)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(barriEwwSubmitAndPresentHostImageFrameVersion1(runtimeAddress, &submitResult)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(g_observedSubmittedCommandBuffer == reinterpret_cast<VkCommandBuffer>(0xB004u));
    REQUIRE(g_recordedCommandBufferCount == creationRecordingCount);
    REQUIRE(g_submitCallCount == 2u);
    REQUIRE(g_presentCallCount == 2u);
    REQUIRE(barriEwwDestroyPresentationRuntimeVersion1(runtimeAddress)
            == NativePresentationRuntimeOperationResult::Success);
}

TEST_CASE("Host image detach skips an open unsubmitted slot and preserves clear presentation",
          "[presentationRuntime][hostRuntime]") {
    resetPresentationState();
    resetHostResourceState();
    const std::uint64_t runtimeAddress = openHostImageRuntime();
    barrieww::NativePresentationBeginFrameResultVersion1 beginResult{};
    barrieww::NativePresentationFrameMetricsVersion1 priorMetrics{};
    barrieww::NativePresentationSubmitFrameResultVersion1 submitResult{};
    barrieww::NativePresentationDetachHostImageResourcesResultVersion1 detachResult{
        VK_ERROR_UNKNOWN, UINT32_MAX};

    REQUIRE(barriEwwBeginPresentationFrameVersion1(runtimeAddress, 1280u, 720u,
                                                   &beginResult, &priorMetrics)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(barriEwwSubmitAndPresentHostImageFrameVersion1(runtimeAddress, &submitResult)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(barriEwwBeginPresentationFrameVersion1(runtimeAddress, 1280u, 720u,
                                                   &beginResult, &priorMetrics)
            == NativePresentationRuntimeOperationResult::Success);

    REQUIRE(barriEwwDetachHostImagePresentationResourcesVersion1(runtimeAddress,
                                                                 &detachResult)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(detachResult.vulkanResult == VK_SUCCESS);
    REQUIRE(detachResult.reserved == 0u);
    REQUIRE(g_waitedHostResourceFences
            == std::vector<VkFence>{reinterpret_cast<VkFence>(0x9000u)});
    const std::uint32_t waitCallCount = g_hostResourceWaitCallCount;
    REQUIRE(barriEwwDetachHostImagePresentationResourcesVersion1(runtimeAddress,
                                                                 &detachResult)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(g_hostResourceWaitCallCount == waitCallCount);

    submitResult = makeSentinelSubmitResult();
    REQUIRE(barriEwwSubmitAndPresentHostImageFrameVersion1(runtimeAddress, &submitResult)
            == NativePresentationRuntimeOperationResult::InvalidArgument);
    REQUIRE(submitResult.frameStatusValue == 0u);
    REQUIRE(submitResult.vulkanResult == VK_SUCCESS);
    REQUIRE(barriEwwSubmitAndPresentClearFrameVersion1(runtimeAddress, 0.2f, 0.4f, 0.6f,
                                                       &submitResult)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(g_clearImageCallCount == 1u);
    REQUIRE(barriEwwDestroyPresentationRuntimeVersion1(runtimeAddress)
            == NativePresentationRuntimeOperationResult::Success);
}

TEST_CASE("Host image detach retains resources when a submitted fence wait fails",
          "[presentationRuntime][hostRuntime]") {
    resetPresentationState();
    resetHostResourceState();
    const std::uint64_t runtimeAddress = openHostImageRuntime();
    barrieww::NativePresentationBeginFrameResultVersion1 beginResult{};
    barrieww::NativePresentationFrameMetricsVersion1 priorMetrics{};
    barrieww::NativePresentationSubmitFrameResultVersion1 submitResult{};
    barrieww::NativePresentationDetachHostImageResourcesResultVersion1 detachResult{};

    REQUIRE(barriEwwBeginPresentationFrameVersion1(runtimeAddress, 1280u, 720u,
                                                   &beginResult, &priorMetrics)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(barriEwwSubmitAndPresentHostImageFrameVersion1(runtimeAddress, &submitResult)
            == NativePresentationRuntimeOperationResult::Success);
    g_acquiredImageIndex = 1u;
    REQUIRE(barriEwwBeginPresentationFrameVersion1(runtimeAddress, 1280u, 720u,
                                                   &beginResult, &priorMetrics)
            == NativePresentationRuntimeOperationResult::Success);
    g_hostResourceWaitResult = VK_ERROR_DEVICE_LOST;

    REQUIRE(barriEwwDetachHostImagePresentationResourcesVersion1(runtimeAddress,
                                                                 &detachResult)
            == NativePresentationRuntimeOperationResult::VulkanFailure);
    REQUIRE(detachResult.vulkanResult == VK_ERROR_DEVICE_LOST);
    REQUIRE(g_destroyedCommandPoolCount == 0u);
    REQUIRE(g_destroyedSamplerCount == 0u);

    g_hostResourceWaitResult = VK_SUCCESS;
    REQUIRE(barriEwwSubmitAndPresentHostImageFrameVersion1(runtimeAddress, &submitResult)
            == NativePresentationRuntimeOperationResult::Success);
    REQUIRE(g_observedSubmittedCommandBuffer == reinterpret_cast<VkCommandBuffer>(0xB004u));
    REQUIRE(barriEwwDestroyPresentationRuntimeVersion1(runtimeAddress)
            == NativePresentationRuntimeOperationResult::Success);
}

TEST_CASE("Host image operations reject null addresses and clear writable results",
          "[presentationRuntime][hostRuntime]") {
    barrieww::NativePresentationDetachHostImageResourcesResultVersion1 detachResult{
        VK_ERROR_UNKNOWN, UINT32_MAX};
    auto submitResult = makeSentinelSubmitResult();

    REQUIRE(barriEwwDetachHostImagePresentationResourcesVersion1(0u, &detachResult)
            == NativePresentationRuntimeOperationResult::InvalidArgument);
    REQUIRE(detachResult.vulkanResult == VK_SUCCESS);
    REQUIRE(detachResult.reserved == 0u);
    REQUIRE(barriEwwDetachHostImagePresentationResourcesVersion1(1u, nullptr)
            == NativePresentationRuntimeOperationResult::InvalidArgument);
    REQUIRE(barriEwwSubmitAndPresentHostImageFrameVersion1(0u, &submitResult)
            == NativePresentationRuntimeOperationResult::InvalidArgument);
    REQUIRE(submitResult.frameStatusValue == 0u);
    REQUIRE(submitResult.vulkanResult == VK_SUCCESS);
    REQUIRE(barriEwwSubmitAndPresentHostImageFrameVersion1(1u, nullptr)
            == NativePresentationRuntimeOperationResult::InvalidArgument);
}

TEST_CASE("Host image boundary operations contain detach and submit exceptions",
          "[presentationRuntime][hostRuntime]") {
    SECTION("creation") {
        NativePresentationRuntimeCreateResultVersion1 createResult{
            UINT64_MAX, VK_ERROR_UNKNOWN, UINT32_MAX, UINT32_MAX,
            UINT32_MAX, UINT32_MAX, UINT32_MAX};
        const auto throwingCreation = []() -> NativePresentationRuntimeOperationResult {
            throw std::runtime_error{"Injected host creation failure"};
        };
        REQUIRE(barrieww::interoperability::executeNativePresentationCreateOperation(
                    &createResult, throwingCreation)
                == NativePresentationRuntimeOperationResult::InternalFailure);
        REQUIRE(createResult.runtimeAddress == 0u);
        REQUIRE(createResult.vulkanResult == VK_SUCCESS);
    }
    SECTION("detach") {
        barrieww::NativePresentationDetachHostImageResourcesResultVersion1 detachResult{
            VK_ERROR_UNKNOWN, UINT32_MAX};
        const auto throwingDetach = []() -> std::expected<void, VkResult> {
            throw std::runtime_error{"Injected detach failure"};
        };
        REQUIRE(barrieww::interoperability::executeNativePresentationDetachOperation(
                    &detachResult, throwingDetach)
                == NativePresentationRuntimeOperationResult::InternalFailure);
        REQUIRE(detachResult.vulkanResult == VK_SUCCESS);
        REQUIRE(detachResult.reserved == 0u);
    }
    SECTION("submit") {
        auto submitResult = makeSentinelSubmitResult();
        const auto throwingHostSubmission =
            []() -> barrieww::VulkanPresentationRuntime::SubmitFrameResult {
            throw std::runtime_error{"Injected host submit failure"};
        };
        REQUIRE(barrieww::interoperability::executeNativePresentationSubmitFrameOperation(
                    &submitResult, throwingHostSubmission)
                == NativePresentationRuntimeOperationResult::InternalFailure);
        REQUIRE(submitResult.frameStatusValue == 0u);
        REQUIRE(submitResult.vulkanResult == VK_SUCCESS);
    }
}

TEST_CASE("Standalone clear creation retains transfer-only SRGB behavior",
          "[presentationRuntime][hostRuntime][regression]") {
    resetPresentationState();
    resetHostResourceState();
    const std::uint64_t runtimeAddress = openRuntime();
    REQUIRE(g_observedSwapchainImageUsage == VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    REQUIRE(g_observedSwapchainImageFormat == VK_FORMAT_B8G8R8A8_SRGB);
    REQUIRE(g_observedSwapchainImageColorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
    REQUIRE(g_recordedCommandBufferCount == 0u);
    REQUIRE(barriEwwDestroyPresentationRuntimeVersion1(runtimeAddress)
            == NativePresentationRuntimeOperationResult::Success);
}
