#include "BarriEww/CommandStream/CommandStreamBarrierBatchTableValidator.hpp"

#include <cstdint>
#include <cstring>

#include "BarriEww/CommandStream/CommandStreamHeader.hpp"
#include "BarriEww/CommandStream/CommandStreamImageAspect.hpp"
#include "BarriEww/CommandStream/CommandStreamImageLayout.hpp"

namespace barrieww {

/*
 * BarrierBatchTable schema validation (v0.1). Algorithm principle: the batch directory
 * is fixed-stride and therefore directly checkable; each batch then claims one barrier
 * record region whose size is fully determined by its three counts, so region validity
 * is a bounds-and-alignment equation. Regions MAY overlap between batches (record
 * sharing is legitimate deduplication), so no overlap check is performed — unlike the
 * module directory, where sections are distinct owned ranges.
 *
 * Pseudocode (complete semantics):
 *   validate(tableBytes):
 *     if length(tableBytes) < tableHeaderSize            -> TableTooSmall
 *     if baseAddress(tableBytes) % 8 != 0                -> MisalignedTableBase
 *     (batchCount, headerReservedFlags) <- decode header
 *     if headerReservedFlags != 0                        -> NonZeroReservedFlags
 *     directoryEndOffset <- 8 + batchCount * 24
 *     if directoryEndOffset > length(tableBytes)         -> DirectoryOutOfBounds
 *     for batchSlot in 0 .. batchCount - 1:
 *       batch <- decode directory record
 *       if batch.reservedFlags != 0                      -> NonZeroReservedFlags
 *       regionByteSize <- batch.globalBarrierCount * 32
 *                         + batch.bufferBarrierCount * 56
 *                         + batch.imageBarrierCount * 64
 *       if batch.barriersByteOffset % 8 != 0             -> MisalignedBarrierRegion
 *       if region outside [directoryEndOffset, length]   -> BarrierRegionOutOfBounds
 *       for each global record:  stage masks non-zero    else ZeroStageMask
 *       for each buffer record:  reservedFlags zero      else NonZeroReservedFlags
 *                                stage masks non-zero    else ZeroStageMask
 *       for each image record (after buffer records):
 *         stage masks non-zero                           else ZeroStageMask
 *         old/new layout values assigned                 else UnknownImageLayout
 *         aspect mask usable                             else UnusableImageAspectMask
 *         mip/layer counts non-zero (sentinel allowed)   else EmptyImageSubresourceRange
 *     return view(tableBytes, batchCount)
 */
std::expected<CommandStreamBarrierBatchTableView,
              CommandStreamBarrierBatchTableValidationFailure>
CommandStreamBarrierBatchTableValidator::validate(
    std::span<const std::byte> tableBytes) noexcept {
    using enum CommandStreamBarrierBatchTableValidationError;

    constexpr std::size_t tableHeaderByteSize = 8u;
    constexpr std::size_t batchRecordByteSize = sizeof(CommandStreamBarrierBatchRecord);
    constexpr std::size_t globalRecordByteSize = sizeof(CommandStreamGlobalBarrierRecord);
    constexpr std::size_t bufferRecordByteSize = sizeof(CommandStreamBufferBarrierRecord);
    constexpr std::size_t imageRecordByteSize = sizeof(CommandStreamImageBarrierRecord);

    const auto fail = [](CommandStreamBarrierBatchTableValidationError error,
                         std::uint64_t byteOffset) {
        return std::unexpected(
            CommandStreamBarrierBatchTableValidationFailure{error, byteOffset});
    };

    if (tableBytes.size() < tableHeaderByteSize) {
        return fail(TableTooSmall, 0u);
    }
    if (reinterpret_cast<std::uintptr_t>(tableBytes.data())
            % CommandStreamHeader::s_commandAlignment != 0u) {
        return fail(MisalignedTableBase, 0u);
    }

    std::uint32_t batchCount = 0;
    std::uint32_t headerReservedFlags = 0;
    std::memcpy(&batchCount, tableBytes.data(), sizeof batchCount);
    std::memcpy(&headerReservedFlags, tableBytes.data() + 4u, sizeof headerReservedFlags);
    if (headerReservedFlags != 0u) {
        return fail(NonZeroReservedFlags, 4u);
    }

    const std::uint64_t directoryEndOffset =
        tableHeaderByteSize + static_cast<std::uint64_t>(batchCount) * batchRecordByteSize;
    if (directoryEndOffset > tableBytes.size()) {
        return fail(DirectoryOutOfBounds, 0u);
    }

    for (std::uint32_t batchSlot = 0; batchSlot < batchCount; ++batchSlot) {
        const std::uint64_t directoryRecordOffset =
            tableHeaderByteSize + static_cast<std::uint64_t>(batchSlot) * batchRecordByteSize;
        CommandStreamBarrierBatchRecord batchRecord{};
        std::memcpy(&batchRecord, tableBytes.data() + directoryRecordOffset,
                    batchRecordByteSize);

        if (batchRecord.reservedFlags != 0u) {
            return fail(NonZeroReservedFlags, directoryRecordOffset);
        }
        const std::uint64_t regionByteSize =
            static_cast<std::uint64_t>(batchRecord.globalBarrierCount) * globalRecordByteSize
            + static_cast<std::uint64_t>(batchRecord.bufferBarrierCount)
                  * bufferRecordByteSize
            + static_cast<std::uint64_t>(batchRecord.imageBarrierCount) * imageRecordByteSize;
        if (batchRecord.barriersByteOffset % CommandStreamHeader::s_commandAlignment != 0u) {
            return fail(MisalignedBarrierRegion, directoryRecordOffset);
        }
        if (batchRecord.barriersByteOffset < directoryEndOffset
            || batchRecord.barriersByteOffset > tableBytes.size()
            || regionByteSize > tableBytes.size() - batchRecord.barriersByteOffset) {
            return fail(BarrierRegionOutOfBounds, directoryRecordOffset);
        }

        for (std::uint32_t barrierIndex = 0; barrierIndex < batchRecord.globalBarrierCount;
             ++barrierIndex) {
            const std::uint64_t recordOffset =
                batchRecord.barriersByteOffset
                + static_cast<std::uint64_t>(barrierIndex) * globalRecordByteSize;
            CommandStreamGlobalBarrierRecord globalRecord{};
            std::memcpy(&globalRecord, tableBytes.data() + recordOffset, globalRecordByteSize);
            if (globalRecord.sourceStageMask == 0u
                || globalRecord.destinationStageMask == 0u) {
                return fail(ZeroStageMask, recordOffset);
            }
        }
        for (std::uint32_t barrierIndex = 0; barrierIndex < batchRecord.bufferBarrierCount;
             ++barrierIndex) {
            const std::uint64_t recordOffset =
                batchRecord.barriersByteOffset
                + static_cast<std::uint64_t>(batchRecord.globalBarrierCount)
                      * globalRecordByteSize
                + static_cast<std::uint64_t>(barrierIndex) * bufferRecordByteSize;
            CommandStreamBufferBarrierRecord bufferRecord{};
            std::memcpy(&bufferRecord, tableBytes.data() + recordOffset, bufferRecordByteSize);
            if (bufferRecord.reservedFlags != 0u) {
                return fail(NonZeroReservedFlags, recordOffset);
            }
            if (bufferRecord.sourceStageMask == 0u
                || bufferRecord.destinationStageMask == 0u) {
                return fail(ZeroStageMask, recordOffset);
            }
        }
        for (std::uint32_t barrierIndex = 0; barrierIndex < batchRecord.imageBarrierCount;
             ++barrierIndex) {
            const std::uint64_t recordOffset =
                batchRecord.barriersByteOffset
                + static_cast<std::uint64_t>(batchRecord.globalBarrierCount)
                      * globalRecordByteSize
                + static_cast<std::uint64_t>(batchRecord.bufferBarrierCount)
                      * bufferRecordByteSize
                + static_cast<std::uint64_t>(barrierIndex) * imageRecordByteSize;
            CommandStreamImageBarrierRecord imageRecord{};
            std::memcpy(&imageRecord, tableBytes.data() + recordOffset, imageRecordByteSize);
            if (imageRecord.sourceStageMask == 0u
                || imageRecord.destinationStageMask == 0u) {
                return fail(ZeroStageMask, recordOffset);
            }
            if (!isAssignedCommandStreamImageLayout(imageRecord.oldLayoutValue)
                || !isAssignedCommandStreamImageLayout(imageRecord.newLayoutValue)) {
                return fail(UnknownImageLayout, recordOffset);
            }
            if (!isUsableCommandStreamImageAspectMask(imageRecord.aspectMaskValue)) {
                return fail(UnusableImageAspectMask, recordOffset);
            }
            if (imageRecord.mipLevelCount == 0u || imageRecord.arrayLayerCount == 0u) {
                return fail(EmptyImageSubresourceRange, recordOffset);
            }
        }
    }

    return CommandStreamBarrierBatchTableView{
        tableBytes, batchCount};
}

} // namespace barrieww
