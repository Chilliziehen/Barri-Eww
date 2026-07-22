#pragma once

#include <cstdint>

#include "BarriEww/Vulkan/VulkanImageViewTableCreationError.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief One image-view materialization failure: stable category, the failing view
 *        slot, its source image slot and the raw VkResult where a Vulkan call occurred.
 */
struct VulkanImageViewTableCreationFailure {
    VulkanImageViewTableCreationError error;
    std::uint32_t imageViewSlotIndex;
    std::uint32_t sourceImageSlotIndex;
    std::int32_t resultValue;
};

} // namespace barrieww
