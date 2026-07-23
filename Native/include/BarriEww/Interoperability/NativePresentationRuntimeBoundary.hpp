#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#if defined(_WIN32)
    #if defined(BARRIEWW_NATIVE_FFM_EXPORTS)
        #define BARRIEWW_PRESENTATION_FFM_EXPORT __declspec(dllexport)
    #else
        #define BARRIEWW_PRESENTATION_FFM_EXPORT __declspec(dllimport)
    #endif
#else
    #define BARRIEWW_PRESENTATION_FFM_EXPORT __attribute__((visibility("default")))
#endif

namespace barrieww {

/**
 * @note ThreadSafety: Enumeration; trivially thread-safe.
 * @brief Stable operation-level results of presentation runtime creation/destruction.
 */
enum class NativePresentationRuntimeOperationResult : std::uint32_t {
    Success = 0,
    InvalidArgument = 1,
    UnsupportedSurface = 2,
    VulkanFailure = 3,
    InternalFailure = 4,
};

/**
 * @note ThreadSafety: Enumeration; trivially thread-safe.
 * @brief Normal Version 1 frame outcomes; ordinary recreation is not an exception.
 */
enum class NativePresentationFrameStatus : std::uint32_t {
    Success = 0,
    SurfaceUnavailable = 1,
    RecreateRequired = 2,
    Suboptimal = 3,
};

/**
 * @note ThreadSafety: Plain input record; separate instances are concurrency-safe.
 * @brief Java-owned Vulkan bootstrap handles borrowed by one Native presentation runtime.
 * @warning MemoryOwnership: Java owns every encoded Vulkan handle and keeps it valid until
 *          Native runtime destruction returns. Native copies scalar values and destroys
 *          none of the borrowed bootstrap objects.
 */
struct NativePresentationRuntimeCreateInfoVersion1 {
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
};

/**
 * @note ThreadSafety: Plain output record; separate instances are concurrency-safe.
 * @brief Creation result containing one Native-owned opaque runtime address.
 * @warning MemoryOwnership: Native owns runtimeAddress after Success; Java must destroy it
 *          exactly once before destroying any borrowed Vulkan bootstrap object.
 */
struct NativePresentationRuntimeCreateResultVersion1 {
    std::uint64_t runtimeAddress;
    std::int32_t vulkanResult;
    std::uint32_t selectedFormatValue;
    std::uint32_t selectedPresentModeValue;
    std::uint32_t selectedSharingModeValue;
    std::uint32_t swapchainImageCount;
    std::uint32_t reserved;
};

static_assert(sizeof(NativePresentationRuntimeCreateInfoVersion1) == 72u);
static_assert(offsetof(NativePresentationRuntimeCreateInfoVersion1, surfaceHandle) == 24u);
static_assert(offsetof(NativePresentationRuntimeCreateInfoVersion1,
                       graphicsQueueFamilyIndex) == 48u);
static_assert(std::is_standard_layout_v<NativePresentationRuntimeCreateInfoVersion1>);
static_assert(std::is_trivially_copyable_v<NativePresentationRuntimeCreateInfoVersion1>);
static_assert(sizeof(NativePresentationRuntimeCreateResultVersion1) == 32u);
static_assert(std::is_standard_layout_v<NativePresentationRuntimeCreateResultVersion1>);

/**
 * @note ThreadSafety: Plain output record; separate instances are concurrency-safe.
 * @brief Completed-frame CPU/GPU timings and identity (§6.7.3). GPU durations are populated
 *        only when timestamp metrics are compiled in; validFlags reports which are valid.
 */
struct NativePresentationFrameMetricsVersion1 {
    std::uint64_t frameSequence;
    std::uint64_t swapchainGeneration;
    std::uint32_t frameSlotIndex;
    std::uint32_t imageIndex;
    std::uint32_t validFlags;
    std::int32_t presentResultValue;
    std::uint64_t fenceWaitNanoseconds;
    std::uint64_t acquireNanoseconds;
    std::uint64_t nativeSubmitCallNanoseconds;
    std::uint64_t presentCallNanoseconds;
    std::uint64_t totalCpuFrameNanoseconds;
    std::uint64_t computeGpuNanoseconds;
    std::uint64_t graphicsGpuNanoseconds;
    std::uint64_t finalTransferGpuNanoseconds;
    std::uint64_t totalSubmittedGpuNanoseconds;
    std::uint32_t presentModeValue;
    std::uint32_t sharingModeValue;
};

static_assert(sizeof(NativePresentationFrameMetricsVersion1) == 112u);
static_assert(offsetof(NativePresentationFrameMetricsVersion1, fenceWaitNanoseconds) == 32u);
static_assert(offsetof(NativePresentationFrameMetricsVersion1, presentModeValue) == 104u);
static_assert(std::is_standard_layout_v<NativePresentationFrameMetricsVersion1>);
static_assert(std::is_trivially_copyable_v<NativePresentationFrameMetricsVersion1>);

/**
 * @note ThreadSafety: Plain output record; separate instances are concurrency-safe.
 * @brief Begin-frame identity and status; priorMetricsValid marks a populated metrics
 *        out-parameter for the prior frame that reused this slot.
 */
struct NativePresentationBeginFrameResultVersion1 {
    std::uint32_t frameStatusValue;
    std::uint32_t frameSlotIndex;
    std::uint32_t imageIndex;
    std::uint32_t priorMetricsValid;
    std::uint64_t frameSequence;
    std::uint64_t swapchainGeneration;
    std::int32_t vulkanResult;
    std::uint32_t reserved;
};

static_assert(sizeof(NativePresentationBeginFrameResultVersion1) == 40u);
static_assert(std::is_standard_layout_v<NativePresentationBeginFrameResultVersion1>);

/**
 * @note ThreadSafety: Plain output record; separate instances are concurrency-safe.
 * @brief Submit-and-present status and raw Vulkan result.
 */
struct NativePresentationSubmitFrameResultVersion1 {
    std::uint32_t frameStatusValue;
    std::int32_t vulkanResult;
};

static_assert(sizeof(NativePresentationSubmitFrameResultVersion1) == 8u);
static_assert(std::is_standard_layout_v<NativePresentationSubmitFrameResultVersion1>);

} // namespace barrieww

