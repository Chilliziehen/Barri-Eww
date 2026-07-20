#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "BarriEww/CommandStream/CommandStreamBarrierBatchRecord.hpp"
#include "BarriEww/CommandStream/CommandStreamBufferBarrierRecord.hpp"
#include "BarriEww/CommandStream/CommandStreamGlobalBarrierRecord.hpp"
#include "BarriEww/CommandStream/CommandStreamImageBarrierRecord.hpp"

namespace barrieww {

class CommandStreamBarrierBatchTableValidator;

/**
 * @note ThreadSafety: Read-only view; safe to query concurrently from multiple
 *       threads. This blanket statement covers all accessors below (spec §2.6).
 * @brief Non-owning view over a VALIDATED BarrierBatchTable section. Instances are
 *        created exclusively by CommandStreamBarrierBatchTableValidator (view
 *        existence proves schema validity), so all decoding below is unchecked.
 *        Consumed by the recorder when it translates ExecuteBarrierBatch commands
 *        (load path).
 * @warning MemoryOwnership: The view BORROWS the underlying section bytes (§6.3); the
 *          provider must keep them alive and unmodified for the lifetime of this view.
 */
class CommandStreamBarrierBatchTableView {
    friend class CommandStreamBarrierBatchTableValidator;

public:
    /** The number of batches (slots 0..batchCount-1) in the table (see class notes). */
    [[nodiscard]] std::uint32_t batchCount() const noexcept { return m_batchCount; }

    /** The directory record of one batch; precondition batchSlot < batchCount(). */
    [[nodiscard]] CommandStreamBarrierBatchRecord
    batch(std::uint32_t batchSlot) const noexcept {
        CommandStreamBarrierBatchRecord batchRecord{};
        std::memcpy(&batchRecord,
                    m_tableBytes.data() + 8u
                        + static_cast<std::size_t>(batchSlot)
                              * sizeof(CommandStreamBarrierBatchRecord),
                    sizeof batchRecord);
        return batchRecord;
    }

    /** One global barrier of a batch; precondition barrierIndex < globalBarrierCount. */
    [[nodiscard]] CommandStreamGlobalBarrierRecord
    globalBarrier(std::uint32_t batchSlot, std::uint32_t barrierIndex) const noexcept {
        const CommandStreamBarrierBatchRecord batchRecord = batch(batchSlot);
        CommandStreamGlobalBarrierRecord barrierRecord{};
        std::memcpy(&barrierRecord,
                    m_tableBytes.data() + batchRecord.barriersByteOffset
                        + static_cast<std::size_t>(barrierIndex)
                              * sizeof(CommandStreamGlobalBarrierRecord),
                    sizeof barrierRecord);
        return barrierRecord;
    }

    /** One buffer barrier of a batch; precondition barrierIndex < bufferBarrierCount. */
    [[nodiscard]] CommandStreamBufferBarrierRecord
    bufferBarrier(std::uint32_t batchSlot, std::uint32_t barrierIndex) const noexcept {
        const CommandStreamBarrierBatchRecord batchRecord = batch(batchSlot);
        CommandStreamBufferBarrierRecord barrierRecord{};
        std::memcpy(&barrierRecord,
                    m_tableBytes.data() + batchRecord.barriersByteOffset
                        + batchRecord.globalBarrierCount
                              * sizeof(CommandStreamGlobalBarrierRecord)
                        + static_cast<std::size_t>(barrierIndex)
                              * sizeof(CommandStreamBufferBarrierRecord),
                    sizeof barrierRecord);
        return barrierRecord;
    }

    /** One image barrier of a batch; precondition barrierIndex < imageBarrierCount. */
    [[nodiscard]] CommandStreamImageBarrierRecord
    imageBarrier(std::uint32_t batchSlot, std::uint32_t barrierIndex) const noexcept {
        const CommandStreamBarrierBatchRecord batchRecord = batch(batchSlot);
        CommandStreamImageBarrierRecord barrierRecord{};
        std::memcpy(&barrierRecord,
                    m_tableBytes.data() + batchRecord.barriersByteOffset
                        + batchRecord.globalBarrierCount
                              * sizeof(CommandStreamGlobalBarrierRecord)
                        + batchRecord.bufferBarrierCount
                              * sizeof(CommandStreamBufferBarrierRecord)
                        + static_cast<std::size_t>(barrierIndex)
                              * sizeof(CommandStreamImageBarrierRecord),
                    sizeof barrierRecord);
        return barrierRecord;
    }

private:
    CommandStreamBarrierBatchTableView(std::span<const std::byte> tableBytes,
                                       std::uint32_t batchCount) noexcept
        : m_tableBytes(tableBytes), m_batchCount(batchCount) {}

    std::span<const std::byte> m_tableBytes;
    std::uint32_t m_batchCount;
};

} // namespace barrieww
