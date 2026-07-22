#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamRenderingTemplateTableValidationError.hpp"
#include "BarriEww/CommandStream/CommandStreamRenderingTemplateTableValidator.hpp"

using barrieww::CommandStreamRenderingTemplateTableValidationError;
using barrieww::CommandStreamRenderingTemplateTableValidator;

namespace {

/** Appends one little-endian value to a byte vector. */
template <typename ValueType>
void appendValue(std::vector<std::byte>& targetBytes, ValueType value) {
    const std::size_t writeOffset = targetBytes.size();
    targetBytes.resize(writeOffset + sizeof value);
    std::memcpy(targetBytes.data() + writeOffset, &value, sizeof value);
}

/** Appends one 32-byte attachment record. */
void appendAttachmentRecord(std::vector<std::byte>& tableBytes, std::uint32_t imageViewSlot,
                            std::uint32_t imageLayoutValue, std::uint32_t loadOpValue,
                            std::uint32_t storeOpValue) {
    appendValue(tableBytes, imageViewSlot);
    appendValue(tableBytes, imageLayoutValue);
    appendValue(tableBytes, loadOpValue);
    appendValue(tableBytes, storeOpValue);
    appendValue(tableBytes, 1.0f);
    appendValue(tableBytes, 0.0f);
    appendValue(tableBytes, 1.0f);
    appendValue(tableBytes, 1.0f);
}

/**
 * A one-template table: one color attachment (ColorAttachment layout, Clear/Store) at
 * 8x8. Directory ends at 8 + 40 = 48; attachment region at 48 (one 32-byte record);
 * total 80.
 */
std::vector<std::byte> makeSingleColorTemplateTable(std::uint32_t colorLayoutValue = 2u,
                                                    std::uint32_t loadOpValue = 1u,
                                                    std::uint32_t storeOpValue = 0u,
                                                    std::uint32_t renderAreaWidth = 8u,
                                                    std::uint32_t layerCount = 1u,
                                                    std::uint32_t depthPresent = 0u) {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{1}); // templateCount
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{1}); // colorAttachmentCount
    appendValue(tableBytes, depthPresent);
    appendValue(tableBytes, std::int32_t{0});  // renderAreaOffsetX
    appendValue(tableBytes, std::int32_t{0});  // renderAreaOffsetY
    appendValue(tableBytes, renderAreaWidth);
    appendValue(tableBytes, std::uint32_t{8}); // renderAreaHeight
    appendValue(tableBytes, layerCount);
    appendValue(tableBytes, std::uint32_t{0}); // viewMask
    appendValue(tableBytes, std::uint64_t{48}); // attachmentsByteOffset
    appendAttachmentRecord(tableBytes, 0u, colorLayoutValue, loadOpValue, storeOpValue);
    return tableBytes;
}

} // namespace

TEST_CASE("Rendering template table validator accepts and decodes a color template",
          "[commandStream][renderingTemplateTable]") {
    const std::vector<std::byte> tableBytes = makeSingleColorTemplateTable();
    REQUIRE(tableBytes.size() == 80u);

    const auto validationResult =
        CommandStreamRenderingTemplateTableValidator::validate(tableBytes);
    REQUIRE(validationResult.has_value());
    REQUIRE(validationResult->templateCount() == 1u);

    const auto record = validationResult->templateRecord(0u);
    REQUIRE(record.colorAttachmentCount == 1u);
    REQUIRE(record.depthAttachmentPresent == 0u);
    REQUIRE(record.renderAreaWidth == 8u);
    REQUIRE(record.layerCount == 1u);

    const auto attachment = validationResult->attachmentRecord(0u, 0u);
    REQUIRE(attachment.imageViewSlot == 0u);
    REQUIRE(attachment.imageLayoutValue == 2u); // ColorAttachment
    REQUIRE(attachment.loadOpValue == 1u);      // Clear
    REQUIRE(attachment.storeOpValue == 0u);     // Store
    REQUIRE(attachment.clearValue[0] == 1.0f);
    REQUIRE(attachment.clearValue[2] == 1.0f);
}

