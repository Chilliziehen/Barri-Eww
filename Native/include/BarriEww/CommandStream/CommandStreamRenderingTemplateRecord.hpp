#pragma once

#include <cstddef>
#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief One 40-byte RenderingTemplateTable directory record (schema v0.1). A template
 *        is the compile-time output for one BeginRendering site (dynamic rendering, no
 *        render passes — ADR-0003 / MC 26.2 baseline): its attachment records live at
 *        attachmentsByteOffset (relative to the table base), the colorAttachmentCount
 *        color records first, then one depth/stencil record when depthAttachmentPresent
 *        is 1. Templates may share attachment regions (deduplication is legitimate).
 *        Layout is the wire format, pinned by the static_asserts below (ADR-0002 D1).
 */
struct CommandStreamRenderingTemplateRecord {
    std::uint32_t colorAttachmentCount;
    std::uint32_t depthAttachmentPresent;
    std::int32_t renderAreaOffsetX;
    std::int32_t renderAreaOffsetY;
    std::uint32_t renderAreaWidth;
    std::uint32_t renderAreaHeight;
    std::uint32_t layerCount;
    std::uint32_t viewMask;
    std::uint64_t attachmentsByteOffset;
};

static_assert(sizeof(CommandStreamRenderingTemplateRecord) == 40u,
              "RenderingTemplateTable directory records must be exactly 40 bytes");
static_assert(offsetof(CommandStreamRenderingTemplateRecord, depthAttachmentPresent) == 4u);
static_assert(offsetof(CommandStreamRenderingTemplateRecord, renderAreaWidth) == 16u);
static_assert(offsetof(CommandStreamRenderingTemplateRecord, layerCount) == 24u);
static_assert(offsetof(CommandStreamRenderingTemplateRecord, attachmentsByteOffset) == 32u);

} // namespace barrieww
