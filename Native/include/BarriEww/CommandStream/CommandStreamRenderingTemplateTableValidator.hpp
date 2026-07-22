#pragma once

#include <cstddef>
#include <expected>
#include <span>

#include "BarriEww/CommandStream/CommandStreamRenderingTemplateTableValidationFailure.hpp"
#include "BarriEww/CommandStream/CommandStreamRenderingTemplateTableView.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Stateless; validate() is a pure function of its input bytes and
 *       may be called concurrently on distinct or identical byte ranges.
 * @brief Load-time schema validator for RenderingTemplateTable sections (schema v0.1):
 *        an 8-byte header (templateCount, zero reserved flags), a 40-byte directory
 *        record per template, then per-template attachment regions (color records first,
 *        then one depth record when present). Structural rules and per-attachment field
 *        assignment are checked here; cross-table image-view existence and format/layout
 *        compatibility deliberately remain materialization/record-time checks (ADR-0002
 *        D5). Attachment regions MAY be shared between templates (deduplication), so no
 *        overlap check is performed.
 */
class CommandStreamRenderingTemplateTableValidator {
public:
    /**
     * @note ThreadSafety: Thread-safe (pure function, see class note).
     * @brief Fully validates one RenderingTemplateTable section.
     * @param tableBytes The complete section byte range, table header included. Must be
     *        8-byte aligned at its base.
     * @return std::expected<CommandStreamRenderingTemplateTableView,
     *         CommandStreamRenderingTemplateTableValidationFailure> A view on success;
     *         the first failure (category + byte offset) otherwise. Errors are values,
     *         not exceptions, so the FFM boundary needs no unwinding (§6.4).
     * @warning MemoryOwnership: tableBytes is BORROWED (§6.3); the returned view aliases
     *          it and the provider must keep it alive and unmodified while the view is
     *          in use.
     */
    [[nodiscard]] static std::expected<CommandStreamRenderingTemplateTableView,
                                       CommandStreamRenderingTemplateTableValidationFailure>
    validate(std::span<const std::byte> tableBytes) noexcept;
};

} // namespace barrieww
