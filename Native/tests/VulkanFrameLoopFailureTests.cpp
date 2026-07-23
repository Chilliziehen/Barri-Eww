#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <cstddef>

#include <vulkan/vulkan.h>

#include "BarriEww/Vulkan/VulkanContext.hpp"
#include "BarriEww/Vulkan/VulkanFrameLoop.hpp"
#include "BarriEww/Vulkan/VulkanFrameLoopError.hpp"

using barrieww::VulkanContext;
using barrieww::VulkanContextCreateInfo;
using barrieww::VulkanFrameLoop;
using barrieww::VulkanFrameLoopError;

namespace {

std::uint32_t g_createFenceCallCount = 0;
std::uint32_t g_destroyFenceCallCount = 0;
VkResult g_createFenceResult = VK_SUCCESS;
std::uint32_t g_createFenceFailureCall = UINT32_MAX;
VkResult g_waitResult = VK_SUCCESS;
VkResult g_resetResult = VK_SUCCESS;
VkResult g_submitResult = VK_SUCCESS;

/** Resets deterministic Vulkan call results and call counters. */
void resetVulkanCallState() {
    g_createFenceCallCount = 0u;
    g_destroyFenceCallCount = 0u;
    g_createFenceResult = VK_SUCCESS;
    g_createFenceFailureCall = UINT32_MAX;
    g_waitResult = VK_SUCCESS;
    g_resetResult = VK_SUCCESS;
    g_submitResult = VK_SUCCESS;
}

/** Creates a context from non-null opaque handles consumed only by local replacements.
 * @return VulkanContext Borrowed-handle context for the isolated failure executable
 */
VulkanContext makeVulkanContext() {
    VulkanContextCreateInfo createInfo{};
    createInfo.instance = reinterpret_cast<VkInstance>(1u);
    createInfo.physicalDevice = reinterpret_cast<VkPhysicalDevice>(2u);
    createInfo.logicalDevice = reinterpret_cast<VkDevice>(3u);
    createInfo.queueFamilyIndices.graphicsFamily = 0u;
    createInfo.queueFamilyIndices.presentFamily = 0u;
    createInfo.graphicsQueue = reinterpret_cast<VkQueue>(4u);
    createInfo.presentQueue = reinterpret_cast<VkQueue>(4u);
    return VulkanContext{createInfo};
}

} // namespace

/**
 * Deterministic link-time replacement for fence creation.
 * @param VkDevice device Opaque device handle
 * @param const VkFenceCreateInfo* createInfo Requested fence description
 * @param const VkAllocationCallbacks* allocationCallbacks Optional allocator callbacks
 * @param VkFence* fence Writable created fence handle
 * @return VkResult Configured failure or success
 */
extern "C" VkResult VKAPI_CALL vkCreateFence(
    VkDevice device, const VkFenceCreateInfo* createInfo,
    const VkAllocationCallbacks* allocationCallbacks, VkFence* fence) {
    static_cast<void>(device);
    static_cast<void>(createInfo);
    static_cast<void>(allocationCallbacks);
    const std::uint32_t callIndex = g_createFenceCallCount++;
    if (callIndex == g_createFenceFailureCall) {
        return g_createFenceResult;
    }
    *fence = reinterpret_cast<VkFence>(static_cast<std::uintptr_t>(100u + callIndex));
    return VK_SUCCESS;
}

/**
 * Deterministic link-time replacement recording fence destruction.
 * @param VkDevice device Opaque device handle
 * @param VkFence fence Fence being destroyed
 * @param const VkAllocationCallbacks* allocationCallbacks Optional allocator callbacks
 */
extern "C" void VKAPI_CALL vkDestroyFence(
    VkDevice device, VkFence fence, const VkAllocationCallbacks* allocationCallbacks) {
    static_cast<void>(device);
    static_cast<void>(fence);
    static_cast<void>(allocationCallbacks);
    ++g_destroyFenceCallCount;
}

/**
 * Deterministic link-time replacement for fence waiting.
 * @param VkDevice device Opaque device handle
 * @param std::uint32_t fenceCount Number of fences
 * @param const VkFence* fences Fence array
 * @param VkBool32 waitAll Whether every fence is required
 * @param std::uint64_t timeout Wait timeout
 * @return VkResult Configured wait result
 */
extern "C" VkResult VKAPI_CALL vkWaitForFences(
    VkDevice device, std::uint32_t fenceCount, const VkFence* fences,
    VkBool32 waitAll, std::uint64_t timeout) {
    static_cast<void>(device);
    static_cast<void>(fenceCount);
    static_cast<void>(fences);
    static_cast<void>(waitAll);
    static_cast<void>(timeout);
    return g_waitResult;
}

