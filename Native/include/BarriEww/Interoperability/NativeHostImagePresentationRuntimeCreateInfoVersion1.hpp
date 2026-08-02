#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace barrieww {

/**
 * @note ThreadSafety: Plain input record; separate instances are concurrency-safe.
 * @brief Java-owned Vulkan bootstrap and host-image handles borrowed by one Native
 *        host-image presentation runtime. reservedFlags must be zero.
 * @warning MemoryOwnership: Java owns every encoded Vulkan handle and keeps it valid until
 *          host-image resources are detached or Native runtime destruction returns. Native
 *          copies scalar values and destroys none of the borrowed objects.
 */
struct NativeHostImagePresentationRuntimeCreateInfoVersion1 {
    std::uint64_t instanceHandle;
    std::uint64_t physicalDeviceHandle;
    std::uint64_t logicalDeviceHandle;
    std::uint64_t surfaceHandle;
    std::uint64_t graphicsQueueHandle;
    std::uint64_t presentQueueHandle;
    std::uint32_t graphicsQueueFamilyIndex;
    std::uint32_t presentQueueFamilyIndex;
    std::uint32_t framebufferWidth;
    std::uint32_t framebufferHeight;
    std::uint32_t framesInFlightCount;
    std::uint32_t reservedFlags;
    std::uint64_t hostImageHandle;
    std::uint32_t hostImageFormatValue;
    std::uint32_t requestedSurfaceFormatValue;
    std::uint32_t hostImageWidth;
    std::uint32_t hostImageHeight;
};

static_assert(sizeof(NativeHostImagePresentationRuntimeCreateInfoVersion1) == 96u);
static_assert(alignof(NativeHostImagePresentationRuntimeCreateInfoVersion1) == 8u);
static_assert(offsetof(NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       instanceHandle) == 0u);
static_assert(offsetof(NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       physicalDeviceHandle) == 8u);
static_assert(offsetof(NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       logicalDeviceHandle) == 16u);
static_assert(offsetof(NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       surfaceHandle) == 24u);
static_assert(offsetof(NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       graphicsQueueHandle) == 32u);
static_assert(offsetof(NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       presentQueueHandle) == 40u);
static_assert(offsetof(NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       graphicsQueueFamilyIndex) == 48u);
static_assert(offsetof(NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       presentQueueFamilyIndex) == 52u);
static_assert(offsetof(NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       framebufferWidth) == 56u);
static_assert(offsetof(NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       framebufferHeight) == 60u);
static_assert(offsetof(NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       framesInFlightCount) == 64u);
static_assert(offsetof(NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       reservedFlags) == 68u);
static_assert(offsetof(NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       hostImageHandle) == 72u);
static_assert(offsetof(NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       hostImageFormatValue) == 80u);
static_assert(offsetof(NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       requestedSurfaceFormatValue) == 84u);
static_assert(offsetof(NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       hostImageWidth) == 88u);
static_assert(offsetof(NativeHostImagePresentationRuntimeCreateInfoVersion1,
                       hostImageHeight) == 92u);
static_assert(std::is_standard_layout_v<
              NativeHostImagePresentationRuntimeCreateInfoVersion1>);
static_assert(std::is_trivially_copyable_v<
              NativeHostImagePresentationRuntimeCreateInfoVersion1>);

} // namespace barrieww
