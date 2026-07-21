#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumerations and the constexpr predicate below are stateless and
 *       fully thread-safe.
 * @brief Backend-neutral memory placement of a created buffer (BufferHandleTable schema
 *        v0.1). The loader maps the kind onto concrete memory properties (Vulkan:
 *        VkMemoryPropertyFlags). None is reserved for IMPORTED entries, whose memory is
 *        owned by the host that provides the handle (§6.3 ownership stays with the
 *        provider).
 */
enum class CommandStreamBufferMemoryKind : std::uint32_t {
    /** No placement: the entry is imported; the provider owns the memory. */
    None = 0,
    /** Device-local memory; written through transfers or GPU work. */
    DeviceLocal = 1,
    /** Host-visible, persistently mapped for per-frame parameter writes (ADR-0001). */
    HostVisiblePersistentMapped = 2,
    /** Host-visible, cached, for GPU-to-host readback. */
    HostVisibleReadback = 3,
};

/**
 * @note ThreadSafety: Thread-safe (pure constant expression).
 * @brief Whether the raw value names an assigned memory kind (None included; context
 *        rules — None only on imported entries — are the validator's job).
 * @param rawMemoryKindValue The raw memory kind value read from a table entry.
 * @return bool True when the value is assigned in the v0.1 schema.
 */
[[nodiscard]] constexpr bool
isAssignedCommandStreamBufferMemoryKind(std::uint32_t rawMemoryKindValue) noexcept {
    return rawMemoryKindValue <= 3u;
}

} // namespace barrieww
