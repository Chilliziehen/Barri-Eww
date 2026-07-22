#include "BarriEww/Interoperability/NativeCommandStreamModuleValidationBoundary.hpp"

#include "BarriEww/CommandStream/CommandStreamModuleValidator.hpp"
#include "NativeCommandStreamModuleValidationOperation.hpp"

extern "C" barrieww::NativeCommandStreamModuleValidationOperationResult
barriEwwValidateCommandStreamModuleVersion1(
    const void* moduleBytes,
    std::uint64_t moduleByteSize,
    barrieww::NativeCommandStreamModuleValidationStatus* validationStatus) noexcept {
    return barrieww::interoperability::executeNativeCommandStreamModuleValidationVersion1(
        moduleBytes, moduleByteSize, validationStatus,
        &barrieww::CommandStreamModuleValidator::validate);
}
