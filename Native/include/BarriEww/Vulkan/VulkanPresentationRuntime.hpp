#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <optional>
#include <vector>

#include <vulkan/vulkan.h>

#include "BarriEww/Vulkan/VulkanHostImagePresentationResources.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Single-threaded creation/destruction and frame access.
 * @brief Owns the baseline visible presentation swapchain generation while borrowing the
 *        Java-created device, surface and queues (ADR-0004).
 * @warning MemoryOwnership: Owns swapchain, image views, per-frame synchronization objects,
 *          and optional host-image-dependent resources. Borrows all bootstrap handles and
 *          never owns a host image or its memory.
 */
class VulkanPresentationRuntime {
public:
    struct CreateInfo {
        VkPhysicalDevice physicalDevice;
        VkDevice logicalDevice;
        VkSurfaceKHR surface;
        VkQueue graphicsQueue;
        VkQueue presentQueue;
        std::uint32_t graphicsQueueFamilyIndex;
        std::uint32_t presentQueueFamilyIndex;
        std::uint32_t framebufferWidth;
        std::uint32_t framebufferHeight;
        std::uint32_t framesInFlightCount;
    };

    struct CreationFailure {
        std::int32_t vulkanResult;
        bool unsupportedSurface;
    };

    /**
     * @note ThreadSafety: Immutable creation input consumed on the presentation thread.
     * @brief Extends the borrowed bootstrap input with one exact host-image generation.
     * @warning MemoryOwnership: Every encoded Vulkan handle remains caller-owned. The host
     *          image must outlive attached host-image resources or the complete runtime.
     */
    struct HostImageCreateInfo {
        CreateInfo presentationCreateInfo;
        VkImage hostImage;
        VkFormat hostImageFormat;
        VkFormat requestedSurfaceFormat;
        std::uint32_t hostImageWidth;
        std::uint32_t hostImageHeight;
    };

    /**
     * @brief Completed-frame CPU timing and identity (FrameMetricsVersion1 semantics).
     *        GPU durations are populated only when timestamp metrics are compiled in.
     */
    struct FrameMetrics {
        std::uint64_t frameSequence = 0u;
        std::uint64_t swapchainGeneration = 0u;
        std::uint32_t frameSlotIndex = 0u;
        std::uint32_t imageIndex = 0u;
        std::int32_t presentResultValue = 0;
        std::uint32_t validFlags = 0u;
        std::uint64_t fenceWaitNanoseconds = 0u;
        std::uint64_t acquireNanoseconds = 0u;
        std::uint64_t nativeSubmitCallNanoseconds = 0u;
        std::uint64_t presentCallNanoseconds = 0u;
        std::uint64_t totalCpuFrameNanoseconds = 0u;
        std::uint64_t computeGpuNanoseconds = 0u;
        std::uint64_t graphicsGpuNanoseconds = 0u;
        std::uint64_t finalTransferGpuNanoseconds = 0u;
        std::uint64_t totalSubmittedGpuNanoseconds = 0u;
    };

    /** The validFlags bit set once a completed frame recorded CPU timings. */
    static constexpr std::uint32_t s_cpuMetricsValidFlag = 0x1u;

    enum class FrameStatus : std::uint32_t {
        Success = 0,
        SurfaceUnavailable = 1,
        RecreateRequired = 2,
        Suboptimal = 3,
    };

    struct BeginFrameResult {
        FrameStatus status = FrameStatus::Success;
        std::uint32_t frameSlotIndex = 0u;
        std::uint32_t imageIndex = 0u;
        std::uint64_t frameSequence = 0u;
        std::uint64_t swapchainGeneration = 0u;
        bool priorMetricsValid = false;
        FrameMetrics priorMetrics{};
        std::int32_t vulkanResult = 0;
    };

    struct SubmitFrameResult {
        FrameStatus status = FrameStatus::Success;
        std::int32_t vulkanResult = 0;
    };

    /** Creates one MAILBOX→FIFO swapchain generation with TRANSFER_DST support. */
    [[nodiscard]] static std::expected<VulkanPresentationRuntime, CreationFailure>
    create(const CreateInfo& createInfo);

    /**
     * @note ThreadSafety: Creation is confined to one presentation thread.
     * @brief Creates an exact UNORM swapchain and transactionally attaches the fixed
     *        host-image presentation resources.
     * @param const HostImageCreateInfo& createInfo Borrowed bootstrap and host-image input
     * @return std::expected<VulkanPresentationRuntime, CreationFailure> Complete runtime or
     *         exact Vulkan/unsupported-surface failure
     * @warning MemoryOwnership: Success owns the swapchain and dependent host resources but
     *          continues to borrow every bootstrap handle and the host image.
     */
    [[nodiscard]] static std::expected<VulkanPresentationRuntime, CreationFailure>
    createHostImagePresentation(const HostImageCreateInfo& createInfo);