TEST_CASE("Rendering template table validator accepts a color plus depth template",
          "[commandStream][renderingTemplateTable]") {
    // Directory ends at 48; two 32-byte records (color + depth) at 48; total 112.
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{1}); // colorAttachmentCount
    appendValue(tableBytes, std::uint32_t{1}); // depthAttachmentPresent
    appendValue(tableBytes, std::int32_t{0});
    appendValue(tableBytes, std::int32_t{0});
    appendValue(tableBytes, std::uint32_t{16});
    appendValue(tableBytes, std::uint32_t{16});
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint64_t{48});
    appendAttachmentRecord(tableBytes, 0u, 2u, 1u, 0u); // color
    appendAttachmentRecord(tableBytes, 1u, 3u, 1u, 0u); // depth (DepthStencilAttachment)
    REQUIRE(tableBytes.size() == 112u);

    const auto validationResult =
        CommandStreamRenderingTemplateTableValidator::validate(tableBytes);
    REQUIRE(validationResult.has_value());
    const auto depthAttachment = validationResult->attachmentRecord(0u, 1u);
    REQUIRE(depthAttachment.imageViewSlot == 1u);
    REQUIRE(depthAttachment.imageLayoutValue == 3u);
}

TEST_CASE("Rendering template table validator rejects malformed tables with the precise failure",
          "[commandStream][renderingTemplateTable]") {
    SECTION("directory that does not fit") {
        std::vector<std::byte> tableBytes;
        appendValue(tableBytes, std::uint32_t{3});
        appendValue(tableBytes, std::uint32_t{0});
        const auto validationResult =
            CommandStreamRenderingTemplateTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamRenderingTemplateTableValidationError::DirectoryOutOfBounds);
    }

    SECTION("non-zero header reserved flags") {
        std::vector<std::byte> tableBytes = makeSingleColorTemplateTable();
        const std::uint32_t nonZeroFlags = 9;
        std::memcpy(tableBytes.data() + 4u, &nonZeroFlags, sizeof nonZeroFlags);
        const auto validationResult =
            CommandStreamRenderingTemplateTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamRenderingTemplateTableValidationError::NonZeroReservedFlags);
    }

    SECTION("invalid depth attachment flag") {
        std::vector<std::byte> tableBytes = makeSingleColorTemplateTable();
        const std::uint32_t badDepthFlag = 2;
        // depthAttachmentPresent lives at 8 + 4 = 12.
        std::memcpy(tableBytes.data() + 12u, &badDepthFlag, sizeof badDepthFlag);
        const auto validationResult =
            CommandStreamRenderingTemplateTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamRenderingTemplateTableValidationError::InvalidDepthAttachmentFlag);
    }

    SECTION("empty render area") {
        const std::vector<std::byte> tableBytes =
            makeSingleColorTemplateTable(2u, 1u, 0u, /*renderAreaWidth=*/0u);
        const auto validationResult =
            CommandStreamRenderingTemplateTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamRenderingTemplateTableValidationError::EmptyRenderArea);
    }

    SECTION("zero layer count") {
        const std::vector<std::byte> tableBytes =
            makeSingleColorTemplateTable(2u, 1u, 0u, 8u, /*layerCount=*/0u);
        const auto validationResult =
            CommandStreamRenderingTemplateTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamRenderingTemplateTableValidationError::ZeroLayerCount);
    }

    SECTION("attachment region leaving the table") {
        std::vector<std::byte> tableBytes = makeSingleColorTemplateTable();
        // attachmentsByteOffset lives at 8 + 32 = 40; push it past the table end.
        const std::uint64_t overrunOffset = 4096;
        std::memcpy(tableBytes.data() + 40u, &overrunOffset, sizeof overrunOffset);
        const auto validationResult =
            CommandStreamRenderingTemplateTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamRenderingTemplateTableValidationError::AttachmentRegionOutOfBounds);
    }

    SECTION("unassigned attachment load op") {
        const std::vector<std::byte> tableBytes =
            makeSingleColorTemplateTable(2u, /*loadOpValue=*/9u);
        const auto validationResult =
            CommandStreamRenderingTemplateTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamRenderingTemplateTableValidationError::UnknownAttachmentLoadOp);
    }

    SECTION("unassigned attachment layout") {
        const std::vector<std::byte> tableBytes =
            makeSingleColorTemplateTable(/*colorLayoutValue=*/99u);
        const auto validationResult =
            CommandStreamRenderingTemplateTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamRenderingTemplateTableValidationError::UnknownAttachmentLayout);
    }
}
