#pragma once

#include <cstddef>
#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief One 80-byte GraphicsPipelineTable record (schema v0.1, §9.14): the complete
 *        compile-time-fixed state of one graphics pipeline materialized against dynamic
 *        rendering. Color attachment formats are inlined (up to 8; unused indices are
 *        zero) so the table stays flat with no variable-size regions. State NOT encoded
 *        here is fixed by the v0.1 schema: no vertex input (vertex pulling via buffer
 *        device addresses), fill polygon mode, one-sample multisampling, blending
 *        disabled with all channels written, no stencil, dynamic viewport/scissor.
 *        Layout is the wire format, pinned by the static_asserts below (ADR-0002 D1).
 */
struct CommandStreamGraphicsPipelineRecord {
    /** The number of usable entries in colorAttachmentFormatValues. */
    static constexpr std::uint32_t s_maximumColorAttachmentCount = 8u;

    std::uint32_t vertexShaderModuleSlot;
    std::uint32_t fragmentShaderModuleSlot;
    std::uint32_t pushConstantByteSize;
    std::uint32_t topologyValue;
    std::uint32_t colorAttachmentCount;
    std::uint32_t depthAttachmentFormatValue;
    std::uint32_t depthTestEnable;
    std::uint32_t depthWriteEnable;
    std::uint32_t depthCompareOperationValue;
    std::uint32_t cullModeValue;
    std::uint32_t frontFaceValue;
    std::uint32_t reservedFlags;
    std::uint32_t colorAttachmentFormatValues[s_maximumColorAttachmentCount];
};

static_assert(sizeof(CommandStreamGraphicsPipelineRecord) == 80u,
              "GraphicsPipelineTable records must be exactly 80 bytes");
static_assert(offsetof(CommandStreamGraphicsPipelineRecord, pushConstantByteSize) == 8u);
static_assert(offsetof(CommandStreamGraphicsPipelineRecord, colorAttachmentCount) == 16u);
static_assert(offsetof(CommandStreamGraphicsPipelineRecord, depthCompareOperationValue) == 32u);
static_assert(offsetof(CommandStreamGraphicsPipelineRecord, reservedFlags) == 44u);
static_assert(offsetof(CommandStreamGraphicsPipelineRecord, colorAttachmentFormatValues) == 48u);

} // namespace barrieww