/**
 * Deterministic link-time replacement for fence reset.
 * @param VkDevice device Opaque device handle
 * @param std::uint32_t fenceCount Number of fences
 * @param const VkFence* fences Fence array
 * @return VkResult Configured reset result
 */
extern "C" VkResult VKAPI_CALL vkResetFences(
    VkDevice device, std::uint32_t fenceCount, const VkFence* fences) {
    static_cast<void>(device);
    static_cast<void>(fenceCount);
    static_cast<void>(fences);
    return g_resetResult;
}

/**
 * Deterministic link-time replacement for queue submission.
 * @param VkQueue queue Opaque queue handle
 * @param std::uint32_t submitCount Number of submissions
 * @param const VkSubmitInfo* submitInfo Submission descriptions
 * @param VkFence fence Signalled fence
 * @return VkResult Configured submission result
 */
extern "C" VkResult VKAPI_CALL vkQueueSubmit(
    VkQueue queue, std::uint32_t submitCount, const VkSubmitInfo* submitInfo,
    VkFence fence) {
    static_cast<void>(queue);
    static_cast<void>(submitCount);
    static_cast<void>(submitInfo);
    static_cast<void>(fence);
    return g_submitResult;
}

/**
 * Deterministic link-time replacement for queue draining.
 * @param VkQueue queue Opaque queue handle
 * @return VkResult Always successful in the isolated failure executable
 */
extern "C" VkResult VKAPI_CALL vkQueueWaitIdle(VkQueue queue) {
    static_cast<void>(queue);
    return VK_SUCCESS;
}

TEST_CASE("Frame loop reports fence creation failure and rolls back prior fences",
          "[vulkanFrameLoop][failure]") {
    resetVulkanCallState();
    g_createFenceFailureCall = 2u;
    g_createFenceResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    const VulkanContext vulkanContext = makeVulkanContext();

    const auto frameLoopResult = VulkanFrameLoop::create(vulkanContext, 4u);

    REQUIRE_FALSE(frameLoopResult.has_value());
    REQUIRE(frameLoopResult.error().error == VulkanFrameLoopError::FenceCreationFailed);
    REQUIRE(frameLoopResult.error().frameSlot == 2u);
    REQUIRE(frameLoopResult.error().resultValue == VK_ERROR_OUT_OF_HOST_MEMORY);
    REQUIRE(g_destroyFenceCallCount == 2u);
}

TEST_CASE("Frame loop reports wait reset and submission Vulkan failures",
          "[vulkanFrameLoop][failure]") {
    const VulkanContext vulkanContext = makeVulkanContext();

    SECTION("fence wait failure") {
        resetVulkanCallState();
        g_waitResult = VK_ERROR_DEVICE_LOST;
        auto frameLoopResult = VulkanFrameLoop::create(vulkanContext, 1u);
        REQUIRE(frameLoopResult.has_value());
        const auto beginResult = frameLoopResult->beginFrame();
        REQUIRE_FALSE(beginResult.has_value());
        REQUIRE(beginResult.error().error == VulkanFrameLoopError::FenceWaitFailed);
        REQUIRE(beginResult.error().frameSlot == 0u);
        REQUIRE(beginResult.error().resultValue == VK_ERROR_DEVICE_LOST);
    }

    SECTION("fence reset failure") {
        resetVulkanCallState();
        g_resetResult = VK_ERROR_DEVICE_LOST;
        auto frameLoopResult = VulkanFrameLoop::create(vulkanContext, 1u);
        REQUIRE(frameLoopResult.has_value());
        const auto beginResult = frameLoopResult->beginFrame();
        REQUIRE_FALSE(beginResult.has_value());
        REQUIRE(beginResult.error().error == VulkanFrameLoopError::FenceResetFailed);
        REQUIRE(beginResult.error().frameSlot == 0u);
        REQUIRE(beginResult.error().resultValue == VK_ERROR_DEVICE_LOST);
    }

    SECTION("submission failure") {
        resetVulkanCallState();
        g_submitResult = VK_ERROR_DEVICE_LOST;
        auto frameLoopResult = VulkanFrameLoop::create(vulkanContext, 1u);
        REQUIRE(frameLoopResult.has_value());
        REQUIRE(frameLoopResult->beginFrame().has_value());
        const auto submitResult = frameLoopResult->submitFrame({});
        REQUIRE_FALSE(submitResult.has_value());
        REQUIRE(submitResult.error().error == VulkanFrameLoopError::SubmissionFailed);
        REQUIRE(submitResult.error().frameSlot == 0u);
        REQUIRE(submitResult.error().resultValue == VK_ERROR_DEVICE_LOST);
    }
}
