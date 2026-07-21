#pragma once

#include <cstddef>
#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief One 32-byte ImageViewHandleTable entry (schema v0.1). The entry describes a
 *        static non-array view of an ImageHandleTable source slot. It is pure BECS data:
 *        source-slot existence and backend compatibility are verified when native
 *        materialization joins both tables. Layout is the wire format, pinned by the
 *        static_asserts below (ADR-0002 D1).
 */
struct CommandStreamImageViewHandleTableEntry {
    std::uint32_t imageSlot;
    std::uint32_t imageViewKindValue;
    std::uint32_t formatValue;
    std::uint32_t aspectMaskValue;
    std::uint32_t baseMipLevel;
    std::uint32_t mipLevelCount;
    std::uint32_t baseArrayLayer;
    std::uint32_t arrayLayerCount;
};

static_assert(sizeof(CommandStreamImageViewHandleTableEntry) == 32u,
              "ImageViewHandleTable entries must be exactly 32 bytes (schema v0.1)");
static_assert(offsetof(CommandStreamImageViewHandleTableEntry, imageViewKindValue) == 4u);
static_assert(offsetof(CommandStreamImageViewHandleTableEntry, aspectMaskValue) == 12u);
static_assert(offsetof(CommandStreamImageViewHandleTableEntry, mipLevelCount) == 20u);
static_assert(offsetof(CommandStreamImageViewHandleTableEntry, arrayLayerCount) == 28u);

} // namespace barrieww
