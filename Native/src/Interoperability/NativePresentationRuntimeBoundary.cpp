#include "BarriEww/Interoperability/NativePresentationRuntimeBoundary.hpp"

#include <cstdint>
#include <memory>

#include <vulkan/vulkan.h>

#include "BarriEww/Vulkan/VulkanPresentationRuntime.hpp"

namespace {

/** Converts the concrete Vulkan format into the stable neutral v0.1 format value. */
std::uint32_t neutralFormatValue(VkFormat format) {
    switch (format) {
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_R8G8B8A8_UNORM: return 1u;
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_UNORM: return 2u;
        default: return 0u;
    }
}

/** Fills the fixed metrics record from a runtime frame-metrics value. */
void writeFrameMetrics(const barrieww::VulkanPresentationRuntime& runtime,
                       const barrieww::VulkanPresentationRuntime::FrameMetrics& source,
                       barrieww::NativePresentationFrameMetricsVersion1* target) {
    *target = {};
    target->frameSequence = source.frameSequence;
    target->swapchainGeneration = source.swapchainGeneration;
    target->frameSlotIndex = source.frameSlotIndex;
    target->imageIndex = source.imageIndex;
    target->validFlags = source.validFlags;
    target->presentResultValue = source.presentResultValue;
    target->fenceWaitNanoseconds = source.fenceWaitNanoseconds;
    target->acquireNanoseconds = source.acquireNanoseconds;
    target->nativeSubmitCallNanoseconds = source.nativeSubmitCallNanoseconds;
    target->presentCallNanoseconds = source.presentCallNanoseconds;
    target->totalCpuFrameNanoseconds = source.totalCpuFrameNanoseconds;
    target->computeGpuNanoseconds = source.computeGpuNanoseconds;
    target->graphicsGpuNanoseconds = source.graphicsGpuNanoseconds;
    target->finalTransferGpuNanoseconds = source.finalTransferGpuNanoseconds;
    target->totalSubmittedGpuNanoseconds = source.totalSubmittedGpuNanoseconds;
    target->presentModeValue = static_cast<std::uint32_t>(runtime.presentMode());
    target->sharingModeValue = static_cast<std::uint32_t>(runtime.sharingMode());
}

} // namespace

extern "C" barrieww::NativePresentationRuntimeOperationResult
barriEwwCreatePresentationRuntimeVersion1(
    const barrieww::NativePresentationRuntimeCreateInfoVersion1* createInfo,
    barrieww::NativePresentationRuntimeCreateResultVersion1* createResult) noexcept {
    using barrieww::NativePresentationRuntimeOperationResult;

    if (createInfo == nullptr || createResult == nullptr) {
        return NativePresentationRuntimeOperationResult::InvalidArgument;
    }
    *createResult = {};
    if (createInfo->instanceHandle == 0u || createInfo->physicalDeviceHandle == 0u
        || createInfo->logicalDeviceHandle == 0u || createInfo->surfaceHandle == 0u
        || createInfo->graphicsQueueHandle == 0u || createInfo->presentQueueHandle == 0u
        || createInfo->reservedFlags != 0u) {
        return NativePresentationRuntimeOperationResult::InvalidArgument;
    }

    try {
        barrieww::VulkanPresentationRuntime::CreateInfo runtimeCreateInfo{
            reinterpret_cast<VkPhysicalDevice>(createInfo->physicalDeviceHandle),
            reinterpret_cast<VkDevice>(createInfo->logicalDeviceHandle),
            reinterpret_cast<VkSurfaceKHR>(createInfo->surfaceHandle),
            reinterpret_cast<VkQueue>(createInfo->graphicsQueueHandle),
            reinterpret_cast<VkQueue>(createInfo->presentQueueHandle),
            createInfo->graphicsQueueFamilyIndex,
            createInfo->presentQueueFamilyIndex,
            createInfo->framebufferWidth,
            createInfo->framebufferHeight,
            createInfo->framesInFlightCount};
        auto runtimeResult = barrieww::VulkanPresentationRuntime::create(runtimeCreateInfo);
        if (!runtimeResult.has_value()) {
            createResult->vulkanResult = runtimeResult.error().vulkanResult;
            return runtimeResult.error().unsupportedSurface
                ? NativePresentationRuntimeOperationResult::UnsupportedSurface
                : NativePresentationRuntimeOperationResult::VulkanFailure;
        }

        auto runtime = std::make_unique<barrieww::VulkanPresentationRuntime>(
            std::move(*runtimeResult));
        createResult->selectedFormatValue = neutralFormatValue(runtime->imageFormat());
        createResult->selectedPresentModeValue =
            static_cast<std::uint32_t>(runtime->presentMode());
        createResult->selectedSharingModeValue =
            static_cast<std::uint32_t>(runtime->sharingMode());
        createResult->swapchainImageCount = runtime->imageCount();
        createResult->runtimeAddress = reinterpret_cast<std::uint64_t>(runtime.release());
        return NativePresentationRuntimeOperationResult::Success;
    } catch (...) {
        *createResult = {};
        return NativePresentationRuntimeOperationResult::InternalFailure;
    }
}

