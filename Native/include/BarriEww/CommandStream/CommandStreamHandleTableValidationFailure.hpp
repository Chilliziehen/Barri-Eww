#pragma once

#include <cstdint>

#include "BarriEww/CommandStream/CommandStreamHandleTableValidationError.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief One handle-table validation failure: the category plus the byte offset within
 *        the table section at which validation stopped (0 for failures concerning the
 *        table as a whole; the entry's absolute offset for entry-level failures).
 */
struct CommandStreamHandleTableValidationFailure {
    CommandStreamHandleTableValidationError error;
    std::uint64_t byteOffset;
};

} // namespace barrieww
