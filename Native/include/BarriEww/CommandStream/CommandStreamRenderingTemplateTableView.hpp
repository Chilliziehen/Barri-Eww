#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "BarriEww/CommandStream/CommandStreamRenderingAttachmentRecord.hpp"
#include "BarriEww/CommandStream/CommandStreamRenderingTemplateRecord.hpp"

namespace barrieww {

class CommandStreamRenderingTemplateTableValidator;

/**
 * @note ThreadSafety: Read-only view; safe to query concurrently from multiple
 *       threads. This blanket statement covers all accessors below (spec §2.6).
 * @brief Non-owning view over a VALIDATED RenderingTemplateTable section. Instances are
 *        created exclusively by CommandStreamRenderingTemplateTableValidator (view
 *        existence proves schema validity), so access is unchecked. Consumed by the
 *        recorder when it translates BeginRendering into a VkRenderingInfo.
 * @warning MemoryOwnership: The view BORROWS the underlying section bytes (§6.3); the
 *          provider must keep them alive and unmodified for the lifetime of this view.
 */
class CommandStreamRenderingTemplateTableView {
    friend class CommandStreamRenderingTemplateTableValidator;

public:
    /** The number of templates (slots 0..templateCount-1) in the table (see class notes). */
    [[nodiscard]] std::uint32_t templateCount() const noexcept { return m_templateCount; }

    /**
     * @note ThreadSafety: Thread-safe (read-only, see class note).
     * @brief Decodes one template directory record. Precondition: templateSlot < templateCount().
     * @param templateSlot The template slot whose record to decode.
     * @return CommandStreamRenderingTemplateRecord The decoded directory record, by value.
     */
    [[nodiscard]] CommandStreamRenderingTemplateRecord
    templateRecord(std::uint32_t templateSlot) const noexcept {
        CommandStreamRenderingTemplateRecord record{};
        std::memcpy(&record,
                    m_tableBytes.data() + 8u
                        + static_cast<std::size_t>(templateSlot)
                              * sizeof(CommandStreamRenderingTemplateRecord),
                    sizeof record);
        return record;
    }

    /**
     * @note ThreadSafety: Thread-safe (read-only, see class note).
     * @brief Decodes one attachment record of a template. Color records occupy indices
     *        0..colorAttachmentCount-1; when depthAttachmentPresent is 1 the depth record
     *        follows at index colorAttachmentCount. Precondition: the index is in range
     *        for the template.
     * @param templateSlot The template slot the attachment belongs to.
     * @param attachmentIndex The attachment index within the template's region.
     * @return CommandStreamRenderingAttachmentRecord The decoded attachment record, by value.
     */
    [[nodiscard]] CommandStreamRenderingAttachmentRecord
    attachmentRecord(std::uint32_t templateSlot,
                     std::uint32_t attachmentIndex) const noexcept {
        const CommandStreamRenderingTemplateRecord record = templateRecord(templateSlot);
        CommandStreamRenderingAttachmentRecord attachment{};
        std::memcpy(&attachment,
                    m_tableBytes.data() + record.attachmentsByteOffset
                        + static_cast<std::size_t>(attachmentIndex)
                              * sizeof(CommandStreamRenderingAttachmentRecord),
                    sizeof attachment);
        return attachment;
    }

private:
    CommandStreamRenderingTemplateTableView(std::span<const std::byte> tableBytes,
                                            std::uint32_t templateCount) noexcept
        : m_tableBytes(tableBytes), m_templateCount(templateCount) {}

    std::span<const std::byte> m_tableBytes;
    std::uint32_t m_templateCount;
};

} // namespace barrieww
