#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumerations and the constexpr predicate below are stateless and
 *       fully thread-safe.
 * @brief Pipeline kinds of the PipelineHandleTable schema (v0.1). Compute is the only
 *        kind of the initial catalog; Graphics arrives with the rendering increments
 *        (its entries will need a richer description reached through the kind
 *        discriminator, additive per ADR-0002 D6).
 */
enum class CommandStreamPipelineKind : std::uint32_t {
    Compute = 1,
};

/**
 * @note ThreadSafety: Thread-safe (pure constant expression).
 * @brief Whether the raw value names an assigned pipeline kind.
 * @param rawPipelineKindValue The raw kind value read from a table entry.
 * @return bool True when the value is assigned in the v0.1 schema.
 */
[[nodiscard]] constexpr bool
isAssignedCommandStreamPipelineKind(std::uint32_t rawPipelineKindValue) noexcept {
    return rawPipelineKindValue == 1u;
}

} // namespace barrieww
