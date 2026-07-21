#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumerations and the constexpr predicate below are stateless and
 *       fully thread-safe.
 * @brief Image dimensionality of the ImageHandleTable schema (v0.1). The loader maps
 *        the kind onto the concrete API's image type (Vulkan: VkImageType).
 */
enum class CommandStreamImageKind : std::uint32_t {
    OneDimensional = 1,
    TwoDimensional = 2,
    ThreeDimensional = 3,
};

/**
 * @note ThreadSafety: Thread-safe (pure constant expression).
 * @brief Whether the raw value names an assigned image kind.
 * @param rawImageKindValue The raw kind value read from a table entry.
 * @return bool True when the value is assigned in the v0.1 schema.
 */
[[nodiscard]] constexpr bool
isAssignedCommandStreamImageKind(std::uint32_t rawImageKindValue) noexcept {
    return rawImageKindValue >= 1u && rawImageKindValue <= 3u;
}

} // namespace barrieww
