#pragma once

#include <cstdint>

#include "BarriEww/CommandStream/CommandStreamGraphicsPipelineTableValidationError.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief One GraphicsPipelineTable validation failure: the category plus the byte
 *        offset within the table section at which validation stopped (a record offset
 *        for record-level failures, 0 for whole-table failures).
 */
struct CommandStreamGraphicsPipelineTableValidationFailure {
    CommandStreamGraphicsPipelineTableValidationError error;
    std::uint64_t byteOffset;
};

} // namespace barrieww
