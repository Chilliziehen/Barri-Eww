#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumeration; trivially thread-safe.
 * @brief Failure categories of load-time command recording (BECS lane stream →
 *        VkCommandBuffer). Values are stable so the Java side can translate them into
 *        checked exceptions (§6.4). Slot and range checks live here because recording
 *        is the load-time point where payloads are decoded (ADR-0002 D5).
 */
enum class CommandBufferRecordingError : std::uint32_t {
    /** vkBeginCommandBuffer failed. */
    CommandBufferBeginFailed = 1,
    /** vkEndCommandBuffer failed. */
    CommandBufferEndFailed = 2,
    /** The opcode is assigned in the catalog but not implemented by this recorder yet. */
    UnsupportedOpcode = 3,
    /** A command references a buffer slot outside the table. */
    BufferSlotOutOfRange = 4,
    /** A command references an imported slot that has no bound handle (v0.1). */
    UnboundImportedBuffer = 5,
    /** A copy's offset + byteCount leaves the referenced buffer. */
    CopyRangeOutOfBounds = 6,
    /** The stream binds or dispatches pipelines but no pipeline table was provided. */
    MissingPipelineTable = 11,
    /** A command references a pipeline slot outside the table. */
    PipelineSlotOutOfRange = 12,
    /** Dispatch or push constants were recorded before any BindComputePipeline. */
    NoBoundComputePipeline = 13,
    /** A push constant write leaves the bound pipeline's declared range. */
    PushConstantRangeExceeded = 14,
    /** PushBufferDeviceAddress references a buffer without a device address. */
    MissingBufferDeviceAddress = 15,
    /** The stream executes a barrier batch but no barrier batch table was provided. */
    MissingBarrierBatchTable = 7,
    /** A command references a barrier batch slot outside the table. */
    BarrierBatchSlotOutOfRange = 8,
    /** A synchronization2 mask uses bits above bit 31; the v0.1 legacy-mapping
     *  recorder cannot express them via vkCmdPipelineBarrier (sync2 path pending). */
    UnmappableSynchronizationScope = 9,
    /** A buffer barrier's byte range leaves the referenced buffer. */
    BarrierRangeOutOfBounds = 10,
};

} // namespace barrieww
