#pragma once

#include <utility>

#include "BarriEww/Interoperability/NativePresentationRuntimeBoundary.hpp"

namespace barrieww::interoperability {

/**
 * @note ThreadSafety: Thread-confined to the presentation thread of the supplied operation.
 * @brief Executes one host-runtime creation operation and contains every exception.
 * @param NativePresentationRuntimeCreateResultVersion1* createResult Writable non-null result
 *        that was cleared by the boundary
 * @param CreateOperation&& createOperation Complete validated creation operation
 * @return NativePresentationRuntimeOperationResult Operation result or InternalFailure
 * @warning MemoryOwnership: The caller owns createResult and captured operation state; this
 *          function writes synchronously and retains nothing.
 */
template <typename CreateOperation>
NativePresentationRuntimeOperationResult executeNativePresentationCreateOperation(
    NativePresentationRuntimeCreateResultVersion1* createResult,
    CreateOperation&& createOperation) noexcept {
    try {
        return std::forward<CreateOperation>(createOperation)();
    } catch (...) {
        *createResult = {};
        return NativePresentationRuntimeOperationResult::InternalFailure;
    }
}

/**
 * @note ThreadSafety: Thread-confined to the presentation thread of the supplied operation.
 * @brief Executes one host-resource detach operation, preserves its raw Vulkan failure,
 *        and contains all exceptions.
 * @param NativePresentationDetachHostImageResourcesResultVersion1* detachResult Writable
 *        non-null result that was cleared by the boundary
 * @param DetachOperation&& detachOperation Operation returning expected<void, VkResult>
 * @return NativePresentationRuntimeOperationResult Success, VulkanFailure, or InternalFailure
 * @warning MemoryOwnership: The caller owns detachResult and captured operation state; this
 *          function writes synchronously and retains nothing.
 */
template <typename DetachOperation>
NativePresentationRuntimeOperationResult executeNativePresentationDetachOperation(
    NativePresentationDetachHostImageResourcesResultVersion1* detachResult,
    DetachOperation&& detachOperation) noexcept {
    try {
        const auto destructionResult =
            std::forward<DetachOperation>(detachOperation)();
        if (!destructionResult.has_value()) {
            detachResult->vulkanResult = destructionResult.error();
            return NativePresentationRuntimeOperationResult::VulkanFailure;
        }
        return NativePresentationRuntimeOperationResult::Success;
    } catch (...) {
        *detachResult = {};
        return NativePresentationRuntimeOperationResult::InternalFailure;
    }
}

/**
 * @note ThreadSafety: Thread-confined to the presentation thread of the supplied operation.
 * @brief Executes one submit-frame operation, maps its result and contains all exceptions.
 * @param NativePresentationSubmitFrameResultVersion1* submitResult Writable non-null result
 * @param SubmitFrameOperation&& submitFrameOperation Operation returning a runtime submit result
 * @return NativePresentationRuntimeOperationResult Success or InternalFailure
 * @warning MemoryOwnership: The caller owns submitResult and all state captured by the
 *          operation; this function writes synchronously and retains nothing.
 */
template <typename SubmitFrameOperation>
NativePresentationRuntimeOperationResult executeNativePresentationSubmitFrameOperation(
    NativePresentationSubmitFrameResultVersion1* submitResult,
    SubmitFrameOperation&& submitFrameOperation) noexcept {
    try {
        const auto frameResult =
            std::forward<SubmitFrameOperation>(submitFrameOperation)();
        submitResult->frameStatusValue = static_cast<std::uint32_t>(frameResult.status);
        submitResult->vulkanResult = frameResult.vulkanResult;
        return NativePresentationRuntimeOperationResult::Success;
    } catch (...) {
        *submitResult = {};
        return NativePresentationRuntimeOperationResult::InternalFailure;
    }
}

} // namespace barrieww::interoperability
