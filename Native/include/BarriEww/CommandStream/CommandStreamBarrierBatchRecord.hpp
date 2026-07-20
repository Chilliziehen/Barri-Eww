#pragma once

#include <cstddef>
#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief One 16-byte BarrierBatchTable directory record (schema v0.1). A batch is the
 *        compile-time barrier pass's output for one ExecuteBarrierBatch site: its
 *        barrier records live at barriersByteOffset (relative to the table base),
 *        global records first, buffer records immediately after. Batches may share
 *        record regions (deduplication is legitimate). Layout is the wire format,
 *        pinned by the static_asserts below (ADR-0002 D1).
 */
struct CommandStreamBarrierBatchRecord {
    std::uint32_t globalBarrierCount;
    std::uint32_t bufferBarrierCount;
    std::uint64_t barriersByteOffset;
};

static_assert(sizeof(CommandStreamBarrierBatchRecord) == 16u,
              "BarrierBatchTable directory records must be exactly 16 bytes");
static_assert(offsetof(CommandStreamBarrierBatchRecord, bufferBarrierCount) == 4u);
static_assert(offsetof(CommandStreamBarrierBatchRecord, barriersByteOffset) == 8u);

} // namespace barrieww
