#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "BarriEww/CommandStream/CommandStreamGraphicsPipelineRecord.hpp"

namespace barrieww {

class CommandStreamGraphicsPipelineTableValidator;

/**
 * @note ThreadSafety: Read-only view; safe to query concurrently from multiple
 *       threads. This blanket statement covers all accessors below (spec §2.6).
 * @brief Non-owning view over a VALIDATED GraphicsPipelineTable section. Instances are
 *        created exclusively by CommandStreamGraphicsPipelineTableValidator (view
 *        existence proves schema validity), so access is unchecked. Consumed by the
 *        load path when it materializes graphics pipelines against dynamic rendering.
 * @warning MemoryOwnership: The view BORROWS the underlying section bytes (§6.3); the
 *          provider must keep them alive and unmodified for the lifetime of this view.
 */
class CommandStreamGraphicsPipelineTableView {
    friend class CommandStreamGraphicsPipelineTableValidator;

public:
    /** The number of pipelines (slots 0..pipelineCount-1) in the table (see class notes). */
    [[nodiscard]] std::uint32_t pipelineCount() const noexcept { return m_pipelineCount; }

    /**
     * @note ThreadSafety: Thread-safe (read-only, see class note).
     * @brief Decodes one 80-byte pipeline record. Precondition: pipelineSlot < pipelineCount().
     * @param pipelineSlot The pipeline slot whose record to decode.
     * @return CommandStreamGraphicsPipelineRecord The decoded record, by value.
     */
    [[nodiscard]] CommandStreamGraphicsPipelineRecord
    pipelineRecord(std::uint32_t pipelineSlot) const noexcept {
        CommandStreamGraphicsPipelineRecord record{};
        std::memcpy(&record,
                    m_tableBytes.data() + 8u
                        + static_cast<std::size_t>(pipelineSlot)
                              * sizeof(CommandStreamGraphicsPipelineRecord),
                    sizeof record);
        return record;
    }

private:
    CommandStreamGraphicsPipelineTableView(std::span<const std::byte> tableBytes,
                                           std::uint32_t pipelineCount) noexcept
        : m_tableBytes(tableBytes), m_pipelineCount(pipelineCount) {}

    std::span<const std::byte> m_tableBytes;
    std::uint32_t m_pipelineCount;
};

} // namespace barrieww
