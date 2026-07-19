#include "BarriEww/CommandStream/CommandStreamBufferHandleTableValidator.hpp"

#include <cstdint>
#include <cstring>

#include "BarriEww/CommandStream/CommandStreamBufferMemoryKind.hpp"
#include "BarriEww/CommandStream/CommandStreamBufferUsage.hpp"
#include "BarriEww/CommandStream/CommandStreamHeader.hpp"

namespace barrieww {

/*
 * BufferHandleTable schema validation (v0.1). Algorithm principle: the table is a
 * fixed-stride record array, so validity is (a) an exact size equation
 * (8 + 24 * entryCount == section size) and (b) per-entry field rules. The shape rule
 * is the load-bearing one: importIdentifier discriminates CREATED entries (must fully
 * describe a creatable buffer) from IMPORTED entries (must not carry a memory kind,
 * because the provider owns the memory, §6.3).
 *
 * Pseudocode (complete semantics):
 *   validate(tableBytes):
 *     if length(tableBytes) < tableHeaderSize            -> TableTooSmall
 *     if baseAddress(tableBytes) % 8 != 0                -> MisalignedTableBase
 *     (entryCount, headerReservedFlags) <- decode header
 *     if headerReservedFlags != 0                        -> NonZeroReservedFlags
 *     if length != tableHeaderSize + entryCount * 24     -> TableSizeMismatch
 *     for slotIndex in 0 .. entryCount - 1:
 *       entry <- decode 24-byte entry at 8 + slotIndex * 24
 *       if entry.reservedFlags != 0                      -> NonZeroReservedFlags
 *       if usage mask has unassigned bits                -> UnknownUsageFlags
 *       if memory kind value unassigned                  -> UnknownMemoryKind
 *       if entry.importIdentifier == 0 (CREATED):
 *         if byteSize == 0 or usageFlags == 0 or kind == None
 *                                                        -> InvalidCreatedBufferDescription
 *       else (IMPORTED):
 *         if kind != None                                -> InvalidImportedBufferDescription
 *     return view(tableBytes, entryCount)
 */
std::expected<CommandStreamBufferHandleTableView, CommandStreamHandleTableValidationFailure>
CommandStreamBufferHandleTableValidator::validate(
    std::span<const std::byte> tableBytes) noexcept {
    using enum CommandStreamHandleTableValidationError;

    constexpr std::size_t tableHeaderByteSize = 8u;
    constexpr std::size_t entryByteSize = sizeof(CommandStreamBufferHandleTableEntry);

    const auto fail = [](CommandStreamHandleTableValidationError error,
                         std::uint64_t byteOffset) {
        return std::unexpected(CommandStreamHandleTableValidationFailure{error, byteOffset});
    };

    if (tableBytes.size() < tableHeaderByteSize) {
        return fail(TableTooSmall, 0u);
    }
    if (reinterpret_cast<std::uintptr_t>(tableBytes.data())
            % CommandStreamHeader::s_commandAlignment != 0u) {
        return fail(MisalignedTableBase, 0u);
    }

    std::uint32_t entryCount = 0;
    std::uint32_t headerReservedFlags = 0;
    std::memcpy(&entryCount, tableBytes.data(), sizeof entryCount);
    std::memcpy(&headerReservedFlags, tableBytes.data() + 4u, sizeof headerReservedFlags);

    if (headerReservedFlags != 0u) {
        return fail(NonZeroReservedFlags, 4u);
    }
    if (tableBytes.size()
        != tableHeaderByteSize + static_cast<std::uint64_t>(entryCount) * entryByteSize) {
        return fail(TableSizeMismatch, 0u);
    }

    for (std::uint32_t slotIndex = 0; slotIndex < entryCount; ++slotIndex) {
        const std::uint64_t entryOffset =
            tableHeaderByteSize + static_cast<std::uint64_t>(slotIndex) * entryByteSize;
        CommandStreamBufferHandleTableEntry entry{};
        std::memcpy(&entry, tableBytes.data() + entryOffset, entryByteSize);

        if (entry.reservedFlags != 0u) {
            return fail(NonZeroReservedFlags, entryOffset);
        }
        if (!isKnownCommandStreamBufferUsageMask(entry.usageFlags)) {
            return fail(UnknownUsageFlags, entryOffset);
        }
        if (!isAssignedCommandStreamBufferMemoryKind(entry.memoryKindValue)) {
            return fail(UnknownMemoryKind, entryOffset);
        }

        const bool isMemoryKindNone =
            entry.memoryKindValue
            == static_cast<std::uint32_t>(CommandStreamBufferMemoryKind::None);
        if (!entry.isImported()) {
            if (entry.byteSize == 0u || entry.usageFlags == 0u || isMemoryKindNone) {
                return fail(InvalidCreatedBufferDescription, entryOffset);
            }
        } else if (!isMemoryKindNone) {
            return fail(InvalidImportedBufferDescription, entryOffset);
        }
    }

    return CommandStreamBufferHandleTableView{tableBytes, entryCount};
}

} // namespace barrieww
