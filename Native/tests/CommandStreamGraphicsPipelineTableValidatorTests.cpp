#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamGraphicsPipelineTableValidationError.hpp"
#include "BarriEww/CommandStream/CommandStreamGraphicsPipelineTableValidator.hpp"

using barrieww::CommandStreamGraphicsPipelineTableValidationError;
using barrieww::CommandStreamGraphicsPipelineTableValidator;

namespace {

/** Appends one little-endian value to a byte vector. */
template <typename ValueType>
void appendValue(std::vector<std::byte>& targetBytes, ValueType value) {
    const std::size_t writeOffset = targetBytes.size();
    targetBytes.resize(writeOffset + sizeof value);
    std::memcpy(targetBytes.data() + writeOffset, &value, sizeof value);
}

/** All the mutable fields of one raw 80-byte record (defaults form a valid record). */
struct RawGraphicsPipelineRecord {
    std::uint32_t vertexShaderModuleSlot = 0u;
    std::uint32_t fragmentShaderModuleSlot = 1u;
    std::uint32_t pushConstantByteSize = 8u;
    std::uint32_t topologyValue = 4u; // TriangleList
    std::uint32_t colorAttachmentCount = 1u;
    std::uint32_t depthAttachmentFormatValue = 0u;
    std::uint32_t depthTestEnable = 0u;
    std::uint32_t depthWriteEnable = 0u;
    std::uint32_t depthCompareOperationValue = 0u;
    std::uint32_t cullModeValue = 1u;  // None
    std::uint32_t frontFaceValue = 1u; // CounterClockwise
    std::uint32_t reservedFlags = 0u;
    std::uint32_t colorAttachmentFormatValues[8] = {1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
};

/** A one-record table around the given raw record. */
std::vector<std::byte> makeTable(const RawGraphicsPipelineRecord& rawRecord) {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, rawRecord.vertexShaderModuleSlot);
    appendValue(tableBytes, rawRecord.fragmentShaderModuleSlot);
    appendValue(tableBytes, rawRecord.pushConstantByteSize);
    appendValue(tableBytes, rawRecord.topologyValue);
    appendValue(tableBytes, rawRecord.colorAttachmentCount);
    appendValue(tableBytes, rawRecord.depthAttachmentFormatValue);
    appendValue(tableBytes, rawRecord.depthTestEnable);
    appendValue(tableBytes, rawRecord.depthWriteEnable);
    appendValue(tableBytes, rawRecord.depthCompareOperationValue);
    appendValue(tableBytes, rawRecord.cullModeValue);
    appendValue(tableBytes, rawRecord.frontFaceValue);
    appendValue(tableBytes, rawRecord.reservedFlags);
    for (std::uint32_t formatValue : rawRecord.colorAttachmentFormatValues) {
        appendValue(tableBytes, formatValue);
    }
    return tableBytes;
}

/** Validates the one-record table and asserts the given precise failure. */
void requireRejection(const RawGraphicsPipelineRecord& rawRecord,
                      CommandStreamGraphicsPipelineTableValidationError expectedError) {
    const std::vector<std::byte> tableBytes = makeTable(rawRecord);
    const auto validationResult =
        CommandStreamGraphicsPipelineTableValidator::validate(tableBytes);
    REQUIRE_FALSE(validationResult.has_value());
    REQUIRE(validationResult.error().error == expectedError);
    REQUIRE(validationResult.error().byteOffset == 8u);
}

} // namespace

