#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamBarrierBatchTableValidator.hpp"
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
using barrieww::CommandStreamBarrierBatchTableValidator;
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

/** Reads one committed SPIR-V fixture from the shared test data directory. */
std::vector<std::byte> readShaderBlob(const char* blobFileName) {
    const std::string blobFilePath =
        std::string(BARRIEWW_TEST_DATA_DIRECTORY) + "/Shaders/" + blobFileName;
    std::ifstream fileStream(blobFilePath, std::ios::binary | std::ios::ate);
    REQUIRE(fileStream.is_open());
    const std::streamsize fileByteCount = fileStream.tellg();
    fileStream.seekg(0);
    std::vector<std::byte> blobBytes(static_cast<std::size_t>(fileByteCount));
    fileStream.read(reinterpret_cast<char*>(blobBytes.data()), fileByteCount);
    return blobBytes;
}

/** A two-blob shader table (argument producer at slot 0, pattern writer at slot 1). */
std::vector<std::byte> makeShaderTableBytes(const std::vector<std::byte>& firstBlob,
                                            const std::vector<std::byte>& secondBlob) {
    const auto alignUp = [](std::uint64_t value) { return (value + 7u) & ~std::uint64_t{7}; };
    const std::uint64_t firstBlobOffset = 8u + 2u * 16u;
    const std::uint64_t secondBlobOffset = alignUp(firstBlobOffset + firstBlob.size());

    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{2});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, firstBlobOffset);
    appendValue(tableBytes, static_cast<std::uint64_t>(firstBlob.size()));
    appendValue(tableBytes, secondBlobOffset);
    appendValue(tableBytes, static_cast<std::uint64_t>(secondBlob.size()));
    tableBytes.insert(tableBytes.end(), firstBlob.begin(), firstBlob.end());
    tableBytes.resize(static_cast<std::size_t>(secondBlobOffset), std::byte{0});
    tableBytes.insert(tableBytes.end(), secondBlob.begin(), secondBlob.end());
    return tableBytes;
}

/** Two compute pipelines, both with the 8-byte device-address push range. */
std::vector<std::byte> makePipelineTableBytes() {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{2});
    appendValue(tableBytes, std::uint32_t{0});
    for (std::uint32_t shaderModuleSlot = 0; shaderModuleSlot < 2u; ++shaderModuleSlot) {
        appendValue(tableBytes, std::uint32_t{1}); // Compute
        appendValue(tableBytes, shaderModuleSlot);
        appendValue(tableBytes, std::uint32_t{8});
        appendValue(tableBytes, std::uint32_t{0});
    }
    return tableBytes;
}

/**
 * Slot 0: the indirect arguments buffer the GPU fills (Indirect | DeviceAddress,
 * device-local). Slot 1: the 128-byte output buffer (Storage | DeviceAddress, readback).
 */
std::vector<std::byte> makeBufferTableBytes(std::uint32_t indirectUsageFlags = 0xC0u) {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{2});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint64_t{16});
    appendValue(tableBytes, indirectUsageFlags);
    appendValue(tableBytes, std::uint32_t{1}); // DeviceLocal
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint64_t{128});
    appendValue(tableBytes, std::uint32_t{0xA0}); // Storage | DeviceAddress
    appendValue(tableBytes, std::uint32_t{3});    // HostVisibleReadback
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0});
    return tableBytes;
}

/** One batch: buffer barrier on the arguments buffer, compute write -> indirect read. */
std::vector<std::byte> makeArgumentBarrierTableBytes() {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0});  // 0 global barriers
    appendValue(tableBytes, std::uint32_t{1});  // 1 buffer barrier
    appendValue(tableBytes, std::uint32_t{0});  // 0 image barriers
    appendValue(tableBytes, std::uint32_t{0});  // reservedFlags
    appendValue(tableBytes, std::uint64_t{32}); // records right after the directory
    appendValue(tableBytes, std::uint64_t{0x800}); // COMPUTE_SHADER stage
    appendValue(tableBytes, std::uint64_t{0x40});  // SHADER_WRITE access
    appendValue(tableBytes, std::uint64_t{0x2});   // DRAW_INDIRECT stage
    appendValue(tableBytes, std::uint64_t{0x1});   // INDIRECT_COMMAND_READ access
    appendValue(tableBytes, std::uint32_t{0});     // bufferSlot 0 (arguments)
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint64_t{0});
    appendValue(tableBytes, std::uint64_t{0xFFFFFFFFFFFFFFFFull}); // whole size
    return tableBytes;
}

