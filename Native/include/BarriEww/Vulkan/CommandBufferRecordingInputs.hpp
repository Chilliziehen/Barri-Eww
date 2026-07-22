#pragma once

namespace barrieww {

class VulkanBufferTable;
class VulkanGraphicsPipelineTable;
class VulkanImageTable;
class VulkanImageViewTable;
class VulkanPipelineTable;
class CommandStreamBarrierBatchTableView;
class CommandStreamRenderingTemplateTableView;

/**
 * @note ThreadSafety: Plain value type of borrowed pointers; no synchronization. Safe
 *       to build and pass by value; the pointed-to tables follow their own thread rules.
 * @brief The materialized tables and views one recording resolves its slots from
 *        (ADR-0002 D2 slot indirection). bufferTable is required (almost every command
 *        may reference a buffer); the rest are optional and a command referencing a
 *        table not supplied fails with the matching Missing...Table error (ADR-0002 D5).
 *        Bundling them keeps CommandBufferRecorder::record's signature stable as new
 *        table kinds land (image views, rendering templates, ...), instead of growing
 *        another positional parameter each time.
 * @warning MemoryOwnership: All pointers are BORROWED; the recorder owns nothing. Each
 *          referenced table must outlive the recorded command buffer's use.
 */
struct CommandBufferRecordingInputs {
    const VulkanBufferTable* bufferTable = nullptr;
    const CommandStreamBarrierBatchTableView* barrierBatchTableView = nullptr;
    const VulkanPipelineTable* pipelineTable = nullptr;
    const VulkanImageTable* imageTable = nullptr;
    const VulkanImageViewTable* imageViewTable = nullptr;
    const CommandStreamRenderingTemplateTableView* renderingTemplateTableView = nullptr;
    const VulkanGraphicsPipelineTable* graphicsPipelineTable = nullptr;
};

} // namespace barrieww
