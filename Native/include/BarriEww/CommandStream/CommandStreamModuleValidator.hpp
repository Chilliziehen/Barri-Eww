#pragma once

#include <cstddef>
#include <expected>
#include <span>

#include "BarriEww/CommandStream/CommandStreamModuleValidationFailure.hpp"
#include "BarriEww/CommandStream/CommandStreamModuleView.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Stateless; validate() is a pure function of its input bytes and
 *       may be called concurrently on distinct or identical byte ranges.
 * @brief Load-time structural validator for BECS module containers (ADR-0002 D2/D5).
 *        Validates the header, the section directory (bounds, alignment, overlap,
 *        duplicates), the lane-stream bookkeeping (count, contiguous lane indices) and
 *        every embedded lane stream via CommandStreamValidator, including graphHash
 *        agreement. Runs once per module on the slow path; the returned view is the
 *        proof of validity that keeps replay zero-check (§8.4 trust model). Slot-range
 *        validation of stream payloads against handle tables is deferred to the
 *        replayer increment that decodes payloads (still load-time, per D5).
 */
class CommandStreamModuleValidator {
public:
    /**
     * @note ThreadSafety: Thread-safe (pure function, see class note).
     * @brief Fully validates one module container and materializes its directory and
     *        per-lane stream views.
     * @param moduleBytes The complete candidate module, header included. Must be 8-byte
     *        aligned at its base (ADR-0002 D1).
     * @return std::expected<CommandStreamModuleView, CommandStreamModuleValidationFailure>
     *         A module view on success; the first failure otherwise (with the nested
     *         lane-stream failure attached for LaneStreamInvalid). Errors are values,
     *         not exceptions, so the FFM boundary needs no unwinding (§6.4).
     * @warning MemoryOwnership: moduleBytes is BORROWED (in production a Java-owned
     *          MemorySegment, §6.3); the returned view and its lane views alias it and
     *          the provider must keep it alive and unmodified while any of them is in use.
     */
    [[nodiscard]] static std::expected<CommandStreamModuleView, CommandStreamModuleValidationFailure>
    validate(std::span<const std::byte> moduleBytes) noexcept;
};

} // namespace barrieww
