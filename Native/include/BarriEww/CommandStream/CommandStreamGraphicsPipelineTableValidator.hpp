#pragma once

#include <cstddef>
#include <expected>
#include <span>

#include "BarriEww/CommandStream/CommandStreamGraphicsPipelineTableValidationFailure.hpp"
#include "BarriEww/CommandStream/CommandStreamGraphicsPipelineTableView.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Stateless; validate() is a pure function of its input bytes and
 *       may be called concurrently on distinct or identical byte ranges.
 * @brief Load-time schema validator for GraphicsPipelineTable sections (schema v0.1,
 *        §9.14): an 8-byte header (pipelineCount, zero reserved flags) then fixed
 *        80-byte records — the table size is exact. Structural rules and per-record
 *        field assignment are checked here; shader-module slot existence deliberately
 *        remains a materialization-time cross-table check, and attachment-format
 *        compatibility with the rendering template a record-time one (ADR-0002 D5).
 */
class CommandStreamGraphicsPipelineTableValidator {
public:
    /**
     * @note ThreadSafety: Thread-safe (pure function, see class note).
     * @brief Fully validates one GraphicsPipelineTable section.
     * @param tableBytes The complete section byte range, table header included. Must be
     *        8-byte aligned at its base.
     * @return std::expected<CommandStreamGraphicsPipelineTableView,
     *         CommandStreamGraphicsPipelineTableValidationFailure> A view on success;
     *         the first failure (category + byte offset) otherwise. Errors are values,
     *         not exceptions, so the FFM boundary needs no unwinding (§6.4).
     * @warning MemoryOwnership: tableBytes is BORROWED (§6.3); the returned view aliases
     *          it and the provider must keep it alive and unmodified while the view is
     *          in use.
     */
    [[nodiscard]] static std::expected<CommandStreamGraphicsPipelineTableView,
                                       CommandStreamGraphicsPipelineTableValidationFailure>
    validate(std::span<const std::byte> tableBytes) noexcept;
};

} // namespace barrieww
