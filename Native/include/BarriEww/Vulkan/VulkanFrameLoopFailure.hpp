#pragma once

#include <cstdint>

#include "BarriEww/Vulkan/VulkanFrameLoopError.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief One frame loop failure: the category, the frame slot involved (UINT32_MAX for
 *        failures not tied to one slot) and the raw VkResult value where a Vulkan call
 *        was involved (0 otherwise).
 */
struct VulkanFrameLoopFailure {
    VulkanFrameLoopError error;
    std::uint32_t frameSlot;
    std::int32_t resultValue;
};

} // namespace barrieww
