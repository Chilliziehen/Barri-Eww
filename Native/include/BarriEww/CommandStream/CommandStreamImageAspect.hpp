#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumerations and the constexpr predicate below are stateless and
 *       fully thread-safe.
 * @brief Backend-neutral image aspect bits (v0.1), used by image barrier records and
 *        image command payloads. The recorder maps them onto concrete API aspects
 *        (Vulkan: VkImageAspectFlagBits).
 */
enum class CommandStreamImageAspect : std::uint32_t {
    Color = 0x1,
    Depth = 0x2,
    Stencil = 0x4,
};

/** OR-mask of every image aspect bit assigned in the v0.1 schema. */
inline constexpr std::uint32_t g_allKnownCommandStreamImageAspectFlags = 0x7u;

/**
 * @note ThreadSafety: Thread-safe (pure constant expression).
 * @brief Whether a raw aspect mask is non-empty and only contains assigned bits.
 * @param rawAspectMask The raw aspect mask read from a record or payload.
 * @return bool True when the mask is usable.
 */
[[nodiscard]] constexpr bool
isUsableCommandStreamImageAspectMask(std::uint32_t rawAspectMask) noexcept {
    return rawAspectMask != 0u
           && (rawAspectMask & ~g_allKnownCommandStreamImageAspectFlags) == 0u;
}

} // namespace barrieww
