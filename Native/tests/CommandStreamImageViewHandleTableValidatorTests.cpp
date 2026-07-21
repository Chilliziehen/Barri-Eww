#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamHandleTableValidationError.hpp"
#include "BarriEww/CommandStream/CommandStreamImageFormat.hpp"
#include "BarriEww/CommandStream/CommandStreamImageViewHandleTableValidator.hpp"

using barrieww::CommandStreamHandleTableValidationError;
using barrieww::CommandStreamImageFormat;
using barrieww::CommandStreamImageViewHandleTableValidator;
using barrieww::isCompatibleCommandStreamImageAspectMask;

namespace {

/** Appends one little-endian value to a byte vector. */
template <typename ValueType>
void appendValue(std::vector<std::byte>& targetBytes, ValueType value) {
    const std::size_t writeOffset = targetBytes.size();
    targetBytes.resize(writeOffset + sizeof value);
    std::memcpy(targetBytes.data() + writeOffset, &value, sizeof value);
}

struct RawImageViewEntry {
    std::uint32_t imageSlot = 3;
    std::uint32_t imageViewKindValue = 2; // TwoDimensional
    std::uint32_t formatValue = 1;        // R8G8B8A8Unorm
    std::uint32_t aspectMaskValue = 0x1;  // Color
    std::uint32_t baseMipLevel = 0;
    std::uint32_t mipLevelCount = 1;
    std::uint32_t baseArrayLayer = 0;
    std::uint32_t arrayLayerCount = 1;
};

std::vector<std::byte> buildTable(const std::vector<RawImageViewEntry>& rawEntries,
                                  std::uint32_t headerReservedFlags = 0u) {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, static_cast<std::uint32_t>(rawEntries.size()));
    appendValue(tableBytes, headerReservedFlags);
    for (const RawImageViewEntry& rawEntry : rawEntries) {
        appendValue(tableBytes, rawEntry.imageSlot);
        appendValue(tableBytes, rawEntry.imageViewKindValue);
        appendValue(tableBytes, rawEntry.formatValue);
        appendValue(tableBytes, rawEntry.aspectMaskValue);
        appendValue(tableBytes, rawEntry.baseMipLevel);
        appendValue(tableBytes, rawEntry.mipLevelCount);
        appendValue(tableBytes, rawEntry.baseArrayLayer);
        appendValue(tableBytes, rawEntry.arrayLayerCount);
    }
    return tableBytes;
}

} // namespace

TEST_CASE("Image-view table validator accepts and decodes v0.1 entries",
          "[commandStream][imageViewHandleTable]") {
    RawImageViewEntry threeDimensionalEntry{};
    threeDimensionalEntry.imageSlot = 9u;
    threeDimensionalEntry.imageViewKindValue = 3u;
    threeDimensionalEntry.formatValue = 5u;
    threeDimensionalEntry.aspectMaskValue = 0x2u;
    threeDimensionalEntry.baseMipLevel = 1u;
    threeDimensionalEntry.mipLevelCount = 2u;
    const std::vector<std::byte> tableBytes =
        buildTable({RawImageViewEntry{}, threeDimensionalEntry});

    const auto validationResult =
        CommandStreamImageViewHandleTableValidator::validate(tableBytes);
    REQUIRE(validationResult.has_value());
    REQUIRE(validationResult->entryCount() == 2u);
    const auto entry = validationResult->entry(1u);
    REQUIRE(entry.imageSlot == 9u);
    REQUIRE(entry.imageViewKindValue == 3u);
    REQUIRE(entry.formatValue == 5u);
    REQUIRE(entry.aspectMaskValue == 0x2u);
    REQUIRE(entry.baseMipLevel == 1u);
    REQUIRE(entry.mipLevelCount == 2u);
    REQUIRE(entry.baseArrayLayer == 0u);
    REQUIRE(entry.arrayLayerCount == 1u);
}

