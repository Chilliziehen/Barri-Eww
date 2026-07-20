#pragma once

#include <cstddef>
#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief One 56-byte buffer memory barrier record (schema v0.1): synchronization2
 *        masks (see CommandStreamGlobalBarrierRecord) plus the buffer slot and byte
 *        range. byteCount of s_wholeByteCount mirrors Vulkan's VK_WHOLE_SIZE ("from
 *        byteOffset to the end of the buffer"). Layout pinned by the static_asserts
 *        below.
 */
struct CommandStreamBufferBarrierRecord {
    /** Sentinel byteCount meaning "from byteOffset to the end of the buffer". */
    static constexpr std::uint64_t s_wholeByteCount = 0xFFFFFFFFFFFFFFFFull;

    std::uint64_t sourceStageMask;
    std::uint64_t sourceAccessMask;
    std::uint64_t destinationStageMask;
    std::uint64_t destinationAccessMask;
    std::uint32_t bufferSlot;
    std::uint32_t reservedFlags;
    std::uint64_t byteOffset;
    std::uint64_t byteCount;
};

static_assert(sizeof(CommandStreamBufferBarrierRecord) == 56u,
              "Buffer barrier records must be exactly 56 bytes");
static_assert(offsetof(CommandStreamBufferBarrierRecord, bufferSlot) == 32u);
static_assert(offsetof(CommandStreamBufferBarrierRecord, reservedFlags) == 36u);
static_assert(offsetof(CommandStreamBufferBarrierRecord, byteOffset) == 40u);
static_assert(offsetof(CommandStreamBufferBarrierRecord, byteCount) == 48u);

} // namespace barrieww
