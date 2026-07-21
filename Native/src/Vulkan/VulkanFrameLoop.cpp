#include "BarriEww/Vulkan/VulkanFrameLoop.hpp"

#include <cstdint>

namespace barrieww {

/*
 * Frame pacing (ADR-0003 D3 items 2/3). Algorithm principle: N slots cycled
 * round-robin, each guarded by one fence, throttle the CPU to at most N frames ahead
 * of the GPU. A slot's fence is created signalled (first use never blocks), waited and
 * reset when the slot is reopened, and signalled again by that frame's single
 * submission. The wait in beginFrame is what transfers ownership of slot-bound
 * resources (mapped parameter memory, dynamic command buffers) back to the CPU.
 *
 * Pseudocode (complete semantics):
 *   beginFrame():
 *     if a frame is open                       -> FrameAlreadyOpen
 *     wait(fence[currentFrameSlot], forever)   -> FenceWaitFailed on error
 *     reset(fence[currentFrameSlot])           -> FenceResetFailed on error
 *     frameOpen <- true
 *     return currentFrameSlot
 *   submitFrame(commandBuffers):
 *     if no frame is open                      -> FrameNotOpen
 *     vkQueueSubmit(queue, commandBuffers, signal fence[currentFrameSlot])
 *                                              -> SubmissionFailed on error
 *     frameOpen <- false
 *     currentFrameSlot <- (currentFrameSlot + 1) % slotCount
 */
std::expected<VulkanFrameLoop, VulkanFrameLoopFailure>
VulkanFrameLoop::create(const VulkanContext& vulkanContext,
                        std::uint32_t framesInFlightCount) {
    using enum VulkanFrameLoopError;
    constexpr std::uint32_t noFrameSlot = UINT32_MAX;

    if (framesInFlightCount == 0u) {
        return std::unexpected(
            VulkanFrameLoopFailure{InvalidFramesInFlightCount, noFrameSlot, 0});
    }

    std::vector<VkFence> frameSlotFences;
    frameSlotFences.reserve(framesInFlightCount);
    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (std::uint32_t frameSlot = 0; frameSlot < framesInFlightCount; ++frameSlot) {
        VkFence frameSlotFence = VK_NULL_HANDLE;
        const VkResult vulkanResult = vkCreateFence(vulkanContext.logicalDevice(),
                                                    &fenceCreateInfo, nullptr,
                                                    &frameSlotFence);
        if (vulkanResult != VK_SUCCESS) {
            for (VkFence createdFence : frameSlotFences) {
                vkDestroyFence(vulkanContext.logicalDevice(), createdFence, nullptr);
            }
            return std::unexpected(VulkanFrameLoopFailure{
                FenceCreationFailed, frameSlot, static_cast<std::int32_t>(vulkanResult)});
        }
        frameSlotFences.push_back(frameSlotFence);
    }

    return VulkanFrameLoop{vulkanContext.logicalDevice(), vulkanContext.graphicsQueue(),
                           std::move(frameSlotFences)};
}

VulkanFrameLoop::~VulkanFrameLoop() {
    if (m_frameSlotFences.empty()) {
        return; // Moved-from shell.
    }
    // Drain before destroying so in-flight submissions cannot outlive their fences.
    vkQueueWaitIdle(m_submissionQueue);
    for (VkFence frameSlotFence : m_frameSlotFences) {
        vkDestroyFence(m_logicalDevice, frameSlotFence, nullptr);
    }
}

std::expected<std::uint32_t, VulkanFrameLoopFailure> VulkanFrameLoop::beginFrame() {
    using enum VulkanFrameLoopError;

    if (m_isFrameOpen) {
        return std::unexpected(
            VulkanFrameLoopFailure{FrameAlreadyOpen, m_currentFrameSlot, 0});
    }

    VkResult vulkanResult =
        vkWaitForFences(m_logicalDevice, 1u, &m_frameSlotFences[m_currentFrameSlot],
                        VK_TRUE, UINT64_MAX);
    if (vulkanResult != VK_SUCCESS) {
        return std::unexpected(VulkanFrameLoopFailure{
            FenceWaitFailed, m_currentFrameSlot, static_cast<std::int32_t>(vulkanResult)});
    }
    vulkanResult =
        vkResetFences(m_logicalDevice, 1u, &m_frameSlotFences[m_currentFrameSlot]);
    if (vulkanResult != VK_SUCCESS) {
        return std::unexpected(VulkanFrameLoopFailure{
            FenceResetFailed, m_currentFrameSlot, static_cast<std::int32_t>(vulkanResult)});
    }

    m_isFrameOpen = true;
    return m_currentFrameSlot;
}

std::expected<void, VulkanFrameLoopFailure>
VulkanFrameLoop::submitFrame(std::span<const VkCommandBuffer> commandBuffers) {
    using enum VulkanFrameLoopError;

    if (!m_isFrameOpen) {
        return std::unexpected(VulkanFrameLoopFailure{FrameNotOpen, m_currentFrameSlot, 0});
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = static_cast<std::uint32_t>(commandBuffers.size());
    submitInfo.pCommandBuffers = commandBuffers.data();
    const VkResult vulkanResult =
        vkQueueSubmit(m_submissionQueue, 1u, &submitInfo,
                      m_frameSlotFences[m_currentFrameSlot]);
    if (vulkanResult != VK_SUCCESS) {
        return std::unexpected(VulkanFrameLoopFailure{
            SubmissionFailed, m_currentFrameSlot, static_cast<std::int32_t>(vulkanResult)});
    }

    m_isFrameOpen = false;
    m_currentFrameSlot = (m_currentFrameSlot + 1u) % framesInFlightCount();
    return {};
}

} // namespace barrieww