TEST_CASE("Graphics pipeline table validator accepts and decodes a color-only pipeline",
          "[commandStream][graphicsPipelineTable]") {
    const std::vector<std::byte> tableBytes = makeTable(RawGraphicsPipelineRecord{});
    REQUIRE(tableBytes.size() == 88u);

    const auto validationResult =
        CommandStreamGraphicsPipelineTableValidator::validate(tableBytes);
    REQUIRE(validationResult.has_value());
    REQUIRE(validationResult->pipelineCount() == 1u);

    const auto record = validationResult->pipelineRecord(0u);
    REQUIRE(record.vertexShaderModuleSlot == 0u);
    REQUIRE(record.fragmentShaderModuleSlot == 1u);
    REQUIRE(record.pushConstantByteSize == 8u);
    REQUIRE(record.topologyValue == 4u); // TriangleList
    REQUIRE(record.colorAttachmentCount == 1u);
    REQUIRE(record.colorAttachmentFormatValues[0] == 1u); // R8G8B8A8Unorm
    REQUIRE(record.depthAttachmentFormatValue == 0u);
}

TEST_CASE("Graphics pipeline table validator accepts a depth-tested pipeline",
          "[commandStream][graphicsPipelineTable]") {
    RawGraphicsPipelineRecord rawRecord{};
    rawRecord.depthAttachmentFormatValue = 5u; // D32Float
    rawRecord.depthTestEnable = 1u;
    rawRecord.depthWriteEnable = 1u;
    rawRecord.depthCompareOperationValue = 4u; // LessOrEqual
    const std::vector<std::byte> tableBytes = makeTable(rawRecord);
    const auto validationResult =
        CommandStreamGraphicsPipelineTableValidator::validate(tableBytes);
    REQUIRE(validationResult.has_value());
    REQUIRE(validationResult->pipelineRecord(0u).depthCompareOperationValue == 4u);
}

