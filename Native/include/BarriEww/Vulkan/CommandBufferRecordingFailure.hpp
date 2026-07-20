#pragma once

#include <cstdint>

#include "BarriEww/Vulkan/CommandBufferRecordingError.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief One recording failure: the category, the zero-based index of the offending
 *        command within the lane stream (UINT32_MAX for begin/end failures that are
 *        not tied to one command) and the raw VkResult value where a Vulkan call was
 *        involved (0 otherwise).
 */
struct CommandBufferRecordingFailure {
    CommandBufferRecordingError error;
    std::uint32_t commandIndex;
    std::int32_t resultValue;
};

} // namespace barrieww