/** Pinned DispatchIndirect payload: bufferSlot, reserved, bufferOffset. */
std::vector<std::byte> makeDispatchIndirectPayload(std::uint32_t bufferSlot,
                                                   std::uint64_t bufferOffset) {
    std::vector<std::byte> payloadBytes;
    appendValue(payloadBytes, bufferSlot);
    appendValue(payloadBytes, std::uint32_t{0});
    appendValue(payloadBytes, bufferOffset);
    return payloadBytes;
}

void appendBindComputePipeline(TestCommandStreamBuilder& streamBuilder,
                               std::uint32_t pipelineSlot) {
    std::vector<std::byte> payloadBytes;
    appendValue(payloadBytes, pipelineSlot);
    appendValue(payloadBytes, std::uint32_t{0});
    streamBuilder.appendCommand(
        static_cast<std::uint16_t>(CommandStreamOpcode::BindComputePipeline), payloadBytes);
}

void appendPushBufferDeviceAddress(TestCommandStreamBuilder& streamBuilder,
                                   std::uint32_t bufferSlot) {
    std::vector<std::byte> payloadBytes;
    appendValue(payloadBytes, bufferSlot);
    appendValue(payloadBytes, std::uint32_t{0});
    streamBuilder.appendCommand(
        static_cast<std::uint16_t>(CommandStreamOpcode::PushBufferDeviceAddress),
        payloadBytes);
}

} // namespace

// The GPU-driven cornerstone in miniature, prerecorded as ONE command buffer:
// dispatch #1 COMPUTES the indirect arguments {2,1,1} on the GPU, a barrier moves them
// from compute-write to indirect-read, and DispatchIndirect runs the pattern shader
// with a workload the CPU never knew — 2 workgroups x 16 invocations = 32 values.
// Everything was decided at load; execution is one submit (ADR-0001/ADR-0003).
TEST_CASE("GPU-computed indirect arguments drive a prerecorded dispatch",
          "[indirectDispatch][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    if (!harness->supportsBufferDeviceAddress()) {
        SKIP("device does not support bufferDeviceAddress");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};

    const std::vector<std::byte> argumentShaderBlob =
        readShaderBlob("WriteDispatchArguments.comp.spv");
    const std::vector<std::byte> patternShaderBlob =
        readShaderBlob("WritePatternByDeviceAddress.comp.spv");
    const std::vector<std::byte> shaderTableBytes =
        makeShaderTableBytes(argumentShaderBlob, patternShaderBlob);
    const std::vector<std::byte> pipelineTableBytes = makePipelineTableBytes();
    const std::vector<std::byte> bufferTableBytes = makeBufferTableBytes();
    const std::vector<std::byte> barrierTableBytes = makeArgumentBarrierTableBytes();

    const auto shaderTableView =
        CommandStreamShaderModuleTableValidator::validate(shaderTableBytes);
    const auto pipelineTableView =
        CommandStreamPipelineHandleTableValidator::validate(pipelineTableBytes);
    const auto bufferTableView =
        CommandStreamBufferHandleTableValidator::validate(bufferTableBytes);
    const auto barrierTableView =
        CommandStreamBarrierBatchTableValidator::validate(barrierTableBytes);
    REQUIRE(shaderTableView.has_value());
    REQUIRE(pipelineTableView.has_value());
    REQUIRE(bufferTableView.has_value());
    REQUIRE(barrierTableView.has_value());

    auto bufferTable = VulkanBufferTable::createFromTable(vulkanContext, *bufferTableView);
    REQUIRE(bufferTable.has_value());
    auto pipelineTable = VulkanPipelineTable::createFromTables(
        vulkanContext, *pipelineTableView, *shaderTableView);
    REQUIRE(pipelineTable.has_value());

    TestCommandStreamBuilder streamBuilder{0u};
    appendBindComputePipeline(streamBuilder, 0u);       // argument producer
    appendPushBufferDeviceAddress(streamBuilder, 0u);   // arguments buffer address
    {
        std::vector<std::byte> payloadBytes;
        appendValue(payloadBytes, std::uint32_t{1});
        appendValue(payloadBytes, std::uint32_t{1});
        appendValue(payloadBytes, std::uint32_t{1});
        appendValue(payloadBytes, std::uint32_t{0});
        streamBuilder.appendCommand(
            static_cast<std::uint16_t>(CommandStreamOpcode::Dispatch), payloadBytes);
    }
    {
        std::vector<std::byte> payloadBytes;
        appendValue(payloadBytes, std::uint32_t{0}); // barrierBatchSlot
        appendValue(payloadBytes, std::uint32_t{0});
        streamBuilder.appendCommand(
            static_cast<std::uint16_t>(CommandStreamOpcode::ExecuteBarrierBatch),
            payloadBytes);
    }
    appendBindComputePipeline(streamBuilder, 1u);       // pattern writer
    appendPushBufferDeviceAddress(streamBuilder, 1u);   // output buffer address
    streamBuilder.appendCommand(
        static_cast<std::uint16_t>(CommandStreamOpcode::DispatchIndirect),
        makeDispatchIndirectPayload(0u, 0u));
    const std::vector<std::byte> streamBytes = streamBuilder.build();
    const auto streamView = CommandStreamValidator::validate(streamBytes);
    REQUIRE(streamView.has_value());

    const VkCommandBuffer commandBuffer = harness->allocateCommandBuffer();
    REQUIRE(CommandBufferRecorder::record(commandBuffer, *streamView,
            {.bufferTable = &*bufferTable, .barrierBatchTableView = &*barrierTableView, .pipelineTable = &*pipelineTable})
                .has_value());
    REQUIRE(harness->submitAndWait(commandBuffer));

    const std::byte* outputBytes = bufferTable->slot(1u).mappedPointer;
    REQUIRE(outputBytes != nullptr);
    for (std::uint32_t valueIndex = 0; valueIndex < 32u; ++valueIndex) {
        std::uint32_t writtenValue = 0;
        std::memcpy(&writtenValue, outputBytes + valueIndex * 4u, sizeof writtenValue);
        REQUIRE(writtenValue == valueIndex * 7u + 3u);
    }
}

