#pragma once

#include <vulkan/vulkan.h>

#include "BarriEww/Interoperability/PresentationImageFormat.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Pure function; fully thread-safe.
 * @brief Maps a presentation-neutral image format onto its exact Vulkan UNORM format.
 * @param barrieww::PresentationImageFormat presentationImageFormat The neutral format.
 * @return VkFormat The concrete Vulkan format, or VK_FORMAT_UNDEFINED for an unknown value.
 */
[[nodiscard]] constexpr VkFormat
mapPresentationImageFormat(PresentationImageFormat presentationImageFormat) noexcept {
    switch (presentationImageFormat) {
        case PresentationImageFormat::R8G8B8A8Unorm: return VK_FORMAT_R8G8B8A8_UNORM;
        case PresentationImageFormat::B8G8R8A8Unorm: return VK_FORMAT_B8G8R8A8_UNORM;
    }
    return VK_FORMAT_UNDEFINED;
}

} // namespace barrieww
