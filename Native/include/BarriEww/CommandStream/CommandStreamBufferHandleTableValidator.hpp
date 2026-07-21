#pragma once

#include <cstddef>
#include <expected>
#include <span>

#include "BarriEww/CommandStream/CommandStreamBufferHandleTableView.hpp"
#include "BarriEww/CommandStream/CommandStreamHandleTableValidationFailure.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Stateless; validate() is a pure function of its input bytes and
 *       may be called concurrently on distinct or identical byte ranges.
 * @brief Load-time schema validator for BufferHandleTable sections (schema v0.1). The
 *        module validator proves the container (ADR-0002 D2); this proves the section's
 *        content, so the loader can create/bind every buffer without further checks.
 *        Runs once per table on the slow path (ADR-0002 D5 trust model).
 */
class CommandStreamBufferHandleTableValidator {
public:
    /**
     * @note ThreadSafety: Thread-safe (pure function, see class note).
     * @brief Fully validates one BufferHandleTable section: header, exact size,
     *        reserved flags, assigned memory kinds and usage bits, and the
     *        created/imported shape rules of each entry.
     * @param tableBytes The complete section byte range, table header included. Must be
     *        8-byte aligned at its base (guaranteed when it comes out of a validated
     *        module; checked again for standalone use).
     * @return std::expected<CommandStreamBufferHandleTableView,
     *         CommandStreamHandleTableValidationFailure> A view on success; the first
     *         failure (category + byte offset) otherwise. Errors are values, not
     *         exceptions, so the FFM boundary needs no unwinding (§6.4).
     * @warning MemoryOwnership: tableBytes is BORROWED (§6.3); the returned view
     *          aliases it and the provider must keep it alive and unmodified while the
     *          view is in use.
     */
    [[nodiscard]] static std::expected<CommandStreamBufferHandleTableView,
                                       CommandStreamHandleTableValidationFailure>
    validate(std::span<const std::byte> tableBytes) noexcept;
};

} // namespace barrieww
