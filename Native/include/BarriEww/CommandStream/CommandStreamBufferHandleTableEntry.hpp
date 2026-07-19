#pragma once

#include <cstddef>
#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief One 24-byte BufferHandleTable entry (schema v0.1). The slot number is the
 *        entry's position in the table; streams reference buffers by that slot
 *        (ADR-0002 D2 slot indirection). Two mutually exclusive shapes share the
 *        layout:
 *        - CREATED (importIdentifier == 0): the loader creates the buffer from
 *          byteSize / usageFlags / memoryKindValue at load time.
 *        - IMPORTED (importIdentifier != 0): the host binds an existing buffer under
 *          that identifier at load time; memoryKindValue is None and the provider
 *          keeps ownership of handle and memory (§6.3).
 *        Layout is the wire format, pinned by the static_asserts below (ADR-0002 D1).
 */
struct CommandStreamBufferHandleTableEntry {
    std::uint64_t byteSize;
    std::uint32_t usageFlags;
    std::uint32_t memoryKindValue;
    std::uint32_t importIdentifier;
    std::uint32_t reservedFlags;

    /** Whether this entry is imported rather than loader-created (see struct notes). */
    [[nodiscard]] bool isImported() const noexcept { return importIdentifier != 0u; }
};

static_assert(sizeof(CommandStreamBufferHandleTableEntry) == 24u,
              "BufferHandleTable entries must be exactly 24 bytes (schema v0.1)");
static_assert(offsetof(CommandStreamBufferHandleTableEntry, usageFlags) == 8u);
static_assert(offsetof(CommandStreamBufferHandleTableEntry, memoryKindValue) == 12u);
static_assert(offsetof(CommandStreamBufferHandleTableEntry, importIdentifier) == 16u);
static_assert(offsetof(CommandStreamBufferHandleTableEntry, reservedFlags) == 20u);

} // namespace barrieww
