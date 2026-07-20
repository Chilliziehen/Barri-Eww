#pragma once

#include <cstddef>
#include <expected>
#include <span>

#include "BarriEww/CommandStream/CommandStreamBarrierBatchTableValidationFailure.hpp"
#include "BarriEww/CommandStream/CommandStreamBarrierBatchTableView.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Stateless; validate() is a pure function of its input bytes and
 *       may be called concurrently on distinct or identical byte ranges.
 * @brief Load-time schema validator for BarrierBatchTable sections (schema v0.1):
 *        header, batch directory bounds, per-batch region alignment and bounds, and
 *        per-record rules (non-zero stage masks, zero reserved flags). Buffer slot
 *        resolution is the recorder's job (it owns the materialized buffer table);
 *        legacy-mappability of sync2 masks is a backend concern, also the recorder's.
 *        Runs once per table on the slow path (ADR-0002 D5 trust model).
 */
class CommandStreamBarrierBatchTableValidator {
public:
    /**
     * @note ThreadSafety: Thread-safe (pure function, see class note).
     * @brief Fully validates one BarrierBatchTable section.
     * @param tableBytes The complete section byte range, table header included. Must be
     *        8-byte aligned at its base.
     * @return std::expected<CommandStreamBarrierBatchTableView,
     *         CommandStreamBarrierBatchTableValidationFailure> A view on success; the
     *         first failure (category + byte offset) otherwise. Errors are values, not
     *         exceptions, so the FFM boundary needs no unwinding (§6.4).
     * @warning MemoryOwnership: tableBytes is BORROWED (§6.3); the returned view
     *          aliases it and the provider must keep it alive and unmodified while the
     *          view is in use.
     */
    [[nodiscard]] static std::expected<CommandStreamBarrierBatchTableView,
                                       CommandStreamBarrierBatchTableValidationFailure>
    validate(std::span<const std::byte> tableBytes) noexcept;
};

} // namespace barrieww
