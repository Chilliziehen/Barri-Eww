#pragma once

#include <cstdint>

#include "BarriEww/Vulkan/VulkanImageTableCreationError.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief One image materialization failure: the category, the slot it occurred on and
 *        the raw VkResult value where a Vulkan call was involved (0 otherwise).
 */
struct VulkanImageTableCreationFailure {
    VulkanImageTableCreationError error;
    std::uint32_t slotIndex;
    std::int32_t resultValue;
};

} // namespace barrieww
