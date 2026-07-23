#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamModuleHeader.hpp"
#include "BarriEww/CommandStream/CommandStreamModuleSectionEntry.hpp"
#include "BarriEww/CommandStream/CommandStreamModuleSectionType.hpp"
#include "BarriEww/CommandStream/CommandStreamModuleValidationError.hpp"
#include "BarriEww/CommandStream/CommandStreamValidationError.hpp"
#include "BarriEww/Interoperability/NativeCommandStreamModuleValidationBoundary.hpp"

using barrieww::CommandStreamModuleHeader;
using barrieww::CommandStreamModuleSectionEntry;
using barrieww::CommandStreamModuleSectionType;
using barrieww::CommandStreamModuleValidationError;
using barrieww::CommandStreamValidationError;
using barrieww::NativeCommandStreamModuleValidationOperationResult;
using barrieww::NativeCommandStreamModuleValidationStatus;

namespace {

/** Reads one complete binary fixture from the shared repository TestData directory. */
std::vector<std::byte> readTestDataFile(const char* relativeFilePath) {
    const std::string absoluteFilePath =
        std::string(BARRIEWW_TEST_DATA_DIRECTORY) + "/" + relativeFilePath;
    std::ifstream fileStream(absoluteFilePath, std::ios::binary | std::ios::ate);
    REQUIRE(fileStream.is_open());
    const std::streamsize fileByteCount = fileStream.tellg();
    fileStream.seekg(0);
    std::vector<std::byte> fileBytes(static_cast<std::size_t>(fileByteCount));
    fileStream.read(reinterpret_cast<char*>(fileBytes.data()), fileByteCount);
    return fileBytes;
}

/** Finds the byte offset of the first section with the requested assigned type. */
std::uint64_t findSectionByteOffset(
    const std::vector<std::byte>& moduleBytes,
    CommandStreamModuleSectionType sectionType) {
    CommandStreamModuleHeader moduleHeader{};
    std::memcpy(&moduleHeader, moduleBytes.data(), sizeof moduleHeader);
    for (std::uint32_t sectionEntryIndex = 0;
         sectionEntryIndex < moduleHeader.sectionCount;
         ++sectionEntryIndex) {
        CommandStreamModuleSectionEntry sectionEntry{};
        const std::size_t sectionEntryByteOffset =
            sizeof(CommandStreamModuleHeader)
            + static_cast<std::size_t>(sectionEntryIndex)
                * sizeof(CommandStreamModuleSectionEntry);
        std::memcpy(&sectionEntry, moduleBytes.data() + sectionEntryByteOffset,
                    sizeof sectionEntry);
        if (sectionEntry.sectionTypeValue == static_cast<std::uint16_t>(sectionType)) {
            return sectionEntry.byteOffset;
        }
    }
    FAIL("requested module section is absent");
    return 0u;
}

} // namespace

TEST_CASE("Version 1 boundary accepts the committed first-triangle module",
          "[ffmBoundary]") {
    const std::vector<std::byte> moduleBytes =
        readTestDataFile("CommandStream/FirstTriangleModule.becs");
    NativeCommandStreamModuleValidationStatus validationStatus{
        0xFFFFFFFFu, 0xFFFFFFFFu, UINT64_MAX, UINT64_MAX};

    const auto operationResult = barriEwwValidateCommandStreamModuleVersion1(
        moduleBytes.data(), moduleBytes.size(), &validationStatus);

    REQUIRE(operationResult == NativeCommandStreamModuleValidationOperationResult::Success);
    REQUIRE(validationStatus.moduleValidationErrorCode == 0u);
    REQUIRE(validationStatus.laneStreamValidationErrorCode == 0u);
    REQUIRE(validationStatus.moduleByteOffset == 0u);
    REQUIRE(validationStatus.laneStreamByteOffset == 0u);
}

