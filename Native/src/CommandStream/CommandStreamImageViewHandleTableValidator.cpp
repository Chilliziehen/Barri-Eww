#include "BarriEww/CommandStream/CommandStreamImageViewHandleTableValidator.hpp"

#include <cstdint>
#include <cstring>

#include "BarriEww/CommandStream/CommandStreamHeader.hpp"
#include "BarriEww/CommandStream/CommandStreamImageAspect.hpp"
#include "BarriEww/CommandStream/CommandStreamImageFormat.hpp"
#include "BarriEww/CommandStream/CommandStreamImageViewKind.hpp"

namespace barrieww {

/*
 * ImageViewHandleTable schema validation (v0.1). Algorithm principle: fixed-stride
 * records make structural validity an exact size equation (8 + 32 * entryCount ==
 * section size), followed only by fields knowable without the source ImageHandleTable.
 * Cross-table existence, source compatibility and Vulkan subresource limits deliberately
 * remain materialization checks.
 *
 * Pseudocode (complete semantics):
 *   validate(tableBytes):
 *     if length < tableHeaderSize                        -> TableTooSmall
 *     if baseAddress % 8 != 0                            -> MisalignedTableBase
 *     (entryCount, headerReservedFlags) <- decode header
 *     if headerReservedFlags != 0                        -> NonZeroReservedFlags
 *     if length != 8 + entryCount * 32                   -> TableSizeMismatch
 *     for slotIndex in 0 .. entryCount - 1:
 *       entry <- decode 32-byte entry
 *       if view kind unassigned                          -> UnknownImageViewKind
 *       if format unassigned                             -> UnknownImageFormat
 *       if aspect mask is not usable                     -> UnusableImageAspectMask
 *       if mip count is zero/remaining, layer count is not one,
 *          or (kind == ThreeDimensional and base layer != 0)
 *                                                        -> InvalidImageViewSubresourceCounts
 *     return view(tableBytes, entryCount)
 */
std::expected<CommandStreamImageViewHandleTableView,
              CommandStreamHandleTableValidationFailure>
CommandStreamImageViewHandleTableValidator::validate(
    std::span<const std::byte> tableBytes) noexcept {
    using enum CommandStreamHandleTableValidationError;

    constexpr std::size_t tableHeaderByteSize = 8u;
    constexpr std::size_t entryByteSize = sizeof(CommandStreamImageViewHandleTableEntry);

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
        CommandStreamImageViewHandleTableEntry entry{};
        std::memcpy(&entry, tableBytes.data() + entryOffset, entryByteSize);

        if (!isAssignedCommandStreamImageViewKind(entry.imageViewKindValue)) {
            return fail(UnknownImageViewKind, entryOffset);
        }
        if (!isAssignedCommandStreamImageFormat(entry.formatValue)) {
            return fail(UnknownImageFormat, entryOffset);
        }
        if (!isUsableCommandStreamImageAspectMask(entry.aspectMaskValue)) {
            return fail(UnusableImageAspectMask, entryOffset);
        }
        const bool isThreeDimensional =
            entry.imageViewKindValue
            == static_cast<std::uint32_t>(CommandStreamImageViewKind::ThreeDimensional);
        if (entry.mipLevelCount == 0u
            || entry.mipLevelCount == CommandStreamImageViewHandleTableEntry::s_remainingCount
            || entry.arrayLayerCount != 1u
            || (isThreeDimensional && entry.baseArrayLayer != 0u)) {
            return fail(InvalidImageViewSubresourceCounts, entryOffset);
        }
    }

    return CommandStreamImageViewHandleTableView{tableBytes, entryCount};
}

} // namespace barrieww
