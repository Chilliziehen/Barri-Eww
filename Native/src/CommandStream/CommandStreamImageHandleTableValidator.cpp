#include "BarriEww/CommandStream/CommandStreamImageHandleTableValidator.hpp"

#include <cstdint>
#include <cstring>

#include "BarriEww/CommandStream/CommandStreamHeader.hpp"
#include "BarriEww/CommandStream/CommandStreamImageFormat.hpp"
#include "BarriEww/CommandStream/CommandStreamImageKind.hpp"
#include "BarriEww/CommandStream/CommandStreamImageUsage.hpp"

namespace barrieww {

/*
 * ImageHandleTable schema validation (v0.1). Algorithm principle: fixed-stride record
 * array, so validity is an exact size equation (8 + 40 * entryCount == section size)
 * plus per-entry field rules. Unlike the buffer table, the descriptive fields must be
 * valid for created AND imported entries (the host may verify a bound image against
 * them); only the non-empty usage rule is created-specific.
 *
 * Pseudocode (complete semantics):
 *   validate(tableBytes):
 *     if length < tableHeaderSize                        -> TableTooSmall
 *     if baseAddress % 8 != 0                            -> MisalignedTableBase
 *     (entryCount, headerReservedFlags) <- decode header
 *     if headerReservedFlags != 0                        -> NonZeroReservedFlags
 *     if length != 8 + entryCount * 40                   -> TableSizeMismatch
 *     for slotIndex in 0 .. entryCount - 1:
 *       entry <- decode 40-byte entry
 *       if kind unassigned                               -> UnknownImageKind
 *       if format unassigned                             -> UnknownImageFormat
 *       if samples not in {1, 2, 4, 8}                   -> UnknownSampleCount
 *       if any of width/height/depth/mips/layers == 0
 *          or (kind == ThreeDimensional and layers != 1) -> InvalidImageDimensions
 *       if usage mask has unassigned bits                -> UnknownUsageFlags
 *       if created (importIdentifier == 0) and usage == 0
 *                                                        -> InvalidCreatedImageDescription
 *     return view(tableBytes, entryCount)
 */
std::expected<CommandStreamImageHandleTableView, CommandStreamHandleTableValidationFailure>
CommandStreamImageHandleTableValidator::validate(
    std::span<const std::byte> tableBytes) noexcept {
    using enum CommandStreamHandleTableValidationError;

    constexpr std::size_t tableHeaderByteSize = 8u;
    constexpr std::size_t entryByteSize = sizeof(CommandStreamImageHandleTableEntry);

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
        CommandStreamImageHandleTableEntry entry{};
        std::memcpy(&entry, tableBytes.data() + entryOffset, entryByteSize);

        if (!isAssignedCommandStreamImageKind(entry.imageKindValue)) {
            return fail(UnknownImageKind, entryOffset);
        }
        if (!isAssignedCommandStreamImageFormat(entry.formatValue)) {
            return fail(UnknownImageFormat, entryOffset);
        }
        const bool isPowerOfTwoSampleCount =
            entry.sampleCountValue == 1u || entry.sampleCountValue == 2u
            || entry.sampleCountValue == 4u || entry.sampleCountValue == 8u;
        if (!isPowerOfTwoSampleCount) {
            return fail(UnknownSampleCount, entryOffset);
        }
        const bool isThreeDimensional =
            entry.imageKindValue
            == static_cast<std::uint32_t>(CommandStreamImageKind::ThreeDimensional);
        if (entry.width == 0u || entry.height == 0u || entry.depth == 0u
            || entry.mipLevelCount == 0u || entry.arrayLayerCount == 0u
            || (isThreeDimensional && entry.arrayLayerCount != 1u)) {
            return fail(InvalidImageDimensions, entryOffset);
        }
        if (!isKnownCommandStreamImageUsageMask(entry.usageFlags)) {
            return fail(UnknownUsageFlags, entryOffset);
        }
        if (!entry.isImported() && entry.usageFlags == 0u) {
            return fail(InvalidCreatedImageDescription, entryOffset);
        }
    }

    return CommandStreamImageHandleTableView{tableBytes, entryCount};
}

} // namespace barrieww
