#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumerations and the constexpr predicate below are stateless and
 *       fully thread-safe.
 * @brief Backend-neutral image usage bits of the ImageHandleTable schema (v0.1).
 *        Entries carry an OR-mask of these; the loader maps them onto the concrete
 *        API's usage flags (Vulkan: VkImageUsageFlagBits).
 */
enum class CommandStreamImageUsage : std::uint32_t {
    TransferSource = 0x1,
    TransferDestination = 0x2,
    Sampled = 0x4,
    Storage = 0x8,
    ColorAttachment = 0x10,
    DepthStencilAttachment = 0x20,
};

/** OR-mask of every image usage bit assigned in the v0.1 schema. */
inline constexpr std::uint32_t g_allKnownCommandStreamImageUsageFlags = 0x3Fu;

/**
 * @note ThreadSafety: Thread-safe (pure constant expression).
 * @brief Whether a raw usage mask only contains assigned bits.
 * @param rawUsageFlags The raw usage mask read from a table entry.
 * @return bool True when every set bit is assigned.
 */
[[nodiscard]] constexpr bool
isKnownCommandStreamImageUsageMask(std::uint32_t rawUsageFlags) noexcept {
    return (rawUsageFlags & ~g_allKnownCommandStreamImageUsageFlags) == 0u;
}

} // namespace barrieww
