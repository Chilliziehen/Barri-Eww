#pragma once

#include <cstddef>
#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief One 32-byte rendering attachment record of a rendering template (schema v0.1).
 *        It names an ImageViewHandleTable slot, the neutral layout the attachment is in
 *        during rendering, its load/store operations, and a clear value. The clear value
 *        is four floats for color attachments; for a depth/stencil attachment
 *        clearValue[0] is the depth clear (float) and clearValue[1] reinterprets its
 *        bits as the u32 stencil clear. Layout is the wire format, pinned below.
 */
struct CommandStreamRenderingAttachmentRecord {
    std::uint32_t imageViewSlot;
    std::uint32_t imageLayoutValue;
    std::uint32_t loadOpValue;
    std::uint32_t storeOpValue;
    float clearValue[4];
};

static_assert(sizeof(CommandStreamRenderingAttachmentRecord) == 32u,
              "Rendering attachment records must be exactly 32 bytes (schema v0.1)");
static_assert(offsetof(CommandStreamRenderingAttachmentRecord, imageLayoutValue) == 4u);
static_assert(offsetof(CommandStreamRenderingAttachmentRecord, loadOpValue) == 8u);
static_assert(offsetof(CommandStreamRenderingAttachmentRecord, storeOpValue) == 12u);
static_assert(offsetof(CommandStreamRenderingAttachmentRecord, clearValue) == 16u);

} // namespace barrieww
