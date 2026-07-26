#include "BarriEww/CommandStream/CommandStreamGraphicsPipelineTableValidator.hpp"

#include <cstdint>
#include <cstring>

#include "BarriEww/CommandStream/CommandStreamCompareOperation.hpp"
#include "BarriEww/CommandStream/CommandStreamCullMode.hpp"
#include "BarriEww/CommandStream/CommandStreamFrontFace.hpp"
#include "BarriEww/CommandStream/CommandStreamHeader.hpp"
#include "BarriEww/CommandStream/CommandStreamImageAspect.hpp"
#include "BarriEww/CommandStream/CommandStreamImageFormat.hpp"
#include "BarriEww/CommandStream/CommandStreamPrimitiveTopology.hpp"

namespace barrieww {

namespace {

/** Whether the raw format value is an assigned pure-color image format. */
bool isColorImageFormat(std::uint32_t rawImageFormatValue) {
    if (!isAssignedCommandStreamImageFormat(rawImageFormatValue)) {
        return false;
    }
    return commandStreamImageFormatAspectMask(
               static_cast<CommandStreamImageFormat>(rawImageFormatValue))
           == static_cast<std::uint32_t>(CommandStreamImageAspect::Color);
}

/** Whether the raw format value is an assigned depth-capable image format. */
bool isDepthCapableImageFormat(std::uint32_t rawImageFormatValue) {
    if (!isAssignedCommandStreamImageFormat(rawImageFormatValue)) {
        return false;
    }
    return (commandStreamImageFormatAspectMask(
                static_cast<CommandStreamImageFormat>(rawImageFormatValue))
            & static_cast<std::uint32_t>(CommandStreamImageAspect::Depth))
           != 0u;
}

} // namespace

/*
 * GraphicsPipelineTable schema validation (v0.1). Algorithm principle: the table is
 * pure fixed-stride directory (no variable regions — color formats are inlined), so
 * validation is one exact-size check plus one linear pass of per-record field rules.
 * Cross-table facts (shader slot existence, template format compatibility) are NOT
 * checked here (ADR-0002 D5: they live where the other table is present).
 *
 * Pseudocode (complete semantics):
 *   validate(tableBytes):
 *     if length < tableHeaderSize                          -> TableTooSmall
 *     if baseAddress % 8 != 0                              -> MisalignedTableBase
 *     (pipelineCount, headerReservedFlags) <- decode header
 *     if headerReservedFlags != 0                          -> NonZeroReservedFlags
 *     if length != 8 + pipelineCount * 80                  -> TableSizeMismatch
 *     for pipelineSlot in 0 .. pipelineCount - 1:
 *       record <- decode 80-byte record
 *       if record.reservedFlags != 0                       -> NonZeroRecordReservedFlags
 *       if topology unassigned                             -> UnknownTopology
 *       if cull mode unassigned                            -> UnknownCullMode
 *       if front face unassigned                           -> UnknownFrontFace
 *       if pushConstantByteSize % 4 != 0 or > 128          -> InvalidPushConstantByteSize
 *       if colorAttachmentCount > 8                        -> TooManyColorAttachments
 *       if colorAttachmentCount == 0 and no depth format   -> EmptyAttachmentSet
 *       used color formats must be assigned color formats  -> UnknownColorAttachmentFormat
 *       unused color format indices must be zero           -> NonZeroUnusedColorFormat
 *       depth format must be 0 or depth-capable            -> InvalidDepthAttachmentFormat
 *       depth enables must each be 0 or 1                  -> InvalidDepthEnableFlag
 *       write enable requires test enable                  -> DepthWriteWithoutDepthTest
 *       test enable requires a depth format                -> DepthStateWithoutDepthAttachment
 *       test on: compare assigned                          -> UnknownDepthCompareOperation
 *       test off: compare == 0                             -> NonZeroDisabledDepthCompareOperation
 *     return view(tableBytes, pipelineCount)
 */
std::expected<CommandStreamGraphicsPipelineTableView,
              CommandStreamGraphicsPipelineTableValidationFailure>
CommandStreamGraphicsPipelineTableValidator::validate(
    std::span<const std::byte> tableBytes) noexcept {
    using enum CommandStreamGraphicsPipelineTableValidationError;

    constexpr std::size_t tableHeaderByteSize = 8u;
    constexpr std::size_t pipelineRecordByteSize =
        sizeof(CommandStreamGraphicsPipelineRecord);

    const auto fail = [](CommandStreamGraphicsPipelineTableValidationError error,
                         std::uint64_t byteOffset) {
        return std::unexpected(
            CommandStreamGraphicsPipelineTableValidationFailure{error, byteOffset});
    };

    if (tableBytes.size() < tableHeaderByteSize) {
        return fail(TableTooSmall, 0u);
    }
    if (reinterpret_cast<std::uintptr_t>(tableBytes.data())
            % CommandStreamHeader::s_commandAlignment != 0u) {
        return fail(MisalignedTableBase, 0u);
    }

    std::uint32_t pipelineCount = 0;
    std::uint32_t headerReservedFlags = 0;
    std::memcpy(&pipelineCount, tableBytes.data(), sizeof pipelineCount);
    std::memcpy(&headerReservedFlags, tableBytes.data() + 4u, sizeof headerReservedFlags);
    if (headerReservedFlags != 0u) {
        return fail(NonZeroReservedFlags, 4u);
    }
    if (tableBytes.size()
        != tableHeaderByteSize
               + static_cast<std::uint64_t>(pipelineCount) * pipelineRecordByteSize) {
        return fail(TableSizeMismatch, 0u);
    }

    for (std::uint32_t pipelineSlot = 0; pipelineSlot < pipelineCount; ++pipelineSlot) {
        const std::uint64_t recordOffset =
            tableHeaderByteSize
            + static_cast<std::uint64_t>(pipelineSlot) * pipelineRecordByteSize;
        CommandStreamGraphicsPipelineRecord record{};
        std::memcpy(&record, tableBytes.data() + recordOffset, pipelineRecordByteSize);

        if (record.reservedFlags != 0u) {
            return fail(NonZeroRecordReservedFlags, recordOffset);
        }
        if (!isAssignedCommandStreamPrimitiveTopology(record.topologyValue)) {
            return fail(UnknownTopology, recordOffset);
        }
        if (!isAssignedCommandStreamCullMode(record.cullModeValue)) {
            return fail(UnknownCullMode, recordOffset);
        }
        if (!isAssignedCommandStreamFrontFace(record.frontFaceValue)) {
            return fail(UnknownFrontFace, recordOffset);
        }
        if (record.pushConstantByteSize % 4u != 0u || record.pushConstantByteSize > 128u) {
            return fail(InvalidPushConstantByteSize, recordOffset);
        }
        if (record.colorAttachmentCount
            > CommandStreamGraphicsPipelineRecord::s_maximumColorAttachmentCount) {
            return fail(TooManyColorAttachments, recordOffset);
        }
        if (record.colorAttachmentCount == 0u && record.depthAttachmentFormatValue == 0u) {
            return fail(EmptyAttachmentSet, recordOffset);
        }
        for (std::uint32_t formatIndex = 0;
             formatIndex < CommandStreamGraphicsPipelineRecord::s_maximumColorAttachmentCount;
             ++formatIndex) {
            const std::uint32_t formatValue = record.colorAttachmentFormatValues[formatIndex];
            if (formatIndex < record.colorAttachmentCount) {
                if (!isColorImageFormat(formatValue)) {
                    return fail(UnknownColorAttachmentFormat, recordOffset);
                }
            } else if (formatValue != 0u) {
                return fail(NonZeroUnusedColorFormat, recordOffset);
            }
        }
        if (record.depthAttachmentFormatValue != 0u
            && !isDepthCapableImageFormat(record.depthAttachmentFormatValue)) {
            return fail(InvalidDepthAttachmentFormat, recordOffset);
        }
        if (record.depthTestEnable > 1u || record.depthWriteEnable > 1u) {
            return fail(InvalidDepthEnableFlag, recordOffset);
        }
        if (record.depthWriteEnable == 1u && record.depthTestEnable == 0u) {
            return fail(DepthWriteWithoutDepthTest, recordOffset);
        }
        if (record.depthTestEnable == 1u && record.depthAttachmentFormatValue == 0u) {
            return fail(DepthStateWithoutDepthAttachment, recordOffset);
        }
        if (record.depthTestEnable == 1u) {
            if (!isAssignedCommandStreamCompareOperation(record.depthCompareOperationValue)) {
                return fail(UnknownDepthCompareOperation, recordOffset);
            }
        } else if (record.depthCompareOperationValue != 0u) {
            return fail(NonZeroDisabledDepthCompareOperation, recordOffset);
        }
    }

    return CommandStreamGraphicsPipelineTableView{tableBytes, pipelineCount};
}

} // namespace barrieww
