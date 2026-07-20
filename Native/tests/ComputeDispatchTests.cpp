#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamBufferHandleTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamOpcode.hpp"
#include "BarriEww/CommandStream/CommandStreamPipelineHandleTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamShaderModuleTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamValidator.hpp"
#include "BarriEww/Vulkan/CommandBufferRecorder.hpp"
#include "BarriEww/Vulkan/CommandBufferRecordingError.hpp"
#include "BarriEww/Vulkan/VulkanBufferTable.hpp"
#include "BarriEww/Vulkan/VulkanContext.hpp"
#include "BarriEww/Vulkan/VulkanPipelineTable.hpp"
#include "TestCommandStreamBuilder.hpp"
#include "TestVulkanDeviceHarness.hpp"

using barrieww::CommandBufferRecorder;
using barrieww::CommandBufferRecordingError;
using barrieww::CommandStreamBufferHandleTableValidator;
using barrieww::CommandStreamOpcode;
using barrieww::CommandStreamPipelineHandleTableValidator;
using barrieww::CommandStreamShaderModuleTableValidator;
using barrieww::CommandStreamValidator;
using barrieww::VulkanBufferTable;
using barrieww::VulkanContext;
using barrieww::VulkanPipelineTable;
using barrieww::testing::TestCommandStreamBuilder;
using barrieww::testing::TestVulkanDeviceHarness;

namespace {

/** Appends one little-endian value to a byte vector. */
template <typename ValueType>
void appendValue(std::vector<std::byte>& targetBytes, ValueType value) {
    const std::size_t writeOffset = targetBytes.size();
    targetBytes.resize(writeOffset + sizeof value);
    std::memcpy(targetBytes.data() + writeOffset, &value, sizeof value);
}

/** Reads the committed SPIR-V fixture from the shared test data directory. */
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

/** A one-blob shader table embedding the given SPIR-V. */
std::vector<std::byte> makeShaderTableBytes(const std::vector<std::byte>& blobBytes) {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint64_t{24});
    appendValue(tableBytes, static_cast<std::uint64_t>(blobBytes.size()));
    tableBytes.insert(tableBytes.end(), blobBytes.begin(), blobBytes.end());
    return tableBytes;
}

/** One compute pipeline: shader slot 0, 8-byte push range (the device address). */
std::vector<std::byte> makePipelineTableBytes() {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{1}); // pipelineKind Compute
    appendValue(tableBytes, std::uint32_t{0}); // shaderModuleSlot
    appendValue(tableBytes, std::uint32_t{8}); // pushConstantByteSize
    appendValue(tableBytes, std::uint32_t{0});
    return tableBytes;
}

/** One 64-byte buffer the shader writes through its device address. */
std::vector<std::byte> makeOutputBufferTableBytes(bool withDeviceAddressUsage = true) {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint64_t{64});
    appendValue(tableBytes,
                std::uint32_t{withDeviceAddressUsage ? 0xA0u : 0x20u}); // Storage(|DeviceAddress)
    appendValue(tableBytes, std::uint32_t{3}); // HostVisibleReadback
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0});
    return tableBytes;
}

/** BindComputePipeline(0) + PushBufferDeviceAddress(slot 0 -> push offset 0) + Dispatch. */
std::vector<std::byte> makeComputeStreamBytes(std::uint32_t dispatchGroupCountX) {
    TestCommandStreamBuilder streamBuilder{0u};
    {
        std::vector<std::byte> payloadBytes;
        appendValue(payloadBytes, std::uint32_t{0}); // pipelineSlot
        appendValue(payloadBytes, std::uint32_t{0});
        streamBuilder.appendCommand(
            static_cast<std::uint16_t>(CommandStreamOpcode::BindComputePipeline),
            payloadBytes);
    }
    {
        std::vector<std::byte> payloadBytes;
        appendValue(payloadBytes, std::uint32_t{0}); // bufferSlot
        appendValue(payloadBytes, std::uint32_t{0}); // pushConstantByteOffset
        streamBuilder.appendCommand(
            static_cast<std::uint16_t>(CommandStreamOpcode::PushBufferDeviceAddress),
            payloadBytes);
    }
    {
        std::vector<std::byte> payloadBytes;
        appendValue(payloadBytes, dispatchGroupCountX);
        appendValue(payloadBytes, std::uint32_t{1});
        appendValue(payloadBytes, std::uint32_t{1});
        appendValue(payloadBytes, std::uint32_t{0});
        streamBuilder.appendCommand(
            static_cast<std::uint16_t>(CommandStreamOpcode::Dispatch), payloadBytes);
    }
    return streamBuilder.build();
}

} // namespace

