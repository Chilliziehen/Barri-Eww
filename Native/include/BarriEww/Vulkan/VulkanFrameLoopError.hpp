#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumeration; trivially thread-safe.
 * @brief Failure categories of the microkernel frame loop (ADR-0003 D3 items 2/3).
 *        Values are stable so the Java side can translate them into checked
 *        exceptions (§6.4).
 */
enum class VulkanFrameLoopError : std::uint32_t {
    /** framesInFlightCount was zero at creation. */
    InvalidFramesInFlightCount = 1,
    /** vkCreateFence failed while building the frame slots. */
    FenceCreationFailed = 2,
    /** vkWaitForFences failed (device loss and similar). */
    FenceWaitFailed = 3,
    /** vkResetFences failed. */
    FenceResetFailed = 4,
    /** vkQueueSubmit failed. */
    SubmissionFailed = 5,
    /** beginFrame was called while a frame is already open. */
    FrameAlreadyOpen = 6,
    /** submitFrame was called without an open frame. */
    FrameNotOpen = 7,
};

} // namespace barrieww
