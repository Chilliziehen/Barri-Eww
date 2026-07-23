#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamPipelineHandleTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamShaderModuleTableValidator.hpp"
#include "BarriEww/Vulkan/VulkanContext.hpp"
#include "BarriEww/Vulkan/VulkanPipelineTable.hpp"
#include "BarriEww/Vulkan/VulkanPipelineTableCreationError.hpp"
#include "TestVulkanDeviceHarness.hpp"

using barrieww::CommandStreamPipelineHandleTableValidator;
using barrieww::CommandStreamShaderModuleTableValidator;
using barrieww::VulkanContext;
using barrieww::VulkanPipelineTable;
using barrieww::VulkanPipelineTableCreationError;
using barrieww::testing::TestVulkanDeviceHarness;

namespace {

/**
 * Appends one little-endian value to a byte vector.
 * @param std::vector<std::byte>& targetBytes Destination byte vector
 * @param ValueType value Fixed-width value to append
 */
template <typename ValueType>
void appendValue(std::vector<std::byte>& targetBytes, ValueType value) {
    const std::size_t writeOffset = targetBytes.size();
    targetBytes.resize(writeOffset + sizeof value);
    std::memcpy(targetBytes.data() + writeOffset, &value, sizeof value);
}

/** Reads the committed valid compute shader used by existing GPU execution tests.
 * @return std::vector<std::byte> Complete committed SPIR-V bytes
 */
std::vector<std::byte> readComputeShaderBlob() {
    const std::string blobFilePath = std::string(BARRIEWW_TEST_DATA_DIRECTORY)
                                     + "/Shaders/WritePatternByDeviceAddress.comp.spv";
    std::ifstream fileStream(blobFilePath, std::ios::binary | std::ios::ate);
    REQUIRE(fileStream.is_open());
    const std::streamsize fileByteCount = fileStream.tellg();
    fileStream.seekg(0);
    std::vector<std::byte> blobBytes(static_cast<std::size_t>(fileByteCount));
    fileStream.read(reinterpret_cast<char*>(blobBytes.data()), fileByteCount);
    return blobBytes;
}

/** Builds a zero- or one-blob shader table.
 * @param const std::vector<std::byte>& blobBytes Optional SPIR-V bytes
 * @return std::vector<std::byte> Complete shader table bytes
 */
std::vector<std::byte> makeShaderTableBytes(const std::vector<std::byte>& blobBytes) {
    std::vector<std::byte> tableBytes;
    const std::uint32_t entryCount = blobBytes.empty() ? 0u : 1u;
    appendValue(tableBytes, entryCount);
    appendValue(tableBytes, std::uint32_t{0});
    if (!blobBytes.empty()) {
        appendValue(tableBytes, std::uint64_t{24});
        appendValue(tableBytes, static_cast<std::uint64_t>(blobBytes.size()));
        tableBytes.insert(tableBytes.end(), blobBytes.begin(), blobBytes.end());
    }
    return tableBytes;
}

/** Builds compute pipeline entries as {shader slot, push-constant byte size}.
 * @param const std::vector<std::array<std::uint32_t, 2>>& entries Pipeline fields
 * @return std::vector<std::byte> Complete pipeline table bytes
 */
std::vector<std::byte> makePipelineTableBytes(
    const std::vector<std::array<std::uint32_t, 2>>& entries) {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, static_cast<std::uint32_t>(entries.size()));
    appendValue(tableBytes, std::uint32_t{0});
    for (const auto& entry : entries) {
        appendValue(tableBytes, std::uint32_t{1}); // Compute
        appendValue(tableBytes, entry[0]);
        appendValue(tableBytes, entry[1]);
        appendValue(tableBytes, std::uint32_t{0});
    }
    return tableBytes;
}

} // namespace

