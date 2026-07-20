#pragma once

#include <cstdint>

#include "BarriEww/CommandStream/CommandStreamBarrierBatchTableValidationError.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief One BarrierBatchTable validation failure: the category plus the byte offset
 *        within the table section at which validation stopped (0 for whole-table
 *        failures, the directory record offset for batch-level failures, the barrier
 *        record offset for record-level failures).
 */
struct CommandStreamBarrierBatchTableValidationFailure {
    CommandStreamBarrierBatchTableValidationError error;
    std::uint64_t byteOffset;
};

} // namespace barrieww
