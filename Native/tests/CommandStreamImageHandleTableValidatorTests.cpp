#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamHandleTableValidationError.hpp"
#include "BarriEww/CommandStream/CommandStreamImageHandleTableValidator.hpp"

using barrieww::CommandStreamHandleTableValidationError;
using barrieww::CommandStreamImageHandleTableValidator;

namespace {

/** Appends one little-endian value to a byte vector. */
template <typename ValueType>
void appendValue(std::vector<std::byte>& targetBytes, ValueType value) {
    const std::size_t writeOffset = targetBytes.size();
    targetBytes.resize(writeOffset + sizeof value);
    std::memcpy(targetBytes.data() + writeOffset, &value, sizeof value);
}

struct RawImageEntry {
    std::uint32_t imageKindValue = 2;    // TwoDimensional
    std::uint32_t formatValue = 1;       // R8G8B8A8Unorm
    std::uint32_t width = 8;
    std::uint32_t height = 8;
    std::uint32_t depth = 1;
    std::uint32_t mipLevelCount = 1;
    std::uint32_t arrayLayerCount = 1;
    std::uint32_t sampleCountValue = 1;
    std::uint32_t usageFlags = 0x3;      // TransferSource | TransferDestination
    std::uint32_t importIdentifier = 0;
};

std::vector<std::byte> buildTable(const std::vector<RawImageEntry>& rawEntries) {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, static_cast<std::uint32_t>(rawEntries.size()));
    appendValue(tableBytes, std::uint32_t{0});
    for (const RawImageEntry& rawEntry : rawEntries) {
        appendValue(tableBytes, rawEntry.imageKindValue);
        appendValue(tableBytes, rawEntry.formatValue);
        appendValue(tableBytes, rawEntry.width);
        appendValue(tableBytes, rawEntry.height);
        appendValue(tableBytes, rawEntry.depth);
        appendValue(tableBytes, rawEntry.mipLevelCount);
        appendValue(tableBytes, rawEntry.arrayLayerCount);
        appendValue(tableBytes, rawEntry.sampleCountValue);
        appendValue(tableBytes, rawEntry.usageFlags);
        appendValue(tableBytes, rawEntry.importIdentifier);
    }
    return tableBytes;
}

} // namespace

TEST_CASE("Image table validator accepts created and imported entries",
          "[commandStream][imageHandleTable]") {
    RawImageEntry importedEntry{};
    importedEntry.usageFlags = 0;
    importedEntry.importIdentifier = 2002;
    const std::vector<std::byte> tableBytes = buildTable({RawImageEntry{}, importedEntry});

    const auto validationResult =
        CommandStreamImageHandleTableValidator::validate(tableBytes);
    REQUIRE(validationResult.has_value());
    REQUIRE(validationResult->entryCount() == 2u);

    const auto createdEntry = validationResult->entry(0u);
    REQUIRE(createdEntry.width == 8u);
    REQUIRE(createdEntry.formatValue == 1u);
    REQUIRE_FALSE(createdEntry.isImported());
    REQUIRE(validationResult->entry(1u).isImported());
}

TEST_CASE("Image table validator rejects malformed tables with the precise failure",
          "[commandStream][imageHandleTable]") {
    SECTION("unassigned image kind") {
        RawImageEntry badEntry{};
        badEntry.imageKindValue = 9;
        const auto validationResult =
            CommandStreamImageHandleTableValidator::validate(buildTable({badEntry}));
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::UnknownImageKind);
    }

    SECTION("unassigned format") {
        RawImageEntry badEntry{};
        badEntry.formatValue = 77;
        const auto validationResult =
            CommandStreamImageHandleTableValidator::validate(buildTable({badEntry}));
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::UnknownImageFormat);
    }

    SECTION("non-power-of-two sample count") {
        RawImageEntry badEntry{};
        badEntry.sampleCountValue = 3;
        const auto validationResult =
            CommandStreamImageHandleTableValidator::validate(buildTable({badEntry}));
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::UnknownSampleCount);
    }

    SECTION("zero width") {
        RawImageEntry badEntry{};
        badEntry.width = 0;
        const auto validationResult =
            CommandStreamImageHandleTableValidator::validate(buildTable({badEntry}));
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::InvalidImageDimensions);
    }

    SECTION("3D image with multiple array layers") {
        RawImageEntry badEntry{};
        badEntry.imageKindValue = 3;
        badEntry.depth = 4;
        badEntry.arrayLayerCount = 2;
        const auto validationResult =
            CommandStreamImageHandleTableValidator::validate(buildTable({badEntry}));
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::InvalidImageDimensions);
    }

    SECTION("unassigned usage bit") {
        RawImageEntry badEntry{};
        badEntry.usageFlags = 0x8000;
        const auto validationResult =
            CommandStreamImageHandleTableValidator::validate(buildTable({badEntry}));
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::UnknownUsageFlags);
    }

    SECTION("created entry with empty usage") {
        RawImageEntry badEntry{};
        badEntry.usageFlags = 0;
        const auto validationResult =
            CommandStreamImageHandleTableValidator::validate(buildTable({badEntry}));
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::InvalidCreatedImageDescription);
    }

    SECTION("size not matching the entry count") {
        std::vector<std::byte> tableBytes = buildTable({RawImageEntry{}});
        tableBytes.resize(tableBytes.size() + 8u);
        const auto validationResult =
            CommandStreamImageHandleTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::TableSizeMismatch);
    }
}
