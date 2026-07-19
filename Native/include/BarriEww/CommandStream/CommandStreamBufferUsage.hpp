#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumerations and the constexpr helpers below are stateless and
 *       fully thread-safe.
 * @brief Backend-neutral buffer usage bits of the BufferHandleTable schema (v0.1).
 *        Entries carry an OR-mask of these; the loader maps them onto the concrete
 *        API's usage flags (Vulkan: VkBufferUsageFlagBits) at creation time (T0[3]:
 *        the wire stays backend-neutral, each backend owns its mapping).
 */
enum class CommandStreamBufferUsage : std::uint32_t {
    TransferSource = 0x1,
    TransferDestination = 0x2,
    Vertex = 0x4,
    Index = 0x8,
    Uniform = 0x10,
    Storage = 0x20,
    Indirect = 0x40,
};

/** OR-mask of every usage bit assigned in the v0.1 schema. */
inline constexpr std::uint32_t g_allKnownCommandStreamBufferUsageFlags = 0x7Fu;

/**
 * @note ThreadSafety: Thread-safe (pure constant expression).
 * @brief Whether a raw usage mask only contains assigned bits. Unassigned bits are
 *        rejected at load: with exact-major / additive-minor versioning (ADR-0002 D6)
 *        an accepted artifact can only carry bits this build knows.
 * @param rawUsageFlags The raw usage mask read from a table entry.
 * @return bool True when every set bit is assigned.
 */
[[nodiscard]] constexpr bool
isKnownCommandStreamBufferUsageMask(std::uint32_t rawUsageFlags) noexcept {
    return (rawUsageFlags & ~g_allKnownCommandStreamBufferUsageFlags) == 0u;
}

} // namespace barrieww
