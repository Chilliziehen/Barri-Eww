#include "BarriEww/CommandStream/CommandStreamPipelineHandleTableValidator.hpp"

#include <cstdint>
#include <cstring>

#include "BarriEww/CommandStream/CommandStreamHeader.hpp"
#include "BarriEww/CommandStream/CommandStreamPipelineKind.hpp"

namespace barrieww {

/*
 * PipelineHandleTable schema validation (v0.1). Algorithm principle: fixed-stride
 * record array, so validity is the exact size equation (8 + 16 * entryCount == section
 * size) plus per-entry field rules; the shaderModuleSlot cross-reference is validated
 * at materialization where the shader table is present.
 *
 * Pseudocode (complete semantics):
 *   validate(tableBytes):
 *     if length < tableHeaderSize                        -> TableTooSmall
 *     if baseAddress % 8 != 0                            -> MisalignedTableBase
 *     (entryCount, headerReservedFlags) <- decode header
 *     if headerReservedFlags != 0                        -> NonZeroReservedFlags
 *     if length != 8 + entryCount * 16                   -> TableSizeMismatch
 *     for slotIndex in 0 .. entryCount - 1:
 *       entry <- decode 16-byte entry
 *       if entry.reservedFlags != 0                      -> NonZeroReservedFlags
 *       if kind unassigned                               -> UnknownPipelineKind
 *       if pushSize % 4 != 0 or pushSize > 128           -> InvalidPushConstantByteSize
 *     return view(tableBytes, entryCount)
 */
std::expected<CommandStreamPipelineHandleTableView, CommandStreamHandleTableValidationFailure>
CommandStreamPipelineHandleTableValidator::validate(
    std::span<const std::byte> tableBytes) noexcept {
    using enum CommandStreamHandleTableValidationError;

    constexpr std::size_t tableHeaderByteSize = 8u;
    constexpr std::size_t entryByteSize = sizeof(CommandStreamPipelineHandleTableEntry);
    constexpr std::uint32_t maximumPushConstantByteSize = 128u;

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
        CommandStreamPipelineHandleTableEntry tableEntry{};
        std::memcpy(&tableEntry, tableBytes.data() + entryOffset, entryByteSize);

        if (tableEntry.reservedFlags != 0u) {
            return fail(NonZeroReservedFlags, entryOffset);
        }
        if (!isAssignedCommandStreamPipelineKind(tableEntry.pipelineKindValue)) {
            return fail(UnknownPipelineKind, entryOffset);
        }
        if (tableEntry.pushConstantByteSize % 4u != 0u
            || tableEntry.pushConstantByteSize > maximumPushConstantByteSize) {
            return fail(InvalidPushConstantByteSize, entryOffset);
        }
    }

    return CommandStreamPipelineHandleTableView{tableBytes, entryCount};
}

} // namespace barrieww