extern "C" barrieww::NativePresentationRuntimeOperationResult
barriEwwDestroyPresentationRuntimeVersion1(std::uint64_t runtimeAddress) noexcept {
    using barrieww::NativePresentationRuntimeOperationResult;
    if (runtimeAddress == 0u) {
        return NativePresentationRuntimeOperationResult::InvalidArgument;
    }
    try {
        delete reinterpret_cast<barrieww::VulkanPresentationRuntime*>(runtimeAddress);
        return NativePresentationRuntimeOperationResult::Success;
    } catch (...) {
        return NativePresentationRuntimeOperationResult::InternalFailure;
    }
}

extern "C" barrieww::NativePresentationRuntimeOperationResult
barriEwwBeginPresentationFrameVersion1(
    std::uint64_t runtimeAddress, std::uint32_t framebufferWidth,
    std::uint32_t framebufferHeight,
    barrieww::NativePresentationBeginFrameResultVersion1* beginResult,
    barrieww::NativePresentationFrameMetricsVersion1* priorMetrics) noexcept {
    using barrieww::NativePresentationRuntimeOperationResult;
    if (beginResult == nullptr || priorMetrics == nullptr) {
        return NativePresentationRuntimeOperationResult::InvalidArgument;
    }
    *beginResult = {};
    *priorMetrics = {};
    if (runtimeAddress == 0u) {
        return NativePresentationRuntimeOperationResult::InvalidArgument;
    }

    try {
        auto* runtime =
            reinterpret_cast<barrieww::VulkanPresentationRuntime*>(runtimeAddress);
        const auto frameResult = runtime->beginFrame(framebufferWidth, framebufferHeight);
        beginResult->frameStatusValue = static_cast<std::uint32_t>(frameResult.status);
        beginResult->frameSlotIndex = frameResult.frameSlotIndex;
        beginResult->imageIndex = frameResult.imageIndex;
        beginResult->frameSequence = frameResult.frameSequence;
        beginResult->swapchainGeneration = frameResult.swapchainGeneration;
        beginResult->vulkanResult = frameResult.vulkanResult;
        if (frameResult.priorMetricsValid) {
            beginResult->priorMetricsValid = 1u;
            writeFrameMetrics(*runtime, frameResult.priorMetrics, priorMetrics);
        }
        return NativePresentationRuntimeOperationResult::Success;
    } catch (...) {
        *beginResult = {};
        *priorMetrics = {};
        return NativePresentationRuntimeOperationResult::InternalFailure;
    }
}

extern "C" barrieww::NativePresentationRuntimeOperationResult
barriEwwSubmitPresentationFrameVersion1(
    std::uint64_t runtimeAddress, std::uint64_t commandBufferHandle,
    barrieww::NativePresentationSubmitFrameResultVersion1* submitResult) noexcept {
    using barrieww::NativePresentationRuntimeOperationResult;
    if (submitResult == nullptr) {
        return NativePresentationRuntimeOperationResult::InvalidArgument;
    }
    *submitResult = {};
    if (runtimeAddress == 0u) {
        return NativePresentationRuntimeOperationResult::InvalidArgument;
    }

    try {
        auto* runtime =
            reinterpret_cast<barrieww::VulkanPresentationRuntime*>(runtimeAddress);
        if (!runtime->isFrameOpen()) {
            return NativePresentationRuntimeOperationResult::InvalidArgument;
        }
        const auto frameResult = runtime->submitAndPresentFrame(
            reinterpret_cast<VkCommandBuffer>(commandBufferHandle));
        submitResult->frameStatusValue = static_cast<std::uint32_t>(frameResult.status);
        submitResult->vulkanResult = frameResult.vulkanResult;
        return NativePresentationRuntimeOperationResult::Success;
    } catch (...) {
        *submitResult = {};
        return NativePresentationRuntimeOperationResult::InternalFailure;
    }
}

