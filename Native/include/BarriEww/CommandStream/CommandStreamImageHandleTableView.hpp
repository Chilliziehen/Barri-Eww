#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "BarriEww/CommandStream/CommandStreamImageHandleTableEntry.hpp"

namespace barrieww {

class CommandStreamImageHandleTableValidator;

/**
 * @note ThreadSafety: Read-only view; safe to query concurrently from multiple
 *       threads. This blanket statement covers all accessors below (spec §2.6).
 * @brief Non-owning view over a VALIDATED ImageHandleTable section. Instances are
 *        created exclusively by CommandStreamImageHandleTableValidator (view existence
 *        proves schema validity), so entry access is unchecked. Consumed once by the
 *        loader when it materializes the dense native image array (slow path).
 * @warning MemoryOwnership: The view BORROWS the underlying section bytes (§6.3); the
 *          provider must keep them alive and unmodified for the lifetime of this view.
 */
class CommandStreamImageHandleTableView {
    friend class CommandStreamImageHandleTableValidator;

public:
    /** The number of entries (slots 0..entryCount-1) in the table (see class notes). */
    [[nodiscard]] std::uint32_t entryCount() const noexcept { return m_entryCount; }

    /**
     * @note ThreadSafety: Thread-safe (read-only, see class note).
     * @brief Decodes one entry. Precondition: slotIndex < entryCount().
     * @param slotIndex The slot whose entry to decode.
     * @return CommandStreamImageHandleTableEntry The decoded entry, by value.
     */
    [[nodiscard]] CommandStreamImageHandleTableEntry
    entry(std::uint32_t slotIndex) const noexcept {
        CommandStreamImageHandleTableEntry decodedEntry{};
        std::memcpy(&decodedEntry,
                    m_tableBytes.data() + 8u
                        + static_cast<std::size_t>(slotIndex)
                              * sizeof(CommandStreamImageHandleTableEntry),
                    sizeof decodedEntry);
        return decodedEntry;
    }

private:
    CommandStreamImageHandleTableView(std::span<const std::byte> tableBytes,
                                      std::uint32_t entryCount) noexcept
        : m_tableBytes(tableBytes), m_entryCount(entryCount) {}

    std::span<const std::byte> m_tableBytes;
    std::uint32_t m_entryCount;
};

} // namespace barrieww
