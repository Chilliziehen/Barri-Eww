#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumerations and the constexpr predicate below are stateless and
 *       fully thread-safe.
 * @brief Image-view dimensionality of the ImageViewHandleTable schema (v0.1). The
 *        numeric values currently mirror CommandStreamImageKind but remain distinct so
 *        later minor versions can add array and cube view types without changing image
 *        resource kinds.
 */
enum class CommandStreamImageViewKind : std::uint32_t {
    OneDimensional = 1,
    TwoDimensional = 2,
    ThreeDimensional = 3,
};

/**
 * @note ThreadSafety: Thread-safe (pure constant expression).
 * @brief Whether the raw value names an assigned image-view kind.
 * @param rawImageViewKindValue The raw kind value read from a table entry.
 * @return bool True when the value is assigned in the v0.1 schema.
 */
[[nodiscard]] constexpr bool
isAssignedCommandStreamImageViewKind(std::uint32_t rawImageViewKindValue) noexcept {
    return rawImageViewKindValue >= 1u && rawImageViewKindValue <= 3u;
}

} // namespace barrieww
