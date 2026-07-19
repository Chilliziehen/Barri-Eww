#pragma once

#include <cstdint>

#include "BarriEww/CommandStream/CommandStreamValidationError.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief One load-time validation failure: the category plus the byte offset within the
 *        stream at which validation stopped (0 for failures concerning the stream as a
 *        whole, such as size or alignment of the base address).
 */
struct CommandStreamValidationFailure {
    CommandStreamValidationError error;
    std::uint64_t byteOffset;
};

} // namespace barrieww
