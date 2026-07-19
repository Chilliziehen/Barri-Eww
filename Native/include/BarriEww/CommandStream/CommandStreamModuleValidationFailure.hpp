#pragma once

#include <cstdint>
#include <optional>

#include "BarriEww/CommandStream/CommandStreamModuleValidationError.hpp"
#include "BarriEww/CommandStream/CommandStreamValidationFailure.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief One module load-time validation failure: the category, the byte offset within
 *        the module at which validation stopped (a directory entry offset for
 *        directory-level failures, the section offset for section-level ones, 0 for
 *        whole-module failures), and — for LaneStreamInvalid — the nested lane-stream
 *        failure with its offset relative to that lane stream's own base.
 */
struct CommandStreamModuleValidationFailure {
    CommandStreamModuleValidationError error;
    std::uint64_t byteOffset;
    std::optional<CommandStreamValidationFailure> laneStreamFailure;
};

} // namespace barrieww
