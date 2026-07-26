#pragma once

#include <cstdint>

#include "BarriEww/Vulkan/VulkanGraphicsPipelineTableCreationError.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief One graphics pipeline materialization failure: the category, the pipeline
 *        slot it occurred on and the raw VkResult value where a Vulkan call was
 *        involved (0 otherwise).
 */
struct VulkanGraphicsPipelineTableCreationFailure {
    VulkanGraphicsPipelineTableCreationError error;
    std::uint32_t slotIndex;
    std::int32_t resultValue;
};

} // namespace barrieww