TEST_CASE("Vulkan pipeline table materializes empty tables and reports slot ranges",
          "[vulkanPipelineTable][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    if (!harness->supportsBufferDeviceAddress()) {
        SKIP("device does not support bufferDeviceAddress required by the shader fixture");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};

    const std::vector<std::byte> emptyShaderTableBytes = makeShaderTableBytes({});
    const std::vector<std::byte> emptyPipelineTableBytes = makePipelineTableBytes({});
    const auto emptyShaderTableView =
        CommandStreamShaderModuleTableValidator::validate(emptyShaderTableBytes);
    const auto emptyPipelineTableView =
        CommandStreamPipelineHandleTableValidator::validate(emptyPipelineTableBytes);
    REQUIRE(emptyShaderTableView.has_value());
    REQUIRE(emptyPipelineTableView.has_value());

    auto emptyPipelineTable = VulkanPipelineTable::createFromTables(
        vulkanContext, *emptyPipelineTableView, *emptyShaderTableView);
    REQUIRE(emptyPipelineTable.has_value());
    REQUIRE(emptyPipelineTable->slotCount() == 0u);

    const std::vector<std::byte> invalidPipelineTableBytes =
        makePipelineTableBytes({{0u, 0u}});
    const auto invalidPipelineTableView =
        CommandStreamPipelineHandleTableValidator::validate(invalidPipelineTableBytes);
    REQUIRE(invalidPipelineTableView.has_value());
    const auto failure = VulkanPipelineTable::createFromTables(
        vulkanContext, *invalidPipelineTableView, *emptyShaderTableView);
    REQUIRE_FALSE(failure.has_value());
    REQUIRE(failure.error().error == VulkanPipelineTableCreationError::ShaderModuleSlotOutOfRange);
    REQUIRE(failure.error().slotIndex == 0u);
    REQUIRE(failure.error().resultValue == VK_SUCCESS);
}

TEST_CASE("Vulkan pipeline table rolls back earlier slots on a later range failure",
          "[vulkanPipelineTable][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    if (!harness->supportsBufferDeviceAddress()) {
        SKIP("device does not support bufferDeviceAddress required by the shader fixture");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};

    const std::vector<std::byte> shaderTableBytes =
        makeShaderTableBytes(readComputeShaderBlob());
    const auto shaderTableView =
        CommandStreamShaderModuleTableValidator::validate(shaderTableBytes);
    REQUIRE(shaderTableView.has_value());

    const std::vector<std::byte> partialPipelineTableBytes =
        makePipelineTableBytes({{0u, 8u}, {1u, 8u}});
    const auto partialPipelineTableView =
        CommandStreamPipelineHandleTableValidator::validate(partialPipelineTableBytes);
    REQUIRE(partialPipelineTableView.has_value());
    const auto failure = VulkanPipelineTable::createFromTables(
        vulkanContext, *partialPipelineTableView, *shaderTableView);
    REQUIRE_FALSE(failure.has_value());
    REQUIRE(failure.error().error == VulkanPipelineTableCreationError::ShaderModuleSlotOutOfRange);
    REQUIRE(failure.error().slotIndex == 1u);
    REQUIRE(failure.error().resultValue == VK_SUCCESS);

    const std::vector<std::byte> validPipelineTableBytes =
        makePipelineTableBytes({{0u, 8u}});
    const auto validPipelineTableView =
        CommandStreamPipelineHandleTableValidator::validate(validPipelineTableBytes);
    REQUIRE(validPipelineTableView.has_value());
    auto validPipelineTable = VulkanPipelineTable::createFromTables(
        vulkanContext, *validPipelineTableView, *shaderTableView);
    REQUIRE(validPipelineTable.has_value());
    REQUIRE(validPipelineTable->slotCount() == 1u);
    REQUIRE(validPipelineTable->slot(0u).pipeline != VK_NULL_HANDLE);
    REQUIRE(validPipelineTable->slot(0u).pipelineLayout != VK_NULL_HANDLE);

    VulkanPipelineTable movedPipelineTable{std::move(*validPipelineTable)};
    REQUIRE(validPipelineTable->slotCount() == 0u);
    REQUIRE(movedPipelineTable.slotCount() == 1u);
    REQUIRE(movedPipelineTable.slot(0u).pipeline != VK_NULL_HANDLE);
}
