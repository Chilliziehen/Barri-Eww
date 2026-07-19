#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "BarriEww/CommandStream/CommandStreamBufferHandleTableEntry.hpp"

namespace barrieww {

class CommandStreamBufferHandleTableValidator;

/**
 * @note ThreadSafety: Read-only view; safe to query concurrently from multiple
 *       threads. This blanket statement covers all accessors below (spec §2.6).
 * @brief Non-owning view over a VALIDATED BufferHandleTable section. Instances are
 *        created exclusively by CommandStreamBufferHandleTableValidator (view existence
 *        proves schema validity, same invariant as the stream and module views), so
 *        entry access is unchecked. Consumed once by the loader when it materializes
 *        the dense native buffer array (slow path).
 * @warning MemoryOwnership: The view BORROWS the underlying section bytes (in
 *          production part of a Java-owned module MemorySegment, §6.3); the provider
 *          must keep them alive and unmodified for the lifetime of this view.
 */
class CommandStreamBufferHandleTableView {
    friend class CommandStreamBufferHandleTableValidator;

public:
    /** The number of entries (slots 0..entryCount-1) in the table (see class notes). */
    [[nodiscard]] std::uint32_t entryCount() const noexcept { return m_entryCount; }

    /**
     * @note ThreadSafety: Thread-safe (read-only, see class note).
     * @brief Decodes one entry. Precondition: slotIndex < entryCount() — the loader
     *        iterates the validated range, so the access is unchecked (T0[1] applies
     *        to the artifact design even though this runs on the slow path).
     * @param slotIndex The slot whose entry to decode.
     * @return CommandStreamBufferHandleTableEntry The decoded entry, by value.
     */
    [[nodiscard]] CommandStreamBufferHandleTableEntry
    entry(std::uint32_t slotIndex) const noexcept {
        CommandStreamBufferHandleTableEntry decodedEntry{};
        std::memcpy(&decodedEntry,
                    m_tableBytes.data() + 8u
                        + static_cast<std::size_t>(slotIndex)
                              * sizeof(CommandStreamBufferHandleTableEntry),
                    sizeof decodedEntry);
        return decodedEntry;
    }

private:
    CommandStreamBufferHandleTableView(std::span<const std::byte> tableBytes,
                                       std::uint32_t entryCount) noexcept
        : m_tableBytes(tableBytes), m_entryCount(entryCount) {}

    std::span<const std::byte> m_tableBytes;
    std::uint32_t m_entryCount;
};

} // namespace barrieww