TEST_CASE("Graphics pipeline table validator rejects malformed tables with the precise failure",
          "[commandStream][graphicsPipelineTable]") {
    SECTION("table smaller than its header") {
        std::vector<std::byte> tableBytes(4u);
        const auto validationResult =
            CommandStreamGraphicsPipelineTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamGraphicsPipelineTableValidationError::TableTooSmall);
    }

    SECTION("size not matching the record count") {
        std::vector<std::byte> tableBytes = makeTable(RawGraphicsPipelineRecord{});
        tableBytes.resize(tableBytes.size() + 8u);
        const auto validationResult =
            CommandStreamGraphicsPipelineTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamGraphicsPipelineTableValidationError::TableSizeMismatch);
    }

    SECTION("non-zero header reserved flags") {
        std::vector<std::byte> tableBytes = makeTable(RawGraphicsPipelineRecord{});
        tableBytes[4] = std::byte{1};
        const auto validationResult =
            CommandStreamGraphicsPipelineTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamGraphicsPipelineTableValidationError::NonZeroReservedFlags);
    }

    SECTION("non-zero record reserved flags") {
        RawGraphicsPipelineRecord rawRecord{};
        rawRecord.reservedFlags = 1u;
        requireRejection(rawRecord,
                         CommandStreamGraphicsPipelineTableValidationError::
                             NonZeroRecordReservedFlags);
    }

    SECTION("unassigned topology") {
        RawGraphicsPipelineRecord rawRecord{};
        rawRecord.topologyValue = 9u;
        requireRejection(
            rawRecord, CommandStreamGraphicsPipelineTableValidationError::UnknownTopology);
    }

    SECTION("unassigned cull mode") {
        RawGraphicsPipelineRecord rawRecord{};
        rawRecord.cullModeValue = 0u;
        requireRejection(
            rawRecord, CommandStreamGraphicsPipelineTableValidationError::UnknownCullMode);
    }

    SECTION("unassigned front face") {
        RawGraphicsPipelineRecord rawRecord{};
        rawRecord.frontFaceValue = 3u;
        requireRejection(
            rawRecord, CommandStreamGraphicsPipelineTableValidationError::UnknownFrontFace);
    }

    SECTION("push constant size not a multiple of 4") {
        RawGraphicsPipelineRecord rawRecord{};
        rawRecord.pushConstantByteSize = 6u;
        requireRejection(rawRecord,
                         CommandStreamGraphicsPipelineTableValidationError::
                             InvalidPushConstantByteSize);
    }

    SECTION("push constant size beyond 128") {
        RawGraphicsPipelineRecord rawRecord{};
        rawRecord.pushConstantByteSize = 132u;
        requireRejection(rawRecord,
                         CommandStreamGraphicsPipelineTableValidationError::
                             InvalidPushConstantByteSize);
    }

    SECTION("more than eight color attachments") {
        RawGraphicsPipelineRecord rawRecord{};
        rawRecord.colorAttachmentCount = 9u;
        requireRejection(rawRecord,
                         CommandStreamGraphicsPipelineTableValidationError::
                             TooManyColorAttachments);
    }

    SECTION("no attachment at all") {
        RawGraphicsPipelineRecord rawRecord{};
        rawRecord.colorAttachmentCount = 0u;
        rawRecord.colorAttachmentFormatValues[0] = 0u;
        requireRejection(
            rawRecord, CommandStreamGraphicsPipelineTableValidationError::EmptyAttachmentSet);
    }

    SECTION("depth format used as a color attachment format") {
        RawGraphicsPipelineRecord rawRecord{};
        rawRecord.colorAttachmentFormatValues[0] = 5u; // D32Float
        requireRejection(rawRecord,
                         CommandStreamGraphicsPipelineTableValidationError::
                             UnknownColorAttachmentFormat);
    }

    SECTION("non-zero format beyond the color count") {
        RawGraphicsPipelineRecord rawRecord{};
        rawRecord.colorAttachmentFormatValues[3] = 1u;
        requireRejection(rawRecord,
                         CommandStreamGraphicsPipelineTableValidationError::
                             NonZeroUnusedColorFormat);
    }

    SECTION("color format used as the depth attachment format") {
        RawGraphicsPipelineRecord rawRecord{};
        rawRecord.depthAttachmentFormatValue = 1u; // R8G8B8A8Unorm
        requireRejection(rawRecord,
                         CommandStreamGraphicsPipelineTableValidationError::
                             InvalidDepthAttachmentFormat);
    }

    SECTION("depth enable flag beyond 1") {
        RawGraphicsPipelineRecord rawRecord{};
        rawRecord.depthAttachmentFormatValue = 5u;
        rawRecord.depthTestEnable = 2u;
        requireRejection(rawRecord,
                         CommandStreamGraphicsPipelineTableValidationError::
                             InvalidDepthEnableFlag);
    }

    SECTION("depth write without depth test") {
        RawGraphicsPipelineRecord rawRecord{};
        rawRecord.depthAttachmentFormatValue = 5u;
        rawRecord.depthWriteEnable = 1u;
        requireRejection(rawRecord,
                         CommandStreamGraphicsPipelineTableValidationError::
                             DepthWriteWithoutDepthTest);
    }

    SECTION("depth test without a depth attachment") {
        RawGraphicsPipelineRecord rawRecord{};
        rawRecord.depthTestEnable = 1u;
        rawRecord.depthCompareOperationValue = 2u;
        requireRejection(rawRecord,
                         CommandStreamGraphicsPipelineTableValidationError::
                             DepthStateWithoutDepthAttachment);
    }

    SECTION("enabled depth test with an unassigned compare operation") {
        RawGraphicsPipelineRecord rawRecord{};
        rawRecord.depthAttachmentFormatValue = 5u;
        rawRecord.depthTestEnable = 1u;
        rawRecord.depthCompareOperationValue = 9u;
        requireRejection(rawRecord,
                         CommandStreamGraphicsPipelineTableValidationError::
                             UnknownDepthCompareOperation);
    }

    SECTION("disabled depth test with a non-zero compare operation") {
        RawGraphicsPipelineRecord rawRecord{};
        rawRecord.depthCompareOperationValue = 2u;
        requireRejection(rawRecord,
                         CommandStreamGraphicsPipelineTableValidationError::
                             NonZeroDisabledDepthCompareOperation);
    }
}