TEST_CASE("Image format aspect compatibility accepts legal combined-format subsets",
          "[commandStream][imageViewHandleTable]") {
    REQUIRE(isCompatibleCommandStreamImageAspectMask(
        CommandStreamImageFormat::D24UnormS8Uint, 0x2u));
    REQUIRE(isCompatibleCommandStreamImageAspectMask(
        CommandStreamImageFormat::D24UnormS8Uint, 0x4u));
    REQUIRE(isCompatibleCommandStreamImageAspectMask(
        CommandStreamImageFormat::D24UnormS8Uint, 0x6u));
    REQUIRE_FALSE(isCompatibleCommandStreamImageAspectMask(
        CommandStreamImageFormat::R8G8B8A8Unorm, 0x2u));
}

TEST_CASE("Image-view table validator rejects malformed tables with the precise failure",
          "[commandStream][imageViewHandleTable]") {
    SECTION("table is too small") {
        const std::vector<std::byte> tableBytes(7u);
        const auto validationResult =
            CommandStreamImageViewHandleTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::TableTooSmall);
    }

    SECTION("header reserved flags are nonzero") {
        const auto validationResult =
            CommandStreamImageViewHandleTableValidator::validate(buildTable({}, 1u));
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::NonZeroReservedFlags);
    }

    SECTION("table size does not match entry count") {
        std::vector<std::byte> tableBytes = buildTable({RawImageViewEntry{}});
        tableBytes.resize(tableBytes.size() + 8u);
        const auto validationResult =
            CommandStreamImageViewHandleTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::TableSizeMismatch);
    }

    SECTION("image-view kind is unassigned") {
        RawImageViewEntry badEntry{};
        badEntry.imageViewKindValue = 7u;
        const auto validationResult =
            CommandStreamImageViewHandleTableValidator::validate(buildTable({badEntry}));
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::UnknownImageViewKind);
        REQUIRE(validationResult.error().byteOffset == 8u);
    }

    SECTION("image format is unassigned") {
        RawImageViewEntry badEntry{};
        badEntry.formatValue = 77u;
        const auto validationResult =
            CommandStreamImageViewHandleTableValidator::validate(buildTable({badEntry}));
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::UnknownImageFormat);
    }

    SECTION("aspect mask is empty") {
        RawImageViewEntry badEntry{};
        badEntry.aspectMaskValue = 0u;
        const auto validationResult =
            CommandStreamImageViewHandleTableValidator::validate(buildTable({badEntry}));
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::UnusableImageAspectMask);
    }

    SECTION("aspect mask has an unassigned bit") {
        RawImageViewEntry badEntry{};
        badEntry.aspectMaskValue = 0x80u;
        const auto validationResult =
            CommandStreamImageViewHandleTableValidator::validate(buildTable({badEntry}));
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::UnusableImageAspectMask);
    }

    SECTION("mip count is zero") {
        RawImageViewEntry badEntry{};
        badEntry.mipLevelCount = 0u;
        const auto validationResult =
            CommandStreamImageViewHandleTableValidator::validate(buildTable({badEntry}));
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::InvalidImageViewSubresourceCounts);
    }

    SECTION("v0.1 rejects remaining-range mip sentinel") {
        RawImageViewEntry badEntry{};
        badEntry.mipLevelCount = 0xFFFFFFFFu;
        const auto validationResult =
            CommandStreamImageViewHandleTableValidator::validate(buildTable({badEntry}));
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::InvalidImageViewSubresourceCounts);
    }

    SECTION("3D view starts after array layer zero") {
        RawImageViewEntry badEntry{};
        badEntry.imageViewKindValue = 3u;
        badEntry.baseArrayLayer = 1u;
        const auto validationResult =
            CommandStreamImageViewHandleTableValidator::validate(buildTable({badEntry}));
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::InvalidImageViewSubresourceCounts);
    }

    SECTION("v0.1 rejects array views") {
        RawImageViewEntry badEntry{};
        badEntry.arrayLayerCount = 2u;
        const auto validationResult =
            CommandStreamImageViewHandleTableValidator::validate(buildTable({badEntry}));
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::InvalidImageViewSubresourceCounts);
    }
}
