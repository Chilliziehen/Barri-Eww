#pragma once

#include <utility>

#include "BarriEww/Interoperability/NativePresentationRuntimeBoundary.hpp"

namespace barrieww::interoperability {

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