    VulkanPresentationRuntime(const VulkanPresentationRuntime&) = delete;
    VulkanPresentationRuntime& operator=(const VulkanPresentationRuntime&) = delete;

    /** Move transfers every owned swapchain and synchronization object. */
    VulkanPresentationRuntime(VulkanPresentationRuntime&& movedFrom) noexcept;

    VulkanPresentationRuntime& operator=(VulkanPresentationRuntime&&) = delete;

    /** Destroys host resources, sync objects, image views and swapchain after queue drain. */
    ~VulkanPresentationRuntime();

    /**
     * @brief Waits the next frame slot, acquires a swapchain image and reports the metrics
     *        of the previous frame that reused that slot.
     * @param framebufferWidth Current framebuffer width; 0 means the surface is unavailable
     * @param framebufferHeight Current framebuffer height; 0 means the surface is unavailable
     */
    [[nodiscard]] BeginFrameResult beginFrame(std::uint32_t framebufferWidth,
                                              std::uint32_t framebufferHeight);

    /**
     * @brief Submits the prerecorded command buffer for the open frame and presents it.
     * @param commandBuffer The prerecorded primary command buffer, or VK_NULL_HANDLE for a
     *        transition-free present of the acquired image
     */
    [[nodiscard]] SubmitFrameResult submitAndPresentFrame(VkCommandBuffer commandBuffer);

    /**
     * @brief Records a clear for the already-open frame, then submits and presents it.
     * @param const float clearColor[3] The RGB clear color (alpha is forced to 1)
     * @return SubmitFrameResult Submission status, with VK_NOT_READY when no frame is open
     */
    [[nodiscard]] SubmitFrameResult submitAndPresentClearFrame(const float clearColor[3]);

    /**
     * @note ThreadSafety: Render-thread confined; no concurrent frame or detach operation.
     * @brief Selects the open frame's prerecorded host matrix entry in constant time and
     *        delegates submission and presentation to the common frame path.
     * @return SubmitFrameResult Submission result; VK_NOT_READY when no frame is open or
     *         host-image resources are detached
     * @warning MemoryOwnership: Borrows the selected command buffer from attached resources.
     */
    [[nodiscard]] SubmitFrameResult submitAndPresentHostImageFrame();

    /**
     * @note ThreadSafety: Render-thread confined; no concurrent submission is permitted.
     * @brief Waits only submitted host-image frame slots, then idempotently destroys every
     *        resource that depends on the borrowed host image while preserving the runtime.
     * @return std::expected<void, VkResult> Success or exact fence-wait failure; failure
     *         retains all host-image resources
     * @warning MemoryOwnership: Releases only Native-owned host-image-dependent resources.
     */
    [[nodiscard]] std::expected<void, VkResult> detachHostImagePresentationResources();

    /**
     * @brief Begins a frame, records a clear of the acquired swapchain image to the given
     *        color (transition Undefined -> TransferDestination -> clear -> Present), submits
     *        and presents it. This is the baseline visible milestone (ADR-0004 D5.1).
     * @param framebufferWidth Current framebuffer width; 0 signals an unavailable surface
     * @param framebufferHeight Current framebuffer height; 0 signals an unavailable surface
     * @param clearColor The RGBA clear color (alpha is forced to 1)
     * @param outBeginResult Receives the begin-frame identity, status and prior metrics
     */
    [[nodiscard]] SubmitFrameResult presentClearFrame(std::uint32_t framebufferWidth,
                                                      std::uint32_t framebufferHeight,
                                                      const float clearColor[3],
                                                      BeginFrameResult& outBeginResult);

    /** The selected concrete surface format. */
    [[nodiscard]] VkFormat imageFormat() const noexcept { return m_surfaceFormat.format; }

    /** The selected present mode (MAILBOX when available, FIFO otherwise). */
    [[nodiscard]] VkPresentModeKHR presentMode() const noexcept { return m_presentMode; }

    /** The selected swapchain sharing mode. */
    [[nodiscard]] VkSharingMode sharingMode() const noexcept { return m_sharingMode; }

    /** The number of swapchain images in this generation. */
    [[nodiscard]] std::uint32_t imageCount() const noexcept {
        return static_cast<std::uint32_t>(m_images.size());
    }

    /** The number of in-flight frame slots. */
    [[nodiscard]] std::uint32_t framesInFlightCount() const noexcept {
        return static_cast<std::uint32_t>(m_frameSlots.size());
    }

