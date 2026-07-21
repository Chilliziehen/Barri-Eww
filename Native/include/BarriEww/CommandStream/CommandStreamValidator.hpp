#pragma once

#include <cstddef>
#include <expected>
#include <span>

#include "BarriEww/CommandStream/CommandStreamValidationFailure.hpp"
#include "BarriEww/CommandStream/CommandStreamView.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Stateless; validate() is a pure function of its input bytes and
 *       may be called concurrently on distinct or identical byte ranges.
 * @brief Load-time structural validator for BECS lane streams (ADR-0002 D5). Runs once
 *        per stream on the slow path; a successfully returned CommandStreamView is the
 *        proof of validity that lets replay run zero-check in release builds — the same
 *        trust model as bytecode passing the JVM verifier before classload (§8.4).
 */
class CommandStreamValidator {
public:
    /**
     * @note ThreadSafety: Thread-safe (pure function, see class note).
     * @brief Fully validates one lane stream: base alignment, header magic and version,
     *        command-size alignment and chaining, reserved flags, opcode assignment,
     *        and the commandCount / totalByteSize agreement (ADR-0002 D5 checklist).
     * @param streamBytes The complete candidate lane stream, header included. Must be
     *        8-byte aligned at its base (ADR-0002 D1).
     * @return std::expected<CommandStreamView, CommandStreamValidationFailure> A view on
     *         success; the first failure (category + byte offset) otherwise. Errors are
     *         values, not exceptions, so the FFM boundary needs no unwinding (§6.4).
     * @warning MemoryOwnership: streamBytes is BORROWED (in production a Java-owned
     *          MemorySegment, §6.3); the returned view aliases it and the provider must
     *          keep it alive and unmodified while either is in use.
     */
    [[nodiscard]] static std::expected<CommandStreamView, CommandStreamValidationFailure>
    validate(std::span<const std::byte> streamBytes) noexcept;
};

} // namespace barrieww