TEST_CASE("Version 1 boundary maps module validation failure exactly",
          "[ffmBoundary]") {
    std::vector<std::byte> moduleBytes =
        readTestDataFile("CommandStream/FirstTriangleModule.becs");
    moduleBytes[0] = std::byte{'X'};
    NativeCommandStreamModuleValidationStatus validationStatus{};

    const auto operationResult = barriEwwValidateCommandStreamModuleVersion1(
        moduleBytes.data(), moduleBytes.size(), &validationStatus);

    REQUIRE(operationResult
            == NativeCommandStreamModuleValidationOperationResult::ValidationFailure);
    REQUIRE(validationStatus.moduleValidationErrorCode
            == static_cast<std::uint32_t>(
                CommandStreamModuleValidationError::InvalidModuleMagic));
    REQUIRE(validationStatus.laneStreamValidationErrorCode == 0u);
    REQUIRE(validationStatus.moduleByteOffset == 0u);
    REQUIRE(validationStatus.laneStreamByteOffset == 0u);
}

TEST_CASE("Version 1 boundary maps nested lane validation failure exactly",
          "[ffmBoundary]") {
    std::vector<std::byte> moduleBytes =
        readTestDataFile("CommandStream/FirstTriangleModule.becs");
    const std::uint64_t laneStreamByteOffset = findSectionByteOffset(
        moduleBytes, CommandStreamModuleSectionType::LaneStream);
    moduleBytes[static_cast<std::size_t>(laneStreamByteOffset)] = std::byte{'X'};
    NativeCommandStreamModuleValidationStatus validationStatus{};

    const auto operationResult = barriEwwValidateCommandStreamModuleVersion1(
        moduleBytes.data(), moduleBytes.size(), &validationStatus);

    REQUIRE(operationResult
            == NativeCommandStreamModuleValidationOperationResult::ValidationFailure);
    REQUIRE(validationStatus.moduleValidationErrorCode
            == static_cast<std::uint32_t>(
                CommandStreamModuleValidationError::LaneStreamInvalid));
    REQUIRE(validationStatus.laneStreamValidationErrorCode
            == static_cast<std::uint32_t>(CommandStreamValidationError::InvalidMagic));
    REQUIRE(validationStatus.moduleByteOffset == laneStreamByteOffset);
    REQUIRE(validationStatus.laneStreamByteOffset == 0u);
}

TEST_CASE("Version 1 boundary rejects null arguments and clears writable status",
          "[ffmBoundary]") {
    NativeCommandStreamModuleValidationStatus validationStatus{
        0xFFFFFFFFu, 0xFFFFFFFFu, UINT64_MAX, UINT64_MAX};

    REQUIRE(barriEwwValidateCommandStreamModuleVersion1(nullptr, 0u, &validationStatus)
            == NativeCommandStreamModuleValidationOperationResult::InvalidArgument);
    REQUIRE(validationStatus.moduleValidationErrorCode == 0u);
    REQUIRE(validationStatus.laneStreamValidationErrorCode == 0u);
    REQUIRE(validationStatus.moduleByteOffset == 0u);
    REQUIRE(validationStatus.laneStreamByteOffset == 0u);

    const std::vector<std::byte> moduleBytes =
        readTestDataFile("CommandStream/FirstTriangleModule.becs");
    REQUIRE(barriEwwValidateCommandStreamModuleVersion1(
                moduleBytes.data(), moduleBytes.size(), nullptr)
            == NativeCommandStreamModuleValidationOperationResult::InvalidArgument);
}

TEST_CASE("Version 1 boundary rejects lengths wider than host size",
          "[ffmBoundary]") {
    if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t)) {
        const std::byte placeholderByte{};
        NativeCommandStreamModuleValidationStatus validationStatus{};
        REQUIRE(barriEwwValidateCommandStreamModuleVersion1(
                    &placeholderByte, std::numeric_limits<std::uint64_t>::max(),
                    &validationStatus)
                == NativeCommandStreamModuleValidationOperationResult::InvalidArgument);
    } else {
        SUCCEED("Every uint64_t module size is representable by this host size_t");
    }
}
