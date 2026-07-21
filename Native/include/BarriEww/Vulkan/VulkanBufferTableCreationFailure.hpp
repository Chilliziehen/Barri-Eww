#pragma once

#include <cstdint>

#include "BarriEww/Vulkan/VulkanBufferTableCreationError.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief One buffer materialization failure: the category, the slot it occurred on and
 *        the raw VkResult value where a Vulkan call was involved (0 otherwise).
 */
struct VulkanBufferTableCreationFailure {
    VulkanBufferTableCreationError error;
    std::uint32_t slotIndex;
    std::int32_t resultValue;
};

} // namespace barrieww