// First real GPU COMPUTE execution: shader table -> pipeline materialization ->
// BindComputePipeline / PushBufferDeviceAddress / Dispatch recorded at load ->
// submit -> the shader writes values[i] = i * 7 + 3 through the pushed device address.
// The address is resolved at record time from the buffer SLOT, keeping the bake
// artifact pure data (ADR-0002 slot indirection, ADR-0003 decided-before-execution).
TEST_CASE("Recorded compute dispatch writes the arithmetic pattern through a device address",
          "[computeDispatch][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    if (!harness->supportsBufferDeviceAddress()) {
        SKIP("device does not support bufferDeviceAddress");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};

    const std::vector<std::byte> shaderBlob = readComputeShaderBlob();
    const std::vector<std::byte> shaderTableBytes = makeShaderTableBytes(shaderBlob);
    const std::vector<std::byte> pipelineTableBytes = makePipelineTableBytes();
    const std::vector<std::byte> bufferTableBytes = makeOutputBufferTableBytes();

    const auto shaderTableView =
        CommandStreamShaderModuleTableValidator::validate(shaderTableBytes);
    const auto pipelineTableView =
        CommandStreamPipelineHandleTableValidator::validate(pipelineTableBytes);
    const auto bufferTableView =
        CommandStreamBufferHandleTableValidator::validate(bufferTableBytes);
    REQUIRE(shaderTableView.has_value());
    REQUIRE(pipelineTableView.has_value());
    REQUIRE(bufferTableView.has_value());

    auto bufferTable = VulkanBufferTable::createFromTable(vulkanContext, *bufferTableView);
    REQUIRE(bufferTable.has_value());
    REQUIRE(bufferTable->slot(0u).deviceAddress != 0u);
    auto pipelineTable = VulkanPipelineTable::createFromTables(
        vulkanContext, *pipelineTableView, *shaderTableView);
    REQUIRE(pipelineTable.has_value());

    const std::vector<std::byte> streamBytes = makeComputeStreamBytes(1u);
    const auto streamView = CommandStreamValidator::validate(streamBytes);
    REQUIRE(streamView.has_value());

    const VkCommandBuffer commandBuffer = harness->allocateCommandBuffer();
    REQUIRE(CommandBufferRecorder::record(commandBuffer, *streamView, *bufferTable,
                                          nullptr, &*pipelineTable)
                .has_value());
    REQUIRE(harness->submitAndWait(commandBuffer));

    const std::byte* outputBytes = bufferTable->slot(0u).mappedPointer;
    REQUIRE(outputBytes != nullptr);
    for (std::uint32_t valueIndex = 0; valueIndex < 16u; ++valueIndex) {
        std::uint32_t writtenValue = 0;
        std::memcpy(&writtenValue, outputBytes + valueIndex * 4u, sizeof writtenValue);
        REQUIRE(writtenValue == valueIndex * 7u + 3u);
    }
}

TEST_CASE("Compute recording rejects invalid streams with the precise failure",
          "[computeDispatch][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    if (!harness->supportsBufferDeviceAddress()) {
        SKIP("device does not support bufferDeviceAddress");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};

    // Named locals: every view below aliases its byte vector (MemoryOwnership contract).
    const std::vector<std::byte> shaderBlob = readComputeShaderBlob();
    const std::vector<std::byte> shaderTableBytes = makeShaderTableBytes(shaderBlob);
    const std::vector<std::byte> pipelineTableBytes = makePipelineTableBytes();
    const auto shaderTableView =
        CommandStreamShaderModuleTableValidator::validate(shaderTableBytes);
    const auto pipelineTableView =
        CommandStreamPipelineHandleTableValidator::validate(pipelineTableBytes);
    REQUIRE(shaderTableView.has_value());
    REQUIRE(pipelineTableView.has_value());
    auto pipelineTable = VulkanPipelineTable::createFromTables(
        vulkanContext, *pipelineTableView, *shaderTableView);
    REQUIRE(pipelineTable.has_value());

    const std::vector<std::byte> bufferTableBytes = makeOutputBufferTableBytes();
    const auto bufferTableView =
        CommandStreamBufferHandleTableValidator::validate(bufferTableBytes);
    REQUIRE(bufferTableView.has_value());
    auto bufferTable = VulkanBufferTable::createFromTable(vulkanContext, *bufferTableView);
    REQUIRE(bufferTable.has_value());

    SECTION("missing pipeline table") {
        const std::vector<std::byte> streamBytes = makeComputeStreamBytes(1u);
        const auto streamView = CommandStreamValidator::validate(streamBytes);
        REQUIRE(streamView.has_value());
        const auto recordingResult = CommandBufferRecorder::record(
            harness->allocateCommandBuffer(), *streamView, *bufferTable);
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::MissingPipelineTable);
    }

    SECTION("dispatch without a bound pipeline") {
        TestCommandStreamBuilder streamBuilder{0u};
        std::vector<std::byte> payloadBytes;
        appendValue(payloadBytes, std::uint32_t{1});
        appendValue(payloadBytes, std::uint32_t{1});
        appendValue(payloadBytes, std::uint32_t{1});
        appendValue(payloadBytes, std::uint32_t{0});
        streamBuilder.appendCommand(
            static_cast<std::uint16_t>(CommandStreamOpcode::Dispatch), payloadBytes);
        const std::vector<std::byte> streamBytes = streamBuilder.build();
        const auto streamView = CommandStreamValidator::validate(streamBytes);
        REQUIRE(streamView.has_value());
        const auto recordingResult =
            CommandBufferRecorder::record(harness->allocateCommandBuffer(), *streamView,
                                          *bufferTable, nullptr, &*pipelineTable);
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::NoBoundComputePipeline);
    }

    SECTION("device address pushed from a buffer without the usage bit") {
        const std::vector<std::byte> plainBufferTableBytes =
            makeOutputBufferTableBytes(/*withDeviceAddressUsage=*/false);
        const auto plainBufferTableView =
            CommandStreamBufferHandleTableValidator::validate(plainBufferTableBytes);
        REQUIRE(plainBufferTableView.has_value());
        auto plainBufferTable =
            VulkanBufferTable::createFromTable(vulkanContext, *plainBufferTableView);
        REQUIRE(plainBufferTable.has_value());
        REQUIRE(plainBufferTable->slot(0u).deviceAddress == 0u);

        const std::vector<std::byte> streamBytes = makeComputeStreamBytes(1u);
        const auto streamView = CommandStreamValidator::validate(streamBytes);
        REQUIRE(streamView.has_value());
        const auto recordingResult =
            CommandBufferRecorder::record(harness->allocateCommandBuffer(), *streamView,
                                          *plainBufferTable, nullptr, &*pipelineTable);
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::MissingBufferDeviceAddress);
    }
}
