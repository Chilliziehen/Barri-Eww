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
};

} // namespace barrieww
