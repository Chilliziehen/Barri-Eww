#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumerations and the constexpr predicate below are stateless and
 *       fully thread-safe.
 * @brief Section types of a BECS module container (ADR-0002 D2). Handle tables carry
 *        bake-time descriptions that init resolves into dense native-handle arrays; the
 *        template tables carry baked barrier / rendering / push-descriptor blueprints;
 *        LaneStream sections embed one lane command stream each. Content schemas of the
 *        non-stream sections are defined by later increments; the container treats them
 *        as opaque byte ranges.
 */
enum class CommandStreamModuleSectionType : std::uint16_t {
    PipelineHandleTable = 0x0001,
    BufferHandleTable = 0x0002,
    ImageHandleTable = 0x0003,
    ImageViewHandleTable = 0x0004,
    SamplerHandleTable = 0x0005,
    /** SPIR-V shader blobs (variable-size records; pipelines reference blobs by slot). */
    ShaderModuleTable = 0x0006,
    /** Fixed graphics pipeline state against dynamic rendering (§9.14). */
    GraphicsPipelineTable = 0x0007,

    BarrierBatchTable = 0x0010,
    RenderingTemplateTable = 0x0011,
    PushDescriptorTemplateTable = 0x0012,

    LaneStream = 0x0020,
};

/**
 * @note ThreadSafety: Thread-safe (pure constant expression).
 * @brief Whether the raw section type value is assigned in the v0.1 catalog. Unassigned
 *        values are rejected at load: with exact-major / additive-minor versioning
 *        (ADR-0002 D6) an accepted stream can only contain types this build knows.
 * @param rawSectionTypeValue The 16-bit section type value read from a module directory.
 * @return bool True when the value names an assigned section type.
 */
[[nodiscard]] constexpr bool
isAssignedCommandStreamModuleSectionType(std::uint16_t rawSectionTypeValue) noexcept {
    switch (static_cast<CommandStreamModuleSectionType>(rawSectionTypeValue)) {
        case CommandStreamModuleSectionType::PipelineHandleTable:
        case CommandStreamModuleSectionType::BufferHandleTable:
        case CommandStreamModuleSectionType::ImageHandleTable:
        case CommandStreamModuleSectionType::ImageViewHandleTable:
        case CommandStreamModuleSectionType::SamplerHandleTable:
        case CommandStreamModuleSectionType::ShaderModuleTable:
        case CommandStreamModuleSectionType::GraphicsPipelineTable:
        case CommandStreamModuleSectionType::BarrierBatchTable:
        case CommandStreamModuleSectionType::RenderingTemplateTable:
        case CommandStreamModuleSectionType::PushDescriptorTemplateTable:
        case CommandStreamModuleSectionType::LaneStream:
            return true;
    }
    return false;
}

} // namespace barrieww
