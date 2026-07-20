#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "BarriEww/CommandStream/CommandStreamPipelineHandleTableEntry.hpp"

namespace barrieww {

class CommandStreamPipelineHandleTableValidator;

/**
 * @note ThreadSafety: Read-only view; safe to query concurrently from multiple
 *       threads. This blanket statement covers all accessors below (spec §2.6).
 * @brief Non-owning view over a VALIDATED PipelineHandleTable section (validator-only
 *        construction, same invariant as every other view). Consumed by pipeline
 *        materialization on the load path. Cross-table references (shaderModuleSlot)
 *        are checked at materialization, where both tables are present.
 * @warning MemoryOwnership: The view BORROWS the underlying section bytes (§6.3); the
 *          provider must keep them alive and unmodified for the lifetime of this view.
 */
class CommandStreamPipelineHandleTableView {
    friend class CommandStreamPipelineHandleTableValidator;

public:
    /** The number of entries (slots 0..entryCount-1) in the table (see class notes). */
    [[nodiscard]] std::uint32_t entryCount() const noexcept { return m_entryCount; }

    /**
     * @note ThreadSafety: Thread-safe (read-only, see class note).
     * @brief Decodes one entry. Precondition: slotIndex < entryCount().
     * @param slotIndex The pipeline slot whose entry to decode.
     * @return CommandStreamPipelineHandleTableEntry The decoded entry, by value.
     */
    [[nodiscard]] CommandStreamPipelineHandleTableEntry
    entry(std::uint32_t slotIndex) const noexcept {
        CommandStreamPipelineHandleTableEntry decodedEntry{};
        std::memcpy(&decodedEntry,
                    m_tableBytes.data() + 8u
                        + static_cast<std::size_t>(slotIndex)
                              * sizeof(CommandStreamPipelineHandleTableEntry),
                    sizeof decodedEntry);
        return decodedEntry;
    }

private:
    CommandStreamPipelineHandleTableView(std::span<const std::byte> tableBytes,
                                         std::uint32_t entryCount) noexcept
        : m_tableBytes(tableBytes), m_entryCount(entryCount) {}

    std::span<const std::byte> m_tableBytes;
    std::uint32_t m_entryCount;
};

} // namespace barrieww
