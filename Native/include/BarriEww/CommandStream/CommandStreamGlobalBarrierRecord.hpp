#pragma once

#include <cstddef>
#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief One 32-byte global memory barrier record (schema v0.1). Masks use
 *        synchronization2 semantics (64-bit, ADR-0002 semantic baseline); the sync2
 *        bit values below bit 32 equal the legacy sync1 bits by Vulkan's design, which
 *        is what lets a 1.2-era recorder map these onto vkCmdPipelineBarrier. Layout
 *        pinned by the static_asserts below.
 */
struct CommandStreamGlobalBarrierRecord {
    std::uint64_t sourceStageMask;
    std::uint64_t sourceAccessMask;
    std::uint64_t destinationStageMask;
    std::uint64_t destinationAccessMask;
};

static_assert(sizeof(CommandStreamGlobalBarrierRecord) == 32u,
              "Global barrier records must be exactly 32 bytes");
static_assert(offsetof(CommandStreamGlobalBarrierRecord, destinationAccessMask) == 24u);

} // namespace barrieww