TEST_CASE("Indirect dispatch recording rejects invalid streams with the precise failure",
          "[indirectDispatch][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    if (!harness->supportsBufferDeviceAddress()) {
        SKIP("device does not support bufferDeviceAddress");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};

    const std::vector<std::byte> argumentShaderBlob =
        readShaderBlob("WriteDispatchArguments.comp.spv");
    const std::vector<std::byte> patternShaderBlob =
        readShaderBlob("WritePatternByDeviceAddress.comp.spv");
    const std::vector<std::byte> shaderTableBytes =
        makeShaderTableBytes(argumentShaderBlob, patternShaderBlob);
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

    const auto recordSingleIndirect = [&](const VulkanBufferTable& bufferTable,
                                          std::uint64_t bufferOffset) {
        TestCommandStreamBuilder streamBuilder{0u};
        appendBindComputePipeline(streamBuilder, 1u);
        streamBuilder.appendCommand(
            static_cast<std::uint16_t>(CommandStreamOpcode::DispatchIndirect),
            makeDispatchIndirectPayload(0u, bufferOffset));
        const std::vector<std::byte> streamBytes = streamBuilder.build();
        const auto streamView = CommandStreamValidator::validate(streamBytes);
        REQUIRE(streamView.has_value());
        return CommandBufferRecorder::record(harness->allocateCommandBuffer(), *streamView,
            {.bufferTable = &bufferTable, .pipelineTable = &*pipelineTable});
    };

    SECTION("arguments buffer without the Indirect usage bit") {
        const std::vector<std::byte> tableBytes =
            makeBufferTableBytes(/*indirectUsageFlags=*/0x80u); // DeviceAddress only
        const auto tableView = CommandStreamBufferHandleTableValidator::validate(tableBytes);
        REQUIRE(tableView.has_value());
        auto bufferTable = VulkanBufferTable::createFromTable(vulkanContext, *tableView);
        REQUIRE(bufferTable.has_value());
        const auto recordingResult = recordSingleIndirect(*bufferTable, 0u);
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::MissingIndirectUsage);
    }

    SECTION("misaligned arguments offset") {
        const std::vector<std::byte> tableBytes = makeBufferTableBytes();
        const auto tableView = CommandStreamBufferHandleTableValidator::validate(tableBytes);
        REQUIRE(tableView.has_value());
        auto bufferTable = VulkanBufferTable::createFromTable(vulkanContext, *tableView);
        REQUIRE(bufferTable.has_value());
        const auto recordingResult = recordSingleIndirect(*bufferTable, 2u);
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::MisalignedIndirectOffset);
    }

    SECTION("arguments structure leaving the buffer") {
        const std::vector<std::byte> tableBytes = makeBufferTableBytes();
        const auto tableView = CommandStreamBufferHandleTableValidator::validate(tableBytes);
        REQUIRE(tableView.has_value());
        auto bufferTable = VulkanBufferTable::createFromTable(vulkanContext, *tableView);
        REQUIRE(bufferTable.has_value());
        const auto recordingResult = recordSingleIndirect(*bufferTable, 8u); // 8 + 12 > 16
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::IndirectArgumentsOutOfBounds);
    }
}
