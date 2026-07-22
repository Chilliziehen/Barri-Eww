#include "NativeCommandStreamModuleValidationOperation.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace barrieww::interoperability {

NativeCommandStreamModuleValidationOperationResult
executeNativeCommandStreamModuleValidationVersion1(
    const void* moduleBytes,
    std::uint64_t moduleByteSize,
    NativeCommandStreamModuleValidationStatus* validationStatus,
    NativeCommandStreamModuleValidationFunction validationFunction) noexcept {
    if (validationStatus == nullptr) {
        return NativeCommandStreamModuleValidationOperationResult::InvalidArgument;
    }
    *validationStatus = {};

    if (moduleBytes == nullptr || validationFunction == nullptr
        || moduleByteSize > std::numeric_limits<std::size_t>::max()) {
        return NativeCommandStreamModuleValidationOperationResult::InvalidArgument;
    }

    try {
        const auto moduleByteSpan = std::span{
            static_cast<const std::byte*>(moduleBytes),
            static_cast<std::size_t>(moduleByteSize)};
        const auto validationResult = validationFunction(moduleByteSpan);
        if (validationResult.has_value()) {
            return NativeCommandStreamModuleValidationOperationResult::Success;
        }

        const CommandStreamModuleValidationFailure& validationFailure =
            validationResult.error();
        validationStatus->moduleValidationErrorCode =
            static_cast<std::uint32_t>(validationFailure.error);
        validationStatus->moduleByteOffset = validationFailure.byteOffset;
        if (validationFailure.laneStreamFailure.has_value()) {
            validationStatus->laneStreamValidationErrorCode =
                static_cast<std::uint32_t>(validationFailure.laneStreamFailure->error);
            validationStatus->laneStreamByteOffset =
                validationFailure.laneStreamFailure->byteOffset;
        }
        return NativeCommandStreamModuleValidationOperationResult::ValidationFailure;
    } catch (...) {
        *validationStatus = {};
        return NativeCommandStreamModuleValidationOperationResult::InternalFailure;
    }
}

} // namespace barrieww::interoperability