extern "C" barrieww::NativePresentationRuntimeOperationResult
barriEwwSubmitAndPresentClearFrameVersion1(
    std::uint64_t runtimeAddress, float clearRed, float clearGreen, float clearBlue,
    barrieww::NativePresentationSubmitFrameResultVersion1* submitResult) noexcept {
    using barrieww::NativePresentationRuntimeOperationResult;
    if (submitResult == nullptr) {
        return NativePresentationRuntimeOperationResult::InvalidArgument;
    }
    *submitResult = {};
    if (runtimeAddress == 0u) {
        return NativePresentationRuntimeOperationResult::InvalidArgument;
    }

    try {
        auto* runtime =
            reinterpret_cast<barrieww::VulkanPresentationRuntime*>(runtimeAddress);
        if (!runtime->isFrameOpen()) {
            return NativePresentationRuntimeOperationResult::InvalidArgument;
        }
        const float clearColor[3] = {clearRed, clearGreen, clearBlue};
        const auto frameResult = runtime->submitAndPresentClearFrame(clearColor);
        submitResult->frameStatusValue = static_cast<std::uint32_t>(frameResult.status);
        submitResult->vulkanResult = frameResult.vulkanResult;
        return NativePresentationRuntimeOperationResult::Success;
    } catch (...) {
        *submitResult = {};
        return NativePresentationRuntimeOperationResult::InternalFailure;
    }
}

extern "C" barrieww::NativePresentationRuntimeOperationResult
barriEwwPresentClearFrameVersion1(
    std::uint64_t runtimeAddress, std::uint32_t framebufferWidth,
    std::uint32_t framebufferHeight, float clearRed, float clearGreen, float clearBlue,
    barrieww::NativePresentationBeginFrameResultVersion1* beginResult,
    barrieww::NativePresentationFrameMetricsVersion1* priorMetrics,
    barrieww::NativePresentationSubmitFrameResultVersion1* submitResult) noexcept {
    using barrieww::NativePresentationRuntimeOperationResult;
    if (beginResult == nullptr || priorMetrics == nullptr || submitResult == nullptr) {
        return NativePresentationRuntimeOperationResult::InvalidArgument;
    }
    *beginResult = {};
    *priorMetrics = {};
    *submitResult = {};
    if (runtimeAddress == 0u) {
        return NativePresentationRuntimeOperationResult::InvalidArgument;
    }

    try {
        auto* runtime =
            reinterpret_cast<barrieww::VulkanPresentationRuntime*>(runtimeAddress);
        const float clearColor[3] = {clearRed, clearGreen, clearBlue};
        barrieww::VulkanPresentationRuntime::BeginFrameResult frameBeginResult{};
        const auto frameSubmitResult = runtime->presentClearFrame(
            framebufferWidth, framebufferHeight, clearColor, frameBeginResult);
        beginResult->frameStatusValue =
            static_cast<std::uint32_t>(frameBeginResult.status);
        beginResult->frameSlotIndex = frameBeginResult.frameSlotIndex;
        beginResult->imageIndex = frameBeginResult.imageIndex;
        beginResult->frameSequence = frameBeginResult.frameSequence;
        beginResult->swapchainGeneration = frameBeginResult.swapchainGeneration;
        beginResult->vulkanResult = frameBeginResult.vulkanResult;
        if (frameBeginResult.priorMetricsValid) {
            beginResult->priorMetricsValid = 1u;
            writeFrameMetrics(*runtime, frameBeginResult.priorMetrics, priorMetrics);
        }
        submitResult->frameStatusValue =
            static_cast<std::uint32_t>(frameSubmitResult.status);
        submitResult->vulkanResult = frameSubmitResult.vulkanResult;
        return NativePresentationRuntimeOperationResult::Success;
    } catch (...) {
        *beginResult = {};
        *priorMetrics = {};
        *submitResult = {};
        return NativePresentationRuntimeOperationResult::InternalFailure;
    }
}
