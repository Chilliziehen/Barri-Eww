#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "BarriEww/CommandStream/CommandStreamAttachmentLoadOp.hpp"
#include "BarriEww/CommandStream/CommandStreamAttachmentStoreOp.hpp"
#include "BarriEww/CommandStream/CommandStreamCompareOperation.hpp"
#include "BarriEww/CommandStream/CommandStreamCullMode.hpp"
#include "BarriEww/CommandStream/CommandStreamFrontFace.hpp"
#include "BarriEww/CommandStream/CommandStreamImageAspect.hpp"
#include "BarriEww/CommandStream/CommandStreamImageFormat.hpp"
#include "BarriEww/CommandStream/CommandStreamImageLayout.hpp"
#include "BarriEww/CommandStream/CommandStreamPrimitiveTopology.hpp"

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

/**
 * @note ThreadSafety: Pure function; fully thread-safe.
 * @brief Maps a backend-neutral attachment load operation onto VkAttachmentLoadOp.
 * @param loadOp The assigned neutral load operation.
 * @return VkAttachmentLoadOp The concrete Vulkan load operation.
 */
[[nodiscard]] constexpr VkAttachmentLoadOp
mapCommandStreamAttachmentLoadOp(CommandStreamAttachmentLoadOp loadOp) noexcept {
    switch (loadOp) {
        case CommandStreamAttachmentLoadOp::Load: return VK_ATTACHMENT_LOAD_OP_LOAD;
        case CommandStreamAttachmentLoadOp::Clear: return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case CommandStreamAttachmentLoadOp::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

/**
 * @note ThreadSafety: Pure function; fully thread-safe.
 * @brief Maps a backend-neutral attachment store operation onto VkAttachmentStoreOp.
 * @param storeOp The assigned neutral store operation.
 * @return VkAttachmentStoreOp The concrete Vulkan store operation.
 */
[[nodiscard]] constexpr VkAttachmentStoreOp
mapCommandStreamAttachmentStoreOp(CommandStreamAttachmentStoreOp storeOp) noexcept {
    switch (storeOp) {
        case CommandStreamAttachmentStoreOp::Store: return VK_ATTACHMENT_STORE_OP_STORE;
        case CommandStreamAttachmentStoreOp::DontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }
    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

/**
 * @note ThreadSafety: Pure function; fully thread-safe.
 * @brief Maps a backend-neutral primitive topology onto VkPrimitiveTopology.
 * @param topology The assigned neutral topology.
 * @return VkPrimitiveTopology The concrete Vulkan topology.
 */
[[nodiscard]] constexpr VkPrimitiveTopology
mapCommandStreamPrimitiveTopology(CommandStreamPrimitiveTopology topology) noexcept {
    switch (topology) {
        case CommandStreamPrimitiveTopology::PointList:
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case CommandStreamPrimitiveTopology::LineList:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case CommandStreamPrimitiveTopology::LineStrip:
            return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case CommandStreamPrimitiveTopology::TriangleList:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case CommandStreamPrimitiveTopology::TriangleStrip:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    }
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

/**
 * @note ThreadSafety: Pure function; fully thread-safe.
 * @brief Maps a backend-neutral compare operation onto VkCompareOp.
 * @param compareOperation The assigned neutral compare operation.
 * @return VkCompareOp The concrete Vulkan compare operation.
 */
[[nodiscard]] constexpr VkCompareOp
mapCommandStreamCompareOperation(CommandStreamCompareOperation compareOperation) noexcept {
    switch (compareOperation) {
        case CommandStreamCompareOperation::Never: return VK_COMPARE_OP_NEVER;
        case CommandStreamCompareOperation::Less: return VK_COMPARE_OP_LESS;
        case CommandStreamCompareOperation::Equal: return VK_COMPARE_OP_EQUAL;
        case CommandStreamCompareOperation::LessOrEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
        case CommandStreamCompareOperation::Greater: return VK_COMPARE_OP_GREATER;
        case CommandStreamCompareOperation::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
        case CommandStreamCompareOperation::GreaterOrEqual:
            return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case CommandStreamCompareOperation::Always: return VK_COMPARE_OP_ALWAYS;
    }
    return VK_COMPARE_OP_NEVER;
}

/**
 * @note ThreadSafety: Pure function; fully thread-safe.
 * @brief Maps a backend-neutral cull mode onto VkCullModeFlags.
 * @param cullMode The assigned neutral cull mode.
 * @return VkCullModeFlags The concrete Vulkan cull mode flags.
 */
[[nodiscard]] constexpr VkCullModeFlags
mapCommandStreamCullMode(CommandStreamCullMode cullMode) noexcept {
    switch (cullMode) {
        case CommandStreamCullMode::None: return VK_CULL_MODE_NONE;
        case CommandStreamCullMode::Front: return VK_CULL_MODE_FRONT_BIT;
        case CommandStreamCullMode::Back: return VK_CULL_MODE_BACK_BIT;
        case CommandStreamCullMode::FrontAndBack: return VK_CULL_MODE_FRONT_AND_BACK;
    }
    return VK_CULL_MODE_NONE;
}

/**
 * @note ThreadSafety: Pure function; fully thread-safe.
 * @brief Maps a backend-neutral front-face winding order onto VkFrontFace.
 * @param frontFace The assigned neutral front-face order.
 * @return VkFrontFace The concrete Vulkan front-face order.
 */
[[nodiscard]] constexpr VkFrontFace
mapCommandStreamFrontFace(CommandStreamFrontFace frontFace) noexcept {
    switch (frontFace) {
        case CommandStreamFrontFace::CounterClockwise:
            return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        case CommandStreamFrontFace::Clockwise: return VK_FRONT_FACE_CLOCKWISE;
    }
    return VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

} // namespace barrieww
