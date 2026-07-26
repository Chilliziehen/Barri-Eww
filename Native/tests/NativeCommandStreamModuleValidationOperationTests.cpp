#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <expected>
#include <span>
#include <stdexcept>

#include "BarriEww/CommandStream/CommandStreamModuleValidationFailure.hpp"
#include "BarriEww/CommandStream/CommandStreamModuleView.hpp"
#include "BarriEww/Interoperability/NativeCommandStreamModuleValidationBoundary.hpp"
#include "../src/Interoperability/NativeCommandStreamModuleValidationOperation.hpp"

namespace {

/** Throws to prove that the operation contains unexpected validator exceptions. */
std::expected<barrieww::CommandStreamModuleView,
              barrieww::CommandStreamModuleValidationFailure>
throwUnexpectedValidationFailure(std::span<const std::byte> moduleBytes) {
    static_cast<void>(moduleBytes);
    throw std::runtime_error("injected validator failure");
}

} // namespace

TEST_CASE("Version 1 operation rejects a null validation function",
          "[ffmOperation]") {
    alignas(8) const std::byte moduleBytes[32]{};
    barrieww::NativeCommandStreamModuleValidationStatus validationStatus{
        0xFFFFFFFFu, 0xFFFFFFFFu, UINT64_MAX, UINT64_MAX};

    const auto operationResult =
        barrieww::interoperability::executeNativeCommandStreamModuleValidationVersion1(
            moduleBytes, sizeof moduleBytes, &validationStatus, nullptr);

    REQUIRE(operationResult
            == barrieww::NativeCommandStreamModuleValidationOperationResult::InvalidArgument);
    REQUIRE(validationStatus.moduleValidationErrorCode == 0u);
    REQUIRE(validationStatus.laneStreamValidationErrorCode == 0u);
    REQUIRE(validationStatus.moduleByteOffset == 0u);
    REQUIRE(validationStatus.laneStreamByteOffset == 0u);
}

TEST_CASE("Version 1 operation contains unexpected validator exceptions",
          "[ffmOperation]") {
    alignas(8) const std::byte moduleBytes[32]{};
    barrieww::NativeCommandStreamModuleValidationStatus validationStatus{
        0xFFFFFFFFu, 0xFFFFFFFFu, UINT64_MAX, UINT64_MAX};

    const auto operationResult =
        barrieww::interoperability::executeNativeCommandStreamModuleValidationVersion1(
            moduleBytes, sizeof moduleBytes, &validationStatus,
            &throwUnexpectedValidationFailure);

    REQUIRE(operationResult
            == barrieww::NativeCommandStreamModuleValidationOperationResult::InternalFailure);
    REQUIRE(validationStatus.moduleValidationErrorCode == 0u);
    REQUIRE(validationStatus.laneStreamValidationErrorCode == 0u);
    REQUIRE(validationStatus.moduleByteOffset == 0u);
    REQUIRE(validationStatus.laneStreamByteOffset == 0u);
}
