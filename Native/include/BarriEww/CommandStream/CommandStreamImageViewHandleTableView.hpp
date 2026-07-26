#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "BarriEww/CommandStream/CommandStreamImageViewHandleTableEntry.hpp"

namespace barrieww {

class CommandStreamImageViewHandleTableValidator;

/**
 * @note ThreadSafety: Read-only view; safe to query concurrently from multiple
 *       threads. This blanket statement covers all accessors below (spec §2.6).
 * @brief Non-owning view over a VALIDATED ImageViewHandleTable section. Instances are
 *        created exclusively by CommandStreamImageViewHandleTableValidator (view
 *        existence proves self-table schema validity). The native loader consumes this
 *        once with a VulkanImageTable to materialize dense native image-view slots.
 * @warning MemoryOwnership: The view BORROWS the underlying section bytes (§6.3); the
 *          provider must keep them alive and unmodified for the lifetime of this view.
 */
class CommandStreamImageViewHandleTableView {
    friend class CommandStreamImageViewHandleTableValidator;

public:
    /** The number of entries (slots 0..entryCount-1) in the table (see class notes). */
    [[nodiscard]] std::uint32_t entryCount() const noexcept { return m_entryCount; }

    /**
     * @note ThreadSafety: Thread-safe (read-only, see class note).
     * @brief Decodes one entry. Precondition: slotIndex < entryCount().
     * @param slotIndex The slot whose entry to decode.
     * @return CommandStreamImageViewHandleTableEntry The decoded entry, by value.
     */
    [[nodiscard]] CommandStreamImageViewHandleTableEntry
    entry(std::uint32_t slotIndex) const noexcept {
        CommandStreamImageViewHandleTableEntry decodedEntry{};
        std::memcpy(&decodedEntry,
                    m_tableBytes.data() + 8u
                        + static_cast<std::size_t>(slotIndex)
                              * sizeof(CommandStreamImageViewHandleTableEntry),
                    sizeof decodedEntry);
        return decodedEntry;
    }

private:
    CommandStreamImageViewHandleTableView(std::span<const std::byte> tableBytes,
                                          std::uint32_t entryCount) noexcept
        : m_tableBytes(tableBytes), m_entryCount(entryCount) {}

    std::span<const std::byte> m_tableBytes;
    std::uint32_t m_entryCount;
};

} // namespace barrieww
