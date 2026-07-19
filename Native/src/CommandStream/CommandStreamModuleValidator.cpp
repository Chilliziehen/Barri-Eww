#include "BarriEww/CommandStream/CommandStreamModuleValidator.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamHeader.hpp"
#include "BarriEww/CommandStream/CommandStreamModuleHeader.hpp"
#include "BarriEww/CommandStream/CommandStreamModuleSectionEntry.hpp"
#include "BarriEww/CommandStream/CommandStreamModuleSectionType.hpp"
#include "BarriEww/CommandStream/CommandStreamValidator.hpp"

namespace barrieww {

/*
 * Module validation (ADR-0002 D2/D5). Algorithm principle: the directory is the single
 * source of truth for the module's shape, so validation is (a) a well-formed header,
 * (b) a directory whose entries individually stay inside the module and collectively
 * neither overlap nor repeat an identity, and (c) lane-stream bookkeeping that is
 * mutually consistent between the header, the directory and the embedded streams.
 * Overlap and duplicate detection sort the entries once instead of comparing all pairs.
 *
 * Pseudocode (complete semantics):
 *   validate(moduleBytes):
 *     if length(moduleBytes) < moduleHeaderSize        -> ModuleTooSmall
 *     if baseAddress(moduleBytes) % 8 != 0             -> MisalignedModuleBase
 *     header <- decode 32-byte module header
 *     if header.magicBytes != "BECM"                   -> InvalidModuleMagic
 *     if version pair unsupported                      -> UnsupportedModuleVersion
 *     if header.totalByteSize != length(moduleBytes)   -> ModuleSizeMismatch
 *     directoryEndOffset <- headerSize + sectionCount * entrySize
 *     if directoryEndOffset > totalByteSize            -> DirectoryOutOfBounds
 *     entries <- decode each 24-byte entry:
 *       reservedFlags != 0                             -> NonZeroReservedFlags
 *       type unassigned                                -> UnknownSectionType
 *       byteOffset % 8 != 0                            -> MisalignedSectionOffset
 *       range outside [directoryEndOffset, totalByteSize] -> SectionOutOfBounds
 *     sort copies of entries by byteOffset:
 *       previousEnd > nextOffset                       -> SectionOverlap
 *     sort copies of entries by (type, sectionIndex):
 *       adjacent equal identity                        -> DuplicateSection
 *     laneEntries <- entries with type LaneStream, sorted by sectionIndex
 *     if count(laneEntries) != header.laneStreamCount  -> LaneStreamCountMismatch
 *     for position, laneEntry in laneEntries:
 *       if laneEntry.sectionIndex != position          -> LaneIndexMismatch
 *       laneView <- CommandStreamValidator.validate(section bytes)
 *       on failure                                     -> LaneStreamInvalid (nested)
 *       if laneView.laneIndex != laneEntry.sectionIndex -> LaneIndexMismatch
 *       if laneView.graphHash != header.graphHash      -> GraphHashMismatch
 *     return view(moduleBytes, header, entries, laneViews)
 */
std::expected<CommandStreamModuleView, CommandStreamModuleValidationFailure>
CommandStreamModuleValidator::validate(std::span<const std::byte> moduleBytes) noexcept {
    using enum CommandStreamModuleValidationError;

    constexpr std::size_t moduleHeaderByteSize = sizeof(CommandStreamModuleHeader);
    constexpr std::size_t directoryEntryByteSize = sizeof(CommandStreamModuleSectionEntry);

    const auto fail = [](CommandStreamModuleValidationError error, std::uint64_t byteOffset) {
        return std::unexpected(
            CommandStreamModuleValidationFailure{error, byteOffset, std::nullopt});
    };

    if (moduleBytes.size() < moduleHeaderByteSize) {
        return fail(ModuleTooSmall, 0u);
    }
    if (reinterpret_cast<std::uintptr_t>(moduleBytes.data())
            % CommandStreamHeader::s_commandAlignment != 0u) {
        return fail(MisalignedModuleBase, 0u);
    }

    CommandStreamModuleHeader header{};
    std::memcpy(&header, moduleBytes.data(), moduleHeaderByteSize);

    if (std::memcmp(header.magicBytes, CommandStreamModuleHeader::s_expectedMagicBytes,
                    sizeof header.magicBytes) != 0) {
        return fail(InvalidModuleMagic, 0u);
    }
    if (header.versionMajor != CommandStreamModuleHeader::s_currentVersionMajor
        || header.versionMinor > CommandStreamModuleHeader::s_currentVersionMinor) {
        return fail(UnsupportedModuleVersion, 4u);
    }
    if (header.totalByteSize != moduleBytes.size()) {
        return fail(ModuleSizeMismatch, 16u);
    }

    const std::uint64_t directoryEndOffset =
        moduleHeaderByteSize
        + static_cast<std::uint64_t>(header.sectionCount) * directoryEntryByteSize;
    if (directoryEndOffset > header.totalByteSize) {
        return fail(DirectoryOutOfBounds, 8u);
    }

    // Decode and individually check every directory entry.
    std::vector<CommandStreamModuleSectionEntry> sectionEntries(header.sectionCount);
    for (std::uint32_t entryIndex = 0; entryIndex < header.sectionCount; ++entryIndex) {
        const std::uint64_t entryOffset =
            moduleHeaderByteSize + static_cast<std::uint64_t>(entryIndex) * directoryEntryByteSize;
        CommandStreamModuleSectionEntry& sectionEntry = sectionEntries[entryIndex];
        std::memcpy(&sectionEntry, moduleBytes.data() + entryOffset, directoryEntryByteSize);

        if (sectionEntry.reservedFlags != 0u) {
            return fail(NonZeroReservedFlags, entryOffset);
        }
        if (!isAssignedCommandStreamModuleSectionType(sectionEntry.sectionTypeValue)) {
            return fail(UnknownSectionType, entryOffset);
        }
        if (sectionEntry.byteOffset % CommandStreamHeader::s_commandAlignment != 0u) {
            return fail(MisalignedSectionOffset, entryOffset);
        }
        if (sectionEntry.byteOffset < directoryEndOffset
            || sectionEntry.byteOffset > header.totalByteSize
            || sectionEntry.byteSize > header.totalByteSize - sectionEntry.byteOffset) {
            return fail(SectionOutOfBounds, entryOffset);
        }
    }

    // Overlap: sort by start offset, then every range must end before the next begins.
    std::vector<std::uint32_t> entriesByOffset(header.sectionCount);
    for (std::uint32_t entryIndex = 0; entryIndex < header.sectionCount; ++entryIndex) {
        entriesByOffset[entryIndex] = entryIndex;
    }
    std::ranges::sort(entriesByOffset, [&](std::uint32_t left, std::uint32_t right) {
        return sectionEntries[left].byteOffset < sectionEntries[right].byteOffset;
    });
    for (std::size_t sortedPosition = 1; sortedPosition < entriesByOffset.size(); ++sortedPosition) {
        const CommandStreamModuleSectionEntry& previousEntry =
            sectionEntries[entriesByOffset[sortedPosition - 1]];
        const CommandStreamModuleSectionEntry& currentEntry =
            sectionEntries[entriesByOffset[sortedPosition]];
        if (previousEntry.byteOffset + previousEntry.byteSize > currentEntry.byteOffset) {
            return fail(SectionOverlap,
                        moduleHeaderByteSize
                            + static_cast<std::uint64_t>(entriesByOffset[sortedPosition])
                                  * directoryEntryByteSize);
        }
    }

    // Duplicates: sort by (type, index) identity and compare neighbours.
    std::vector<std::uint32_t> entriesByIdentity = entriesByOffset;
    std::ranges::sort(entriesByIdentity, [&](std::uint32_t left, std::uint32_t right) {
        const CommandStreamModuleSectionEntry& leftEntry = sectionEntries[left];
        const CommandStreamModuleSectionEntry& rightEntry = sectionEntries[right];
        return std::pair{leftEntry.sectionTypeValue, leftEntry.sectionIndex}
               < std::pair{rightEntry.sectionTypeValue, rightEntry.sectionIndex};
    });
    for (std::size_t sortedPosition = 1; sortedPosition < entriesByIdentity.size(); ++sortedPosition) {
        const CommandStreamModuleSectionEntry& previousEntry =
            sectionEntries[entriesByIdentity[sortedPosition - 1]];
        const CommandStreamModuleSectionEntry& currentEntry =
            sectionEntries[entriesByIdentity[sortedPosition]];
        if (previousEntry.sectionTypeValue == currentEntry.sectionTypeValue
            && previousEntry.sectionIndex == currentEntry.sectionIndex) {
            return fail(DuplicateSection,
                        moduleHeaderByteSize
                            + static_cast<std::uint64_t>(entriesByIdentity[sortedPosition])
                                  * directoryEntryByteSize);
        }
    }

    // Lane-stream bookkeeping: exactly laneStreamCount streams with indices 0..N-1,
    // each internally valid and consistent with the module header.
    std::vector<const CommandStreamModuleSectionEntry*> laneEntries;
    for (const CommandStreamModuleSectionEntry& sectionEntry : sectionEntries) {
        if (sectionEntry.sectionTypeValue
            == static_cast<std::uint16_t>(CommandStreamModuleSectionType::LaneStream)) {
            laneEntries.push_back(&sectionEntry);
        }
    }
    if (laneEntries.size() != header.laneStreamCount) {
        return fail(LaneStreamCountMismatch, 12u);
    }
    std::ranges::sort(laneEntries,
                      [](const CommandStreamModuleSectionEntry* left,
                         const CommandStreamModuleSectionEntry* right) {
                          return left->sectionIndex < right->sectionIndex;
                      });

    std::vector<CommandStreamView> laneStreamViews;
    laneStreamViews.reserve(laneEntries.size());
    for (std::size_t lanePosition = 0; lanePosition < laneEntries.size(); ++lanePosition) {
        const CommandStreamModuleSectionEntry& laneEntry = *laneEntries[lanePosition];
        if (laneEntry.sectionIndex != lanePosition) {
            return fail(LaneIndexMismatch, laneEntry.byteOffset);
        }

        const std::span<const std::byte> laneStreamBytes =
            moduleBytes.subspan(static_cast<std::size_t>(laneEntry.byteOffset),
                                static_cast<std::size_t>(laneEntry.byteSize));
        const auto laneValidationResult = CommandStreamValidator::validate(laneStreamBytes);
        if (!laneValidationResult.has_value()) {
            return std::unexpected(CommandStreamModuleValidationFailure{
                LaneStreamInvalid, laneEntry.byteOffset, laneValidationResult.error()});
        }
        if (laneValidationResult->laneIndex() != laneEntry.sectionIndex) {
            return fail(LaneIndexMismatch, laneEntry.byteOffset);
        }
        if (laneValidationResult->graphHash() != header.graphHash) {
            return fail(GraphHashMismatch, laneEntry.byteOffset);
        }
        laneStreamViews.push_back(*laneValidationResult);
    }

    return CommandStreamModuleView{moduleBytes, header, std::move(sectionEntries),
                                   std::move(laneStreamViews)};
}

} // namespace barrieww
