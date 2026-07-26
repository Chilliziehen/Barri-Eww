#include "BarriEww/CommandStream/CommandStreamRenderingTemplateTableValidator.hpp"

#include <cstdint>
#include <cstring>

#include "BarriEww/CommandStream/CommandStreamAttachmentLoadOp.hpp"
#include "BarriEww/CommandStream/CommandStreamAttachmentStoreOp.hpp"
#include "BarriEww/CommandStream/CommandStreamHeader.hpp"
#include "BarriEww/CommandStream/CommandStreamImageLayout.hpp"

namespace barrieww {

/*
 * RenderingTemplateTable schema validation (v0.1). Algorithm principle: the template
 * directory is fixed-stride and directly checkable; each template then claims one
 * attachment region whose size is fully determined by its color count plus a possible
 * depth record, so region validity is a bounds-and-alignment equation. Regions MAY
 * overlap between templates (record sharing is legitimate deduplication), so no overlap
 * check is performed.
 *
 * Pseudocode (complete semantics):
 *   validate(tableBytes):
 *     if length < tableHeaderSize                         -> TableTooSmall
 *     if baseAddress % 8 != 0                             -> MisalignedTableBase
 *     (templateCount, headerReservedFlags) <- decode header
 *     if headerReservedFlags != 0                         -> NonZeroReservedFlags
 *     directoryEndOffset <- 8 + templateCount * 40
 *     if directoryEndOffset > length                      -> DirectoryOutOfBounds
 *     for templateSlot in 0 .. templateCount - 1:
 *       record <- decode 40-byte directory record
 *       if record.depthAttachmentPresent > 1              -> InvalidDepthAttachmentFlag
 *       attachmentCount <- record.colorAttachmentCount + record.depthAttachmentPresent
 *       if attachmentCount == 0                           -> EmptyAttachmentSet
 *       if record.renderAreaWidth == 0 or height == 0     -> EmptyRenderArea
 *       if record.layerCount == 0                         -> ZeroLayerCount
 *       regionByteSize <- attachmentCount * 32
 *       if record.attachmentsByteOffset % 8 != 0          -> MisalignedAttachmentRegion
 *       if region outside [directoryEndOffset, length]    -> AttachmentRegionOutOfBounds
 *       for each attachment record:
 *         if image layout unassigned                      -> UnknownAttachmentLayout
 *         if load op unassigned                           -> UnknownAttachmentLoadOp
 *         if store op unassigned                          -> UnknownAttachmentStoreOp
 *     return view(tableBytes, templateCount)
 */
std::expected<CommandStreamRenderingTemplateTableView,
              CommandStreamRenderingTemplateTableValidationFailure>
CommandStreamRenderingTemplateTableValidator::validate(
    std::span<const std::byte> tableBytes) noexcept {
    using enum CommandStreamRenderingTemplateTableValidationError;

    constexpr std::size_t tableHeaderByteSize = 8u;
    constexpr std::size_t templateRecordByteSize =
        sizeof(CommandStreamRenderingTemplateRecord);
    constexpr std::size_t attachmentRecordByteSize =
        sizeof(CommandStreamRenderingAttachmentRecord);

    const auto fail = [](CommandStreamRenderingTemplateTableValidationError error,
                         std::uint64_t byteOffset) {
        return std::unexpected(
            CommandStreamRenderingTemplateTableValidationFailure{error, byteOffset});
    };

    if (tableBytes.size() < tableHeaderByteSize) {
        return fail(TableTooSmall, 0u);
    }
    if (reinterpret_cast<std::uintptr_t>(tableBytes.data())
            % CommandStreamHeader::s_commandAlignment != 0u) {
        return fail(MisalignedTableBase, 0u);
    }

    std::uint32_t templateCount = 0;
    std::uint32_t headerReservedFlags = 0;
    std::memcpy(&templateCount, tableBytes.data(), sizeof templateCount);
    std::memcpy(&headerReservedFlags, tableBytes.data() + 4u, sizeof headerReservedFlags);
    if (headerReservedFlags != 0u) {
        return fail(NonZeroReservedFlags, 4u);
    }

    const std::uint64_t directoryEndOffset =
        tableHeaderByteSize
        + static_cast<std::uint64_t>(templateCount) * templateRecordByteSize;
    if (directoryEndOffset > tableBytes.size()) {
        return fail(DirectoryOutOfBounds, 0u);
    }

    for (std::uint32_t templateSlot = 0; templateSlot < templateCount; ++templateSlot) {
        const std::uint64_t directoryRecordOffset =
            tableHeaderByteSize
            + static_cast<std::uint64_t>(templateSlot) * templateRecordByteSize;
        CommandStreamRenderingTemplateRecord record{};
        std::memcpy(&record, tableBytes.data() + directoryRecordOffset,
                    templateRecordByteSize);

        if (record.depthAttachmentPresent > 1u) {
            return fail(InvalidDepthAttachmentFlag, directoryRecordOffset);
        }
        const std::uint64_t attachmentCount =
            static_cast<std::uint64_t>(record.colorAttachmentCount)
            + record.depthAttachmentPresent;
        if (attachmentCount == 0u) {
            return fail(EmptyAttachmentSet, directoryRecordOffset);
        }
        if (record.renderAreaWidth == 0u || record.renderAreaHeight == 0u) {
            return fail(EmptyRenderArea, directoryRecordOffset);
        }
        if (record.layerCount == 0u) {
            return fail(ZeroLayerCount, directoryRecordOffset);
        }

        const std::uint64_t regionByteSize = attachmentCount * attachmentRecordByteSize;
        if (record.attachmentsByteOffset % CommandStreamHeader::s_commandAlignment != 0u) {
            return fail(MisalignedAttachmentRegion, directoryRecordOffset);
        }
        if (record.attachmentsByteOffset < directoryEndOffset
            || record.attachmentsByteOffset > tableBytes.size()
            || regionByteSize > tableBytes.size() - record.attachmentsByteOffset) {
            return fail(AttachmentRegionOutOfBounds, directoryRecordOffset);
        }

        for (std::uint64_t attachmentIndex = 0; attachmentIndex < attachmentCount;
             ++attachmentIndex) {
            const std::uint64_t attachmentOffset =
                record.attachmentsByteOffset + attachmentIndex * attachmentRecordByteSize;
            CommandStreamRenderingAttachmentRecord attachment{};
            std::memcpy(&attachment, tableBytes.data() + attachmentOffset,
                        attachmentRecordByteSize);
            if (!isAssignedCommandStreamImageLayout(attachment.imageLayoutValue)) {
                return fail(UnknownAttachmentLayout, attachmentOffset);
            }
            if (!isAssignedCommandStreamAttachmentLoadOp(attachment.loadOpValue)) {
                return fail(UnknownAttachmentLoadOp, attachmentOffset);
            }
            if (!isAssignedCommandStreamAttachmentStoreOp(attachment.storeOpValue)) {
                return fail(UnknownAttachmentStoreOp, attachmentOffset);
            }
        }
    }

    return CommandStreamRenderingTemplateTableView{tableBytes, templateCount};
}

} // namespace barrieww
