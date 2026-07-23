#pragma once

#include <cstdint>
#include <expected>
#include <vector>

#include <vulkan/vulkan.h>

namespace barrieww {

/**
 * @note ThreadSafety: Single-threaded creation/destruction and frame access.
 * @brief Owns the baseline visible presentation swapchain generation while borrowing the
 *        Java-created device, surface and queues (ADR-0004).
 * @warning MemoryOwnership: Owns swapchain and image views. Borrows all bootstrap handles.
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

    /** Creates one MAILBOX→FIFO swapchain generation with TRANSFER_DST support. */
    [[nodiscard]] static std::expected<VulkanPresentationRuntime, CreationFailure>
    create(const CreateInfo& createInfo);

    VulkanPresentationRuntime(const VulkanPresentationRuntime&) = delete;
    VulkanPresentationRuntime& operator=(const VulkanPresentationRuntime&) = delete;

    /** Move transfers every owned swapchain object. */
    VulkanPresentationRuntime(VulkanPresentationRuntime&& movedFrom) noexcept;

    VulkanPresentationRuntime& operator=(VulkanPresentationRuntime&&) = delete;

    /** Destroys image views and swapchain after draining both borrowed queues. */
    ~VulkanPresentationRuntime();

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

private:
    VulkanPresentationRuntime(VkDevice logicalDevice, VkQueue graphicsQueue,
                              VkQueue presentQueue, VkSurfaceKHR surface,
                              VkSwapchainKHR swapchain, VkSurfaceFormatKHR surfaceFormat,
                              VkPresentModeKHR presentMode, VkSharingMode sharingMode,
                              std::vector<VkImage> images,
                              std::vector<VkImageView> imageViews) noexcept;

    VkDevice m_logicalDevice = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkSurfaceFormatKHR m_surfaceFormat{};
    VkPresentModeKHR m_presentMode = VK_PRESENT_MODE_FIFO_KHR;
    VkSharingMode m_sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
};

} // namespace barrieww
