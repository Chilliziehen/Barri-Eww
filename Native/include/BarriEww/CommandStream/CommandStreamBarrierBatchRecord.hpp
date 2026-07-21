#pragma once

#include <cstddef>
#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief One 24-byte BarrierBatchTable directory record (schema v0.1). A batch is the
 *        compile-time barrier pass's output for one ExecuteBarrierBatch site: its
 *        barrier records live at barriersByteOffset (relative to the table base) —
 *        global records first, buffer records after them, image records last. Batches
 *        may share record regions (deduplication is legitimate). Layout is the wire
 *        format, pinned by the static_asserts below (ADR-0002 D1).
 */
struct CommandStreamBarrierBatchRecord {
    std::uint32_t globalBarrierCount;
    std::uint32_t bufferBarrierCount;
    std::uint32_t imageBarrierCount;
    std::uint32_t reservedFlags;
    std::uint64_t barriersByteOffset;
};

static_assert(sizeof(CommandStreamBarrierBatchRecord) == 24u,
              "BarrierBatchTable directory records must be exactly 24 bytes");
static_assert(offsetof(CommandStreamBarrierBatchRecord, imageBarrierCount) == 8u);
static_assert(offsetof(CommandStreamBarrierBatchRecord, reservedFlags) == 12u);
static_assert(offsetof(CommandStreamBarrierBatchRecord, barriersByteOffset) == 16u);

} // namespace barrieww
