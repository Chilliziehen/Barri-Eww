#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include <vulkan/vulkan.h>

#include "BarriEww/Vulkan/VulkanContext.hpp"
#include "BarriEww/Vulkan/VulkanFrameLoopFailure.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Not thread-safe. The frame loop is driven by exactly one render
 *       thread (later Minecraft's render thread via FFM downcalls); recording threads
 *       never touch it (THREADED_RECORDING parallelism happens at load, not here).
 * @brief Microkernel frame pacing and submission (ADR-0003 D3 whitelist items 2 and 3):
 *        framesInFlightCount slots, each with a fence, cycled round-robin.
 *        beginFrame() blocks until the slot's previous work finished — that wait is the
 *        contract that makes per-frame writes into slot-owned mapped memory safe
 *        (ADR-0001: the Java side may write slot K's parameters only after beginFrame
 *        returned K). submitFrame() submits prerecorded, reusable command buffers with
 *        the slot's fence — the whole per-frame CPU cost of executing a compiled graph
 *        (ADR-0003 D1: one wait, one reset, one submit; O(1) per frame regardless of
 *        scene complexity). Swapchain acquire/present and semaphores attach around this
 *        core in the Minecraft-integration increment; the fence pacing stays unchanged.
 * @warning MemoryOwnership: OWNS its fences (destroyed in the destructor after a
 *          vkQueueWaitIdle drain). BORROWS the VulkanContext's device and graphics
 *          queue, which must outlive this loop. Submitted command buffers are borrowed
 *          per call and must stay valid until their slot's fence signals.
 */
class VulkanFrameLoop {
public:
    /**
     * @note ThreadSafety: Not thread-safe; call on the init path.
     * @brief Creates the frame slots with signalled fences (so the first beginFrame of
     *        every slot does not block).
     * @param vulkanContext The borrowed device context to create against.
     * @param framesInFlightCount The number of frames the CPU may run ahead of the
     *        GPU; must be at least 1 (2 is the conventional double-buffering choice).
     * @return std::expected<VulkanFrameLoop, VulkanFrameLoopFailure> The loop on
     *         success; the first failure otherwise. Errors are values (§6.4).
     */
    [[nodiscard]] static std::expected<VulkanFrameLoop, VulkanFrameLoopFailure>
    create(const VulkanContext& vulkanContext, std::uint32_t framesInFlightCount);

    VulkanFrameLoop(const VulkanFrameLoop&) = delete;
    VulkanFrameLoop& operator=(const VulkanFrameLoop&) = delete;

    /** Move transfers ownership of the fences (source becomes empty). */
    VulkanFrameLoop(VulkanFrameLoop&& movedFrom) noexcept
        : m_logicalDevice(movedFrom.m_logicalDevice)
        , m_submissionQueue(movedFrom.m_submissionQueue)
        , m_frameSlotFences(std::move(movedFrom.m_frameSlotFences))
        , m_currentFrameSlot(movedFrom.m_currentFrameSlot)
        , m_isFrameOpen(movedFrom.m_isFrameOpen) {
        movedFrom.m_frameSlotFences.clear();
    }

    VulkanFrameLoop& operator=(VulkanFrameLoop&&) = delete;

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * @brief Drains the queue (vkQueueWaitIdle) and destroys the fences. Draining first
     *        keeps destruction valid even with submissions still in flight.
     */
    ~VulkanFrameLoop();

    /** The number of frame slots (see class notes). */
    [[nodiscard]] std::uint32_t framesInFlightCount() const noexcept {
        return static_cast<std::uint32_t>(m_frameSlotFences.size());
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * @brief Opens the next frame: blocks until the slot's previous submission
     *        finished, then resets the slot's fence. After this returns, slot-owned
     *        per-frame resources (mapped parameter memory, the slot's prerecorded
     *        dynamic buffers) are safe to reuse.
     * @return std::expected<std::uint32_t, VulkanFrameLoopFailure> The opened frame
     *         slot index on success; FrameAlreadyOpen when called twice without a
     *         submitFrame in between.
     */
    [[nodiscard]] std::expected<std::uint32_t, VulkanFrameLoopFailure> beginFrame();

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * @brief Submits the frame's command buffers in one vkQueueSubmit signalling the
     *        slot's fence, closes the frame and advances to the next slot. An empty
     *        span is a legal skip-frame (the fence still signals once prior queue work
     *        completes).
     * @param commandBuffers The prerecorded command buffers to execute this frame, in
     *        submission order.
     * @return std::expected<void, VulkanFrameLoopFailure> Nothing on success;
     *         FrameNotOpen without a preceding beginFrame, or the submission failure.
     * @warning MemoryOwnership: commandBuffers are borrowed and must stay valid until
     *          this slot's fence signals (the next beginFrame of this slot).
     */
    [[nodiscard]] std::expected<void, VulkanFrameLoopFailure>
    submitFrame(std::span<const VkCommandBuffer> commandBuffers);

private:
    VulkanFrameLoop(VkDevice logicalDevice, VkQueue submissionQueue,
                    std::vector<VkFence> frameSlotFences) noexcept
        : m_logicalDevice(logicalDevice)
        , m_submissionQueue(submissionQueue)
        , m_frameSlotFences(std::move(frameSlotFences))
        , m_currentFrameSlot(0)
        , m_isFrameOpen(false) {}

    VkDevice m_logicalDevice;
    VkQueue m_submissionQueue;
    std::vector<VkFence> m_frameSlotFences;
    std::uint32_t m_currentFrameSlot;
    bool m_isFrameOpen;
};

} // namespace barrieww
