#include "BarriEww/CommandStream/CommandStreamShaderModuleTableValidator.hpp"

#include <cstdint>
#include <cstring>

#include "BarriEww/CommandStream/CommandStreamHeader.hpp"

namespace barrieww {

/*
 * ShaderModuleTable schema validation (v0.1). Algorithm principle: the table is a
 * directory of byte ranges into its own blob region, so validity is (a) a directory
 * that fits and (b) per-entry range/alignment/SPIR-V-shape rules. Blob sharing between
 * entries is allowed (deduplication), so ranges are bounds-checked but not
 * overlap-checked.
 *
 * Pseudocode (complete semantics):
 *   validate(tableBytes):
 *     if length < tableHeaderSize                        -> TableTooSmall
 *     if baseAddress % 8 != 0                            -> MisalignedTableBase
 *     (entryCount, headerReservedFlags) <- decode header
 *     if headerReservedFlags != 0                        -> NonZeroReservedFlags
 *     directoryEndOffset <- 8 + entryCount * 16
 *     if directoryEndOffset > length                     -> TableSizeMismatch
 *     for slotIndex in 0 .. entryCount - 1:
 *       entry <- decode 16-byte entry at 8 + slotIndex * 16
 *       if entry.blobByteOffset % 8 != 0                 -> MisalignedShaderBlob
 *       if range outside [directoryEndOffset, length]    -> ShaderBlobOutOfBounds
 *       if size < 20 or size % 4 != 0                    -> InvalidShaderBlobByteSize
 *       if first word != 0x07230203                      -> InvalidShaderBlobMagic
 *     return view(tableBytes, entryCount)
 */
std::expected<CommandStreamShaderModuleTableView, CommandStreamHandleTableValidationFailure>
CommandStreamShaderModuleTableValidator::validate(
    std::span<const std::byte> tableBytes) noexcept {
    using enum CommandStreamHandleTableValidationError;

    constexpr std::size_t tableHeaderByteSize = 8u;
    constexpr std::size_t entryByteSize = sizeof(CommandStreamShaderModuleTableEntry);
    constexpr std::uint32_t spirvMagicWord = 0x07230203u;
    // Magic, version, generator, bound and schema words form the minimal SPIR-V header.
    constexpr std::uint64_t minimumSpirvByteSize = 20u;

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

    const std::uint64_t directoryEndOffset =
        tableHeaderByteSize + static_cast<std::uint64_t>(entryCount) * entryByteSize;
    if (directoryEndOffset > tableBytes.size()) {
        return fail(TableSizeMismatch, 0u);
    }

    for (std::uint32_t slotIndex = 0; slotIndex < entryCount; ++slotIndex) {
        const std::uint64_t entryOffset =
            tableHeaderByteSize + static_cast<std::uint64_t>(slotIndex) * entryByteSize;
        CommandStreamShaderModuleTableEntry tableEntry{};
        std::memcpy(&tableEntry, tableBytes.data() + entryOffset, entryByteSize);

        if (tableEntry.blobByteOffset % CommandStreamHeader::s_commandAlignment != 0u) {
            return fail(MisalignedShaderBlob, entryOffset);
        }
        if (tableEntry.blobByteOffset < directoryEndOffset
            || tableEntry.blobByteOffset > tableBytes.size()
            || tableEntry.blobByteSize > tableBytes.size() - tableEntry.blobByteOffset) {
            return fail(ShaderBlobOutOfBounds, entryOffset);
        }
        if (tableEntry.blobByteSize < minimumSpirvByteSize
            || tableEntry.blobByteSize % 4u != 0u) {
            return fail(InvalidShaderBlobByteSize, entryOffset);
        }
        std::uint32_t firstBlobWord = 0;
        std::memcpy(&firstBlobWord,
                    tableBytes.data() + static_cast<std::size_t>(tableEntry.blobByteOffset),
                    sizeof firstBlobWord);
        if (firstBlobWord != spirvMagicWord) {
            return fail(InvalidShaderBlobMagic, tableEntry.blobByteOffset);
        }
    }

    return CommandStreamShaderModuleTableView{tableBytes, entryCount};
}

} // namespace barrieww
