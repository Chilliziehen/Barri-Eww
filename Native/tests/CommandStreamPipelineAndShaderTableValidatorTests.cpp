#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamHandleTableValidationError.hpp"
#include "BarriEww/CommandStream/CommandStreamPipelineHandleTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamShaderModuleTableValidator.hpp"

using barrieww::CommandStreamHandleTableValidationError;
using barrieww::CommandStreamPipelineHandleTableValidator;
using barrieww::CommandStreamShaderModuleTableValidator;

namespace {

/** Appends one little-endian value to a byte vector. */
template <typename ValueType>
void appendValue(std::vector<std::byte>& targetBytes, ValueType value) {
    const std::size_t writeOffset = targetBytes.size();
    targetBytes.resize(writeOffset + sizeof value);
    std::memcpy(targetBytes.data() + writeOffset, &value, sizeof value);
}

/** A minimal well-formed 24-byte SPIR-V-shaped blob (magic + five zero words). */
std::vector<std::byte> makeMinimalSpirvBlob() {
    std::vector<std::byte> blobBytes;
    appendValue(blobBytes, std::uint32_t{0x07230203});
    for (int wordIndex = 0; wordIndex < 5; ++wordIndex) {
        appendValue(blobBytes, std::uint32_t{0});
    }
    return blobBytes;
}

/** A one-blob shader table with the given blob bytes placed right after the directory. */
std::vector<std::byte> makeSingleBlobShaderTable(const std::vector<std::byte>& blobBytes) {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint64_t{24}); // blobByteOffset (8 header + 16 entry)
    appendValue(tableBytes, static_cast<std::uint64_t>(blobBytes.size()));
    tableBytes.insert(tableBytes.end(), blobBytes.begin(), blobBytes.end());
    return tableBytes;
}

/** A pipeline table from raw entries (kind, moduleSlot, pushSize, reserved). */
std::vector<std::byte> makePipelineTable(
    const std::vector<std::array<std::uint32_t, 4>>& rawEntries) {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, static_cast<std::uint32_t>(rawEntries.size()));
    appendValue(tableBytes, std::uint32_t{0});
    for (const auto& rawEntry : rawEntries) {
        for (std::uint32_t fieldValue : rawEntry) {
            appendValue(tableBytes, fieldValue);
        }
    }
    return tableBytes;
}

} // namespace

TEST_CASE("Shader module table validator accepts and rejects precisely",
          "[commandStream][shaderModuleTable]") {
    SECTION("valid single blob") {
        const std::vector<std::byte> tableBytes =
            makeSingleBlobShaderTable(makeMinimalSpirvBlob());
        const auto validationResult =
            CommandStreamShaderModuleTableValidator::validate(tableBytes);
        REQUIRE(validationResult.has_value());
        REQUIRE(validationResult->entryCount() == 1u);
        REQUIRE(validationResult->blob(0u).size() == 24u);
    }

    SECTION("blob with a corrupt magic word") {
        std::vector<std::byte> corruptBlob = makeMinimalSpirvBlob();
        corruptBlob[0] = std::byte{0x00};
        const auto validationResult = CommandStreamShaderModuleTableValidator::validate(
            makeSingleBlobShaderTable(corruptBlob));
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::InvalidShaderBlobMagic);
    }

    SECTION("blob shorter than the SPIR-V minimum") {
        std::vector<std::byte> tinyBlob(16u, std::byte{0});
        std::memcpy(tinyBlob.data(), "\x03\x02\x23\x07", 4u);
        const auto validationResult = CommandStreamShaderModuleTableValidator::validate(
            makeSingleBlobShaderTable(tinyBlob));
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::InvalidShaderBlobByteSize);
    }

    SECTION("blob region leaving the table") {
        std::vector<std::byte> tableBytes =
            makeSingleBlobShaderTable(makeMinimalSpirvBlob());
        // Inflate the entry's blobByteSize (at offset 16) past the table end.
        const std::uint64_t inflatedByteSize = 4096u;
        std::memcpy(tableBytes.data() + 16u, &inflatedByteSize, sizeof inflatedByteSize);
        const auto validationResult =
            CommandStreamShaderModuleTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::ShaderBlobOutOfBounds);
    }
}

TEST_CASE("Pipeline table validator accepts and rejects precisely",
          "[commandStream][pipelineHandleTable]") {
    SECTION("valid compute entry") {
        // Named local: the view aliases these bytes (MemoryOwnership contract).
        const std::vector<std::byte> tableBytes = makePipelineTable({{1u, 0u, 8u, 0u}});
        const auto validationResult =
            CommandStreamPipelineHandleTableValidator::validate(tableBytes);
        REQUIRE(validationResult.has_value());
        REQUIRE(validationResult->entryCount() == 1u);
        REQUIRE(validationResult->entry(0u).pushConstantByteSize == 8u);
    }

    SECTION("unassigned pipeline kind") {
        const auto validationResult = CommandStreamPipelineHandleTableValidator::validate(
            makePipelineTable({{7u, 0u, 8u, 0u}}));
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::UnknownPipelineKind);
    }

    SECTION("push constant size above the portable bound") {
        const auto validationResult = CommandStreamPipelineHandleTableValidator::validate(
            makePipelineTable({{1u, 0u, 132u, 0u}}));
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::InvalidPushConstantByteSize);
    }

    SECTION("size not matching the entry count") {
        std::vector<std::byte> tableBytes = makePipelineTable({{1u, 0u, 8u, 0u}});
        tableBytes.resize(tableBytes.size() + 8u);
        const auto validationResult =
            CommandStreamPipelineHandleTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::TableSizeMismatch);
    }
}
