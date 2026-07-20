#pragma once

#include <expected>

#include <vulkan/vulkan.h>

#include "BarriEww/CommandStream/CommandStreamBarrierBatchTableView.hpp"
#include "BarriEww/CommandStream/CommandStreamView.hpp"
#include "BarriEww/Vulkan/CommandBufferRecordingFailure.hpp"
#include "BarriEww/Vulkan/VulkanBufferTable.hpp"
#include "BarriEww/Vulkan/VulkanImageTable.hpp"
#include "BarriEww/Vulkan/VulkanPipelineTable.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Stateless. Distinct command buffers from distinct command pools
 *       may be recorded concurrently (one pool per recording thread, the Vulkan
 *       external-synchronization rule and the THREADED_RECORDING lane model); never
 *       record into the same pool from two threads.
 * @brief LOAD-TIME recorder of BECS lane streams into Vulkan command buffers (ADR-0003
 *        D2: this is a Recorder, not a replayer — it runs once per stream when a graph
 *        artifact is loaded, producing prerecorded, reusable command buffers whose
 *        per-frame cost is submission only). Payload decoding happens here, so this is
 *        also where slot and range validation land (ADR-0002 D5: still load-time).
 *        v0.1 opcode coverage: CopyBuffer and ExecuteBarrierBatch (global and buffer
 *        barriers, mapped onto legacy vkCmdPipelineBarrier — sync2 bit values below
 *        bit 32 equal the sync1 bits by Vulkan's design, so the mapping is a
 *        truncation plus a per-call OR of the per-barrier stage masks; masks using
 *        bits above bit 31 are rejected until a sync2 recording path lands).
 *        Assigned-but-unimplemented opcodes fail loudly rather than being skipped.
 */
class CommandBufferRecorder {
public:
    /**
     * @note ThreadSafety: See the class note (pool rule).
     * @brief Records one validated lane stream into commandBuffer: begins the buffer,
     *        translates every command (checking slots against the materialized table
     *        and copy ranges against buffer sizes), and ends the buffer. On failure the
     *        command buffer is left in an unusable state and must be reset or freed by
     *        the caller.
     * @param commandBuffer The target command buffer, freshly allocated or reset;
     *        recorded without one-time flags so the result is reusable (ADR-0003 D1).
     * @param streamView The validated lane stream to record.
     * @param bufferTable The materialized buffer table the stream's slots refer to.
     * @param barrierBatchTableView The validated barrier batch table for
     *        ExecuteBarrierBatch commands, or nullptr for streams without barriers
     *        (executing a batch then fails with MissingBarrierBatchTable).
     * @param pipelineTable The materialized pipeline table for BindComputePipeline /
     *        Dispatch / PushBufferDeviceAddress commands, or nullptr for streams
     *        without pipelines (those commands then fail with MissingPipelineTable).
     * @param imageTable The materialized image table for image barriers and image
     *        commands (ClearColorImage / CopyImageToBuffer), or nullptr for streams
     *        without images (those commands then fail with MissingImageTable).
     * @return std::expected<void, CommandBufferRecordingFailure> Nothing on success;
     *         the first failure (category, command index, VkResult) otherwise. Errors
     *         are values so the FFM boundary needs no unwinding (§6.4).
     * @warning MemoryOwnership: All inputs are BORROWED; the recorder owns nothing.
     *          commandBuffer's pool, the stream's backing bytes and both tables must
     *          outlive the recorded command buffer's use.
     */
    [[nodiscard]] static std::expected<void, CommandBufferRecordingFailure>
    record(VkCommandBuffer commandBuffer, const CommandStreamView& streamView,
           const VulkanBufferTable& bufferTable,
           const CommandStreamBarrierBatchTableView* barrierBatchTableView = nullptr,
           const VulkanPipelineTable* pipelineTable = nullptr,
           const VulkanImageTable* imageTable = nullptr);
};

} // namespace barrieww
