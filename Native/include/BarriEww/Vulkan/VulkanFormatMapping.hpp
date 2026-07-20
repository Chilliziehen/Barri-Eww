#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "BarriEww/CommandStream/CommandStreamImageAspect.hpp"
#include "BarriEww/CommandStream/CommandStreamImageFormat.hpp"
#include "BarriEww/CommandStream/CommandStreamImageLayout.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Pure functions; fully thread-safe.
 * @brief Maps the backend-neutral image format onto VkFormat (T0[3] mapping point;
 *        the DX12 backend will own its DXGI mapping).
 * @param imageFormat The assigned neutral format.
 * @return VkFormat The concrete Vulkan format.
 */
[[nodiscard]] constexpr VkFormat
mapCommandStreamImageFormat(CommandStreamImageFormat imageFormat) noexcept {
    switch (imageFormat) {
        case CommandStreamImageFormat::R8G8B8A8Unorm: return VK_FORMAT_R8G8B8A8_UNORM;
        case CommandStreamImageFormat::B8G8R8A8Unorm: return VK_FORMAT_B8G8R8A8_UNORM;
        case CommandStreamImageFormat::R16G16B16A16Float:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case CommandStreamImageFormat::R32Uint: return VK_FORMAT_R32_UINT;
        case CommandStreamImageFormat::D32Float: return VK_FORMAT_D32_SFLOAT;
        case CommandStreamImageFormat::D24UnormS8Uint: return VK_FORMAT_D24_UNORM_S8_UINT;
    }
    return VK_FORMAT_UNDEFINED;
}

/**
 * @note ThreadSafety: Pure function; fully thread-safe.
 * @brief Maps the backend-neutral image layout onto VkImageLayout.
 * @param imageLayout The assigned neutral layout.
 * @return VkImageLayout The concrete Vulkan layout.
 */
[[nodiscard]] constexpr VkImageLayout
mapCommandStreamImageLayout(CommandStreamImageLayout imageLayout) noexcept {
    switch (imageLayout) {
        case CommandStreamImageLayout::Undefined: return VK_IMAGE_LAYOUT_UNDEFINED;
        case CommandStreamImageLayout::General: return VK_IMAGE_LAYOUT_GENERAL;
        case CommandStreamImageLayout::ColorAttachment:
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case CommandStreamImageLayout::DepthStencilAttachment:
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case CommandStreamImageLayout::ShaderReadOnly:
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case CommandStreamImageLayout::TransferSource:
            return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case CommandStreamImageLayout::TransferDestination:
            return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        case CommandStreamImageLayout::Present: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

/**
 * @note ThreadSafety: Pure function; fully thread-safe.
 * @brief Maps a backend-neutral aspect mask onto VkImageAspectFlags.
 * @param neutralAspectMask A usable neutral aspect mask (CommandStreamImageAspect bits).
 * @return VkImageAspectFlags The concrete Vulkan aspect mask.
 */
[[nodiscard]] constexpr VkImageAspectFlags
mapCommandStreamImageAspectMask(std::uint32_t neutralAspectMask) noexcept {
    VkImageAspectFlags vulkanAspectMask = 0;
    if (neutralAspectMask & static_cast<std::uint32_t>(CommandStreamImageAspect::Color)) {
        vulkanAspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
    }
    if (neutralAspectMask & static_cast<std::uint32_t>(CommandStreamImageAspect::Depth)) {
        vulkanAspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    if (neutralAspectMask & static_cast<std::uint32_t>(CommandStreamImageAspect::Stencil)) {
        vulkanAspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    return vulkanAspectMask;
}

} // namespace barrieww
