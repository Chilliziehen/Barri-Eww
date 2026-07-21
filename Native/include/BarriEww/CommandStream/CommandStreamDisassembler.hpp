#pragma once

#include <string>

#include "BarriEww/CommandStream/CommandStreamModuleView.hpp"
#include "BarriEww/CommandStream/CommandStreamView.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Stateless; both methods are pure functions of their input views
 *       and may be called concurrently.
 * @brief Renders validated BECS streams and modules as human-readable text for
 *        diagnostics (the ADR-0002 follow-up tooling). Input is deliberately restricted
 *        to VALIDATED views: invalid bytes cannot reach the disassembler, so it decodes
 *        without checks — to inspect a rejected artifact, read the validator's failure
 *        (category + byte offset) instead. Diagnostics only; never call on a hot path.
 */
class CommandStreamDisassembler {
public:
    /**
     * @note ThreadSafety: Thread-safe (pure function, see class note).
     * @brief Renders one lane stream: a header line, then one indented line per command
     *        with opcode name, raw opcode value, byte size and a payload hex dump.
     * @param streamView The validated lane stream to render.
     * @return std::string The multi-line, newline-terminated rendering (caller-owned).
     */
    [[nodiscard]] static std::string disassemble(const CommandStreamView& streamView);

    /**
     * @note ThreadSafety: Thread-safe (pure function, see class note).
     * @brief Renders one module container: a header line, the section directory, then
     *        every embedded lane stream via the lane overload.
     * @param moduleView The validated module to render.
     * @return std::string The multi-line, newline-terminated rendering (caller-owned).
     */
    [[nodiscard]] static std::string disassemble(const CommandStreamModuleView& moduleView);
};

} // namespace barrieww
