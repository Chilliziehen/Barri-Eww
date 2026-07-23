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
