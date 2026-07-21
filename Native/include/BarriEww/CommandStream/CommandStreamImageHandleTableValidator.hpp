#pragma once

#include <cstddef>
#include <expected>
#include <span>

#include "BarriEww/CommandStream/CommandStreamHandleTableValidationFailure.hpp"
#include "BarriEww/CommandStream/CommandStreamImageHandleTableView.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Stateless; validate() is a pure function of its input bytes and
 *       may be called concurrently on distinct or identical byte ranges.
 * @brief Load-time schema validator for ImageHandleTable sections (schema v0.1): an
 *        8-byte header (entryCount, zero reserved flags) followed by fixed 40-byte
 *        entries. Field rules apply to created AND imported entries alike (the host
 *        may verify a bound image against the descriptive fields); created entries
 *        additionally need a non-empty usage mask. Failures share the handle-table
 *        error space (§6.4).
 */
class CommandStreamImageHandleTableValidator {
public:
    /**
     * @note ThreadSafety: Thread-safe (pure function, see class note).
     * @brief Fully validates one ImageHandleTable section: header, exact size, assigned
     *        kind/format/sample count, usable dimensions and counts (3D images must
     *        have exactly one array layer), known usage bits, and the created/imported
     *        shape rules.
     * @param tableBytes The complete section byte range, table header included. Must be
     *        8-byte aligned at its base.
     * @return std::expected<CommandStreamImageHandleTableView,
     *         CommandStreamHandleTableValidationFailure> A view on success; the first
     *         failure (category + byte offset) otherwise.
     * @warning MemoryOwnership: tableBytes is BORROWED (§6.3); the returned view
     *          aliases it and the provider must keep it alive and unmodified while the
     *          view is in use.
     */
    [[nodiscard]] static std::expected<CommandStreamImageHandleTableView,
                                       CommandStreamHandleTableValidationFailure>
    validate(std::span<const std::byte> tableBytes) noexcept;
};

} // namespace barrieww