/**
 * @note ThreadSafety: Not thread-safe; call on the single presentation/render thread.
 * @brief Creates the Native-owned Version 1 swapchain and frame runtime around Java-owned
 *        Vulkan bootstrap handles.
 * @param const barrieww::NativePresentationRuntimeCreateInfoVersion1* createInfo Borrowed
 *        fixed-layout creation input
 * @param barrieww::NativePresentationRuntimeCreateResultVersion1* createResult Writable
 *        creation result, cleared before work
 * @return barrieww::NativePresentationRuntimeOperationResult Stable operation result
 * @warning MemoryOwnership: Java owns both records and every input handle; Native borrows
 *          them during the call. Success transfers ownership only of runtimeAddress to Java.
 */
extern "C" BARRIEWW_PRESENTATION_FFM_EXPORT
barrieww::NativePresentationRuntimeOperationResult
barriEwwCreatePresentationRuntimeVersion1(
    const barrieww::NativePresentationRuntimeCreateInfoVersion1* createInfo,
    barrieww::NativePresentationRuntimeCreateResultVersion1* createResult) noexcept;

/**
 * @note ThreadSafety: Not thread-safe; call on the owning presentation/render thread.
 * @brief Drains and destroys one Version 1 Native presentation runtime.
 * @param std::uint64_t runtimeAddress Native-owned address returned by create
 * @return barrieww::NativePresentationRuntimeOperationResult Stable operation result
 * @warning MemoryOwnership: Native frees the runtime. Java must never reuse the address.
 */
extern "C" BARRIEWW_PRESENTATION_FFM_EXPORT
barrieww::NativePresentationRuntimeOperationResult
barriEwwDestroyPresentationRuntimeVersion1(std::uint64_t runtimeAddress) noexcept;

/**
 * @note ThreadSafety: Not thread-safe; call on the owning presentation/render thread.
 * @brief Waits the next frame slot, acquires an image and reports prior-frame metrics.
 * @param std::uint64_t runtimeAddress Native-owned runtime address
 * @param std::uint32_t framebufferWidth Current framebuffer width; 0 means unavailable
 * @param std::uint32_t framebufferHeight Current framebuffer height; 0 means unavailable
 * @param barrieww::NativePresentationBeginFrameResultVersion1* beginResult Writable result
 * @param barrieww::NativePresentationFrameMetricsVersion1* priorMetrics Writable metrics of
 *        the prior frame that reused this slot; cleared then filled when valid
 * @return barrieww::NativePresentationRuntimeOperationResult Stable operation result
 * @warning MemoryOwnership: Java owns both output records; Native writes them synchronously.
 */
extern "C" BARRIEWW_PRESENTATION_FFM_EXPORT
barrieww::NativePresentationRuntimeOperationResult
barriEwwBeginPresentationFrameVersion1(
    std::uint64_t runtimeAddress, std::uint32_t framebufferWidth,
    std::uint32_t framebufferHeight,
    barrieww::NativePresentationBeginFrameResultVersion1* beginResult,
    barrieww::NativePresentationFrameMetricsVersion1* priorMetrics) noexcept;

/**
 * @note ThreadSafety: Not thread-safe; call on the owning presentation/render thread.
 * @brief Submits the open frame's prerecorded command buffer and presents it.
 * @param std::uint64_t runtimeAddress Native-owned runtime address
 * @param std::uint64_t commandBufferHandle Prerecorded primary command buffer, or 0 for none
 * @param barrieww::NativePresentationSubmitFrameResultVersion1* submitResult Writable result
 * @return barrieww::NativePresentationRuntimeOperationResult Stable operation result
 * @warning MemoryOwnership: Java owns submitResult and the borrowed command buffer.
 */
extern "C" BARRIEWW_PRESENTATION_FFM_EXPORT
barrieww::NativePresentationRuntimeOperationResult
barriEwwSubmitPresentationFrameVersion1(
    std::uint64_t runtimeAddress, std::uint64_t commandBufferHandle,
    barrieww::NativePresentationSubmitFrameResultVersion1* submitResult) noexcept;