    /** The current swapchain generation identifier (increments on each recreation). */
    [[nodiscard]] std::uint64_t swapchainGeneration() const noexcept {
        return m_swapchainGeneration;
    }

    /** Whether a frame is currently open between beginFrame and submitAndPresentFrame. */
    [[nodiscard]] bool isFrameOpen() const noexcept { return m_isFrameOpen; }

    /** Whether the borrowed host image still has attached Native-owned dependent resources. */
    [[nodiscard]] bool hasHostImagePresentationResources() const noexcept {
        return m_hostImagePresentationResources.has_value();
    }

private:
    struct FrameSlot {
        VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
        VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
        VkFence inFlightFence = VK_NULL_HANDLE;
        FrameMetrics completedMetrics{};
        bool hasCompletedMetrics = false;
        bool hasSubmittedHostImageResources = false;
    };

    VulkanPresentationRuntime(VkDevice logicalDevice, VkQueue graphicsQueue,
                              VkQueue presentQueue, VkSurfaceKHR surface,
                              VkSwapchainKHR swapchain, VkCommandPool commandPool,
                              VkSurfaceFormatKHR surfaceFormat,
                              VkPresentModeKHR presentMode, VkSharingMode sharingMode,
                              std::vector<VkImage> images,
                              std::vector<VkImageView> imageViews,
                              std::vector<VkCommandBuffer> frameCommandBuffers,
                               std::vector<FrameSlot> frameSlots) noexcept;

    /**
     * @note ThreadSafety: Creation is confined to one presentation thread.
     * @brief Builds the shared swapchain/synchronization runtime under an explicit surface
     *        format, usage, extent, and failure-preservation policy.
     * @param const CreateInfo& createInfo Borrowed bootstrap and dimensions
     * @param VkFormat requiredSurfaceFormat Exact format, or VK_FORMAT_UNDEFINED for legacy
     *        SRGB preference and first-format fallback
     * @param VkImageUsageFlags requiredImageUsageFlags Required and created swapchain usage
     * @param bool requiresExactExtent Whether selected extent must equal framebuffer extent
     * @param bool preservesExactCreationFailures Whether synchronization and command-pool
     *        failures retain their exact VkResult instead of legacy out-of-memory mapping
     * @return std::expected<VulkanPresentationRuntime, CreationFailure> Runtime or failure
     * @warning MemoryOwnership: Transactionally owns created resources only on success.
     */
    [[nodiscard]] static std::expected<VulkanPresentationRuntime, CreationFailure>
    createSwapchainRuntime(const CreateInfo& createInfo, VkFormat requiredSurfaceFormat,
                           VkImageUsageFlags requiredImageUsageFlags,
                           bool requiresExactExtent,
                           bool preservesExactCreationFailures);

    /**
     * @note ThreadSafety: Render-thread confined; requires serialized frame access.
     * @brief Executes the common submit/present path and marks a frame-slot fence only after
     *        a successful submission that references attached host-image resources.
     * @param VkCommandBuffer commandBuffer Borrowed primary command buffer, or null
     * @param bool usesHostImageResources Whether the submission references host resources
     * @return SubmitFrameResult Submission and presentation status with raw Vulkan result
     * @warning MemoryOwnership: Borrows the command buffer and all runtime-owned sync state.
     */
    [[nodiscard]] SubmitFrameResult submitAndPresentFrameWithResourceTracking(
        VkCommandBuffer commandBuffer, bool usesHostImageResources);

    void recordClearCommandBuffer(VkCommandBuffer commandBuffer, VkImage swapchainImage,
                                  const float clearColor[3]);

    void destroyOwnedObjects() noexcept;

    VkDevice m_logicalDevice = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkSurfaceFormatKHR m_surfaceFormat{};
    VkPresentModeKHR m_presentMode = VK_PRESENT_MODE_FIFO_KHR;
    VkSharingMode m_sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
    std::vector<VkFence> m_imageInFlightFences;
    std::vector<VkCommandBuffer> m_frameCommandBuffers;
    std::vector<FrameSlot> m_frameSlots;
    std::optional<VulkanHostImagePresentationResources> m_hostImagePresentationResources;
    std::uint64_t m_swapchainGeneration = 1u;
    std::uint64_t m_frameSequence = 0u;
    std::uint32_t m_currentFrameSlot = 0u;
    std::uint32_t m_acquiredImageIndex = 0u;
    bool m_isFrameOpen = false;
    FrameMetrics m_openFrameMetrics{};
    std::chrono::steady_clock::time_point m_frameStartTimePoint{};
};

} // namespace barrieww
