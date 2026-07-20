#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "BarriEww/CommandStream/CommandStreamShaderModuleTableEntry.hpp"

namespace barrieww {

class CommandStreamShaderModuleTableValidator;

/**
 * @note ThreadSafety: Read-only view; safe to query concurrently from multiple
 *       threads. This blanket statement covers all accessors below (spec §2.6).
 * @brief Non-owning view over a VALIDATED ShaderModuleTable section. Instances are
 *        created exclusively by CommandStreamShaderModuleTableValidator (view existence
 *        proves schema validity — every blob is in bounds, aligned and starts with the
 *        SPIR-V magic), so access is unchecked. Consumed by pipeline materialization
 *        on the load path.
 * @warning MemoryOwnership: The view BORROWS the underlying section bytes (§6.3); the
 *          provider must keep them alive and unmodified for the lifetime of this view
 *          and of any blob span obtained from it.
 */
class CommandStreamShaderModuleTableView {
    friend class CommandStreamShaderModuleTableValidator;

public:
    /** The number of shader blobs (slots 0..entryCount-1) in the table (see class notes). */
    [[nodiscard]] std::uint32_t entryCount() const noexcept { return m_entryCount; }

    /**
     * @note ThreadSafety: Thread-safe (read-only, see class note).
     * @brief The SPIR-V blob of one slot. Precondition: slotIndex < entryCount().
     * @param slotIndex The shader module slot whose blob to reference.
     * @return std::span<const std::byte> The borrowed blob byte range (4-multiple size,
     *         8-aligned base within the table).
     */
    [[nodiscard]] std::span<const std::byte> blob(std::uint32_t slotIndex) const noexcept {
        CommandStreamShaderModuleTableEntry tableEntry{};
        std::memcpy(&tableEntry,
                    m_tableBytes.data() + 8u
                        + static_cast<std::size_t>(slotIndex)
                              * sizeof(CommandStreamShaderModuleTableEntry),
                    sizeof tableEntry);
        return m_tableBytes.subspan(static_cast<std::size_t>(tableEntry.blobByteOffset),
                                    static_cast<std::size_t>(tableEntry.blobByteSize));
    }

private:
    CommandStreamShaderModuleTableView(std::span<const std::byte> tableBytes,
                                       std::uint32_t entryCount) noexcept
        : m_tableBytes(tableBytes), m_entryCount(entryCount) {}

    std::span<const std::byte> m_tableBytes;
    std::uint32_t m_entryCount;
};

} // namespace barrieww
