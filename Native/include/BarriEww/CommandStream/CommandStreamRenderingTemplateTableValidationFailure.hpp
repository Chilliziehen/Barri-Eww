#pragma once

#include <cstdint>

#include "BarriEww/CommandStream/CommandStreamRenderingTemplateTableValidationError.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief One RenderingTemplateTable validation failure: the category plus the byte
 *        offset within the table section at which validation stopped (a directory
 *        record offset for template-level failures, an attachment record offset for
 *        attachment-level ones, 0 for whole-table failures).
 */
struct CommandStreamRenderingTemplateTableValidationFailure {
    CommandStreamRenderingTemplateTableValidationError error;
    std::uint64_t byteOffset;
};

} // namespace barrieww
