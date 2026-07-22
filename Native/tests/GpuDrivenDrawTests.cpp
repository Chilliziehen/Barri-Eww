#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamBarrierBatchTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamBufferHandleTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamGraphicsPipelineTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamImageBarrierRecord.hpp"
#include "BarriEww/CommandStream/CommandStreamImageHandleTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamImageViewHandleTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamOpcode.hpp"
#include "BarriEww/CommandStream/CommandStreamPipelineHandleTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamRenderingTemplateTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamShaderModuleTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamValidator.hpp"
#include "BarriEww/Vulkan/CommandBufferRecorder.hpp"
#include "BarriEww/Vulkan/CommandBufferRecordingError.hpp"
#include "BarriEww/Vulkan/VulkanBufferTable.hpp"
#include "BarriEww/Vulkan/VulkanContext.hpp"
#include "BarriEww/Vulkan/VulkanGraphicsPipelineTable.hpp"
#include "BarriEww/Vulkan/VulkanImageTable.hpp"
#include "BarriEww/Vulkan/VulkanImageViewTable.hpp"
#include "BarriEww/Vulkan/VulkanPipelineTable.hpp"
#include "TestCommandStreamBuilder.hpp"
#include "TestVulkanDeviceHarness.hpp"

using barrieww::CommandBufferRecorder;
using barrieww::CommandBufferRecordingError;
using barrieww::CommandStreamBarrierBatchTableValidator;
using barrieww::CommandStreamBufferHandleTableValidator;
using barrieww::CommandStreamGraphicsPipelineTableValidator;
using barrieww::CommandStreamImageBarrierRecord;
using barrieww::CommandStreamImageHandleTableValidator;
using barrieww::CommandStreamImageViewHandleTableValidator;
using barrieww::CommandStreamOpcode;
using barrieww::CommandStreamPipelineHandleTableValidator;
using barrieww::CommandStreamRenderingTemplateTableValidator;
using barrieww::CommandStreamShaderModuleTableValidator;
using barrieww::CommandStreamValidator;
using barrieww::VulkanBufferTable;
using barrieww::VulkanContext;
using barrieww::VulkanGraphicsPipelineTable;
using barrieww::VulkanImageTable;
using barrieww::VulkanImageViewTable;
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
std::vector<std::byte> readShaderBlob(const char* relativeFilePath) {
    const std::string blobFilePath =
        std::string(BARRIEWW_TEST_DATA_DIRECTORY) + "/" + relativeFilePath;
    std::ifstream fileStream(blobFilePath, std::ios::binary | std::ios::ate);
    REQUIRE(fileStream.is_open());
    const std::streamsize fileByteCount = fileStream.tellg();
    fileStream.seekg(0);
    std::vector<std::byte> blobBytes(static_cast<std::size_t>(fileByteCount));
    fileStream.read(reinterpret_cast<char*>(blobBytes.data()), fileByteCount);
    return blobBytes;
}

/** A shader table embedding the given blobs at 8-aligned offsets, in slot order. */
std::vector<std::byte>
makeShaderTableBytes(const std::vector<std::vector<std::byte>>& shaderBlobs) {
    const std::uint64_t directoryEndOffset = 8u + 16u * shaderBlobs.size();
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, static_cast<std::uint32_t>(shaderBlobs.size()));
    appendValue(tableBytes, std::uint32_t{0});
    std::uint64_t blobOffset = directoryEndOffset;
    for (const std::vector<std::byte>& shaderBlob : shaderBlobs) {
        appendValue(tableBytes, blobOffset);
        appendValue(tableBytes, static_cast<std::uint64_t>(shaderBlob.size()));
        blobOffset = (blobOffset + shaderBlob.size() + 7u) & ~std::uint64_t{7u};
    }
    for (const std::vector<std::byte>& shaderBlob : shaderBlobs) {
        tableBytes.insert(tableBytes.end(), shaderBlob.begin(), shaderBlob.end());
        tableBytes.resize((tableBytes.size() + 7u) & ~std::size_t{7u});
    }
    return tableBytes;
}

/** One compute pipeline: the draw-argument writer (shader slot 0, 8-byte push range). */
std::vector<std::byte> makeComputePipelineTableBytes() {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{1}); // pipelineKind Compute
    appendValue(tableBytes, std::uint32_t{0}); // shaderModuleSlot
    appendValue(tableBytes, std::uint32_t{8}); // pushConstantByteSize
    appendValue(tableBytes, std::uint32_t{0});
    return tableBytes;
}

/** The vertex-pulling graphics pipeline over shader slots 1 (vertex) and 2 (fragment). */
std::vector<std::byte> makeGraphicsPipelineTableBytes() {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{1}); // vertexShaderModuleSlot
    appendValue(tableBytes, std::uint32_t{2}); // fragmentShaderModuleSlot
    appendValue(tableBytes, std::uint32_t{8}); // pushConstantByteSize
    appendValue(tableBytes, std::uint32_t{4}); // TriangleList
    appendValue(tableBytes, std::uint32_t{1}); // colorAttachmentCount
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{1}); // CullMode::None
    appendValue(tableBytes, std::uint32_t{1}); // FrontFace::CounterClockwise
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{1}); // R8G8B8A8Unorm
    for (std::uint32_t unusedIndex = 1; unusedIndex < 8u; ++unusedIndex) {
        appendValue(tableBytes, std::uint32_t{0});
    }
    return tableBytes;
}

/**
 * Three buffers: slot 0 = the 24-byte BDA vertex buffer (CPU-seeded), slot 1 = the
 * 256-byte readback target, slot 2 = the 16-byte GPU-WRITTEN draw-argument buffer
 * (Indirect | DeviceAddress, device-local — the CPU never touches it).
 */
std::vector<std::byte> makeBufferTableBytes() {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{3});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint64_t{24});
    appendValue(tableBytes, std::uint32_t{0x80}); // DeviceAddress
    appendValue(tableBytes, std::uint32_t{2});    // HostVisiblePersistentMapped
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint64_t{256});
    appendValue(tableBytes, std::uint32_t{0x2});  // TransferDestination
    appendValue(tableBytes, std::uint32_t{3});    // HostVisibleReadback
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint64_t{16});
    appendValue(tableBytes, std::uint32_t{0xC0}); // Indirect | DeviceAddress
    appendValue(tableBytes, std::uint32_t{1});    // DeviceLocal
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0});
    return tableBytes;
}

/** One 8x8 R8G8B8A8Unorm image usable as color attachment and transfer source. */
std::vector<std::byte> makeImageTableBytes() {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{2});
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{8});
    appendValue(tableBytes, std::uint32_t{8});
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0x11});
    appendValue(tableBytes, std::uint32_t{0});
    return tableBytes;
}

/** One 2D color image view over image slot 0 (mip 0, layer 0). */
std::vector<std::byte> makeImageViewTableBytes() {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{2});
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0x1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{1});
    return tableBytes;
}

/** One rendering template: single color attachment, Clear to opaque black, 8x8. */
std::vector<std::byte> makeRenderingTemplateTableBytes() {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::int32_t{0});
    appendValue(tableBytes, std::int32_t{0});
    appendValue(tableBytes, std::uint32_t{8});
    appendValue(tableBytes, std::uint32_t{8});
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint64_t{48});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{2});
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, 0.0f);
    appendValue(tableBytes, 0.0f);
    appendValue(tableBytes, 0.0f);
    appendValue(tableBytes, 1.0f);
    return tableBytes;
}

/** Appends one 64-byte image barrier record on image slot 0 (full color subresource). */
void appendImageBarrierRecord(std::vector<std::byte>& tableBytes,
                              std::uint64_t sourceStageMask,
                              std::uint64_t sourceAccessMask,
                              std::uint64_t destinationStageMask,
                              std::uint64_t destinationAccessMask,
                              std::uint32_t oldLayoutValue,
                              std::uint32_t newLayoutValue) {
    appendValue(tableBytes, sourceStageMask);
    appendValue(tableBytes, sourceAccessMask);
    appendValue(tableBytes, destinationStageMask);
    appendValue(tableBytes, destinationAccessMask);
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0x1});
    appendValue(tableBytes, oldLayoutValue);
    appendValue(tableBytes, newLayoutValue);
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, CommandStreamImageBarrierRecord::s_remainingCount);
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, CommandStreamImageBarrierRecord::s_remainingCount);
}

/**
 * Three barrier batches: batch 0 = the GPU-driven handoff (compute write of the draw
 * arguments -> indirect-command read, whole buffer slot 2), batch 1 = image Undefined
 * -> ColorAttachment, batch 2 = image ColorAttachment -> TransferSource. Directory
 * ends at 8 + 3*24 = 80; regions: 80 (56B buffer record), 136 and 200 (64B image
 * records each).
 */
std::vector<std::byte> makeGpuDrivenBarrierTableBytes() {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{3});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0}); // batch 0: 1 buffer barrier
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint64_t{80});
    appendValue(tableBytes, std::uint32_t{0}); // batch 1: 1 image barrier
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint64_t{136});
    appendValue(tableBytes, std::uint32_t{0}); // batch 2: 1 image barrier
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint64_t{200});
    // COMPUTE_SHADER/SHADER_WRITE -> DRAW_INDIRECT/INDIRECT_COMMAND_READ on slot 2.
    appendValue(tableBytes, std::uint64_t{0x800});
    appendValue(tableBytes, std::uint64_t{0x40});
    appendValue(tableBytes, std::uint64_t{0x2});
    appendValue(tableBytes, std::uint64_t{0x1});
    appendValue(tableBytes, std::uint32_t{2});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint64_t{0});
    appendValue(tableBytes, std::uint64_t{0xFFFFFFFFFFFFFFFFull}); // whole size
    appendImageBarrierRecord(tableBytes, 0x1u, 0u, 0x400u, 0x100u, 0u, 2u);
    appendImageBarrierRecord(tableBytes, 0x400u, 0x100u, 0x1000u, 0x800u, 2u, 5u);
    return tableBytes;
}

/** Pinned 56-byte CopyImageToBuffer payload (image 0 -> buffer slot 1, full 8x8 mip). */
std::vector<std::byte> makeCopyImageToBufferPayload() {
    std::vector<std::byte> payloadBytes;
    appendValue(payloadBytes, std::uint32_t{0});
    appendValue(payloadBytes, std::uint32_t{1});
    appendValue(payloadBytes, std::uint32_t{5});
    appendValue(payloadBytes, std::uint32_t{0x1});
    appendValue(payloadBytes, std::uint32_t{0});
    appendValue(payloadBytes, std::uint32_t{0});
    appendValue(payloadBytes, std::uint32_t{1});
    appendValue(payloadBytes, std::uint32_t{0});
    appendValue(payloadBytes, std::uint64_t{0});
    appendValue(payloadBytes, std::uint32_t{8});
    appendValue(payloadBytes, std::uint32_t{8});
    appendValue(payloadBytes, std::uint32_t{1});
    appendValue(payloadBytes, std::uint32_t{0});
    return payloadBytes;
}

/** Appends one command whose payload is a list of little-endian u32 values. */
void appendCommandWithValues(TestCommandStreamBuilder& streamBuilder,
                             CommandStreamOpcode opcode,
                             std::initializer_list<std::uint32_t> payloadValues) {
    std::vector<std::byte> payloadBytes;
    for (std::uint32_t payloadValue : payloadValues) {
        appendValue(payloadBytes, payloadValue);
    }
    streamBuilder.appendCommand(static_cast<std::uint16_t>(opcode), payloadBytes);
}

/** Pinned 24-byte DrawIndirect payload. */
std::vector<std::byte> makeDrawIndirectPayload(std::uint32_t bufferSlot,
                                               std::uint32_t drawCount,
                                               std::uint64_t bufferOffset,
                                               std::uint32_t strideByteCount) {
    std::vector<std::byte> payloadBytes;
    appendValue(payloadBytes, bufferSlot);
    appendValue(payloadBytes, drawCount);
    appendValue(payloadBytes, bufferOffset);
    appendValue(payloadBytes, strideByteCount);
    appendValue(payloadBytes, std::uint32_t{0});
    return payloadBytes;
}

/** Appends a SetViewport command covering the full 8x8 target. */
void appendSetViewport(TestCommandStreamBuilder& streamBuilder) {
    std::vector<std::byte> payloadBytes;
    appendValue(payloadBytes, 0.0f);
    appendValue(payloadBytes, 0.0f);
    appendValue(payloadBytes, 8.0f);
    appendValue(payloadBytes, 8.0f);
    appendValue(payloadBytes, 0.0f);
    appendValue(payloadBytes, 1.0f);
    streamBuilder.appendCommand(
        static_cast<std::uint16_t>(CommandStreamOpcode::SetViewport), payloadBytes);
}

/** Every materialized table one GPU-driven recording needs, built once per test. */
struct GpuDrivenFixture {
    std::vector<std::byte> bufferTableBytes = makeBufferTableBytes();
    std::vector<std::byte> imageTableBytes = makeImageTableBytes();
    std::vector<std::byte> imageViewTableBytes = makeImageViewTableBytes();
    std::vector<std::byte> renderingTemplateTableBytes = makeRenderingTemplateTableBytes();
    std::vector<std::byte> barrierTableBytes = makeGpuDrivenBarrierTableBytes();
    std::vector<std::byte> shaderTableBytes =
        makeShaderTableBytes({readShaderBlob("Shaders/WriteDrawArguments.comp.spv"),
                              readShaderBlob("Shaders/VertexPullByDeviceAddress.vert.spv"),
                              readShaderBlob("Shaders/SolidGreen.frag.spv")});
    std::vector<std::byte> computePipelineTableBytes = makeComputePipelineTableBytes();
    std::vector<std::byte> graphicsPipelineTableBytes = makeGraphicsPipelineTableBytes();
};

} // namespace

// The GPU-driven spine of ADR-0001, end to end in ONE prerecorded command buffer: a
// compute stage writes the VkDrawIndirectCommand {3,1,0,0} into a device-local buffer
// the CPU never touches, a barrier hands it to the indirect-command stage, and the
// graphics scope draws THROUGH those GPU-produced arguments — vertex data still pulled
// by device address. The CPU prerecorded every command without knowing the vertex
// count; the readback asserting 64 green texels proves the GPU decided the workload.
TEST_CASE("Recorded GPU-driven indirect draw round-trips rasterized texels",
          "[gpuDrivenDraw][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    if (!harness->supportsDynamicRendering()) {
        SKIP("driver does not support dynamic rendering (core Vulkan 1.3)");
    }
    if (!harness->supportsBufferDeviceAddress()) {
        SKIP("driver does not support bufferDeviceAddress (core Vulkan 1.2)");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};
    const GpuDrivenFixture fixture{};

    const auto bufferTableView =
        CommandStreamBufferHandleTableValidator::validate(fixture.bufferTableBytes);
    const auto imageTableView =
        CommandStreamImageHandleTableValidator::validate(fixture.imageTableBytes);
    const auto imageViewTableView =
        CommandStreamImageViewHandleTableValidator::validate(fixture.imageViewTableBytes);
    const auto renderingTemplateTableView =
        CommandStreamRenderingTemplateTableValidator::validate(
            fixture.renderingTemplateTableBytes);
    const auto barrierTableView =
        CommandStreamBarrierBatchTableValidator::validate(fixture.barrierTableBytes);
    const auto shaderTableView =
        CommandStreamShaderModuleTableValidator::validate(fixture.shaderTableBytes);
    const auto computePipelineTableView =
        CommandStreamPipelineHandleTableValidator::validate(
            fixture.computePipelineTableBytes);
    const auto graphicsPipelineTableView =
        CommandStreamGraphicsPipelineTableValidator::validate(
            fixture.graphicsPipelineTableBytes);
    REQUIRE(bufferTableView.has_value());
    REQUIRE(imageTableView.has_value());
    REQUIRE(imageViewTableView.has_value());
    REQUIRE(renderingTemplateTableView.has_value());
    REQUIRE(barrierTableView.has_value());
    REQUIRE(shaderTableView.has_value());
    REQUIRE(computePipelineTableView.has_value());
    REQUIRE(graphicsPipelineTableView.has_value());

    auto bufferTable = VulkanBufferTable::createFromTable(vulkanContext, *bufferTableView);
    REQUIRE(bufferTable.has_value());
    REQUIRE(bufferTable->slot(0u).deviceAddress != 0u);
    REQUIRE(bufferTable->slot(2u).deviceAddress != 0u);
    auto imageTable = VulkanImageTable::createFromTable(vulkanContext, *imageTableView);
    REQUIRE(imageTable.has_value());
    const auto sharedImageTable =
        std::make_shared<const VulkanImageTable>(std::move(*imageTable));
    auto imageViewTable =
        VulkanImageViewTable::createFromTable(sharedImageTable, *imageViewTableView);
    REQUIRE(imageViewTable.has_value());
    auto computePipelineTable = VulkanPipelineTable::createFromTables(
        vulkanContext, *computePipelineTableView, *shaderTableView);
    REQUIRE(computePipelineTable.has_value());
    auto graphicsPipelineTable = VulkanGraphicsPipelineTable::createFromTables(
        vulkanContext, *graphicsPipelineTableView, *shaderTableView);
    REQUIRE(graphicsPipelineTable.has_value());

    // The CPU seeds ONLY the fullscreen-triangle positions; the draw arguments buffer
    // (slot 2) stays untouched — the compute stage below is its sole writer.
    const float trianglePositions[6] = {-1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f};
    std::byte* vertexBufferBytes = bufferTable->slot(0u).mappedPointer;
    REQUIRE(vertexBufferBytes != nullptr);
    std::memcpy(vertexBufferBytes, trianglePositions, sizeof trianglePositions);

    TestCommandStreamBuilder streamBuilder{0u};
    appendCommandWithValues(streamBuilder, CommandStreamOpcode::BindComputePipeline,
                            {0u, 0u});
    appendCommandWithValues(streamBuilder, CommandStreamOpcode::PushBufferDeviceAddress,
                            {2u, 0u}); // draw-argument buffer address -> compute push
    appendCommandWithValues(streamBuilder, CommandStreamOpcode::Dispatch, {1u, 1u, 1u, 0u});
    appendCommandWithValues(streamBuilder, CommandStreamOpcode::ExecuteBarrierBatch,
                            {0u, 0u}); // compute write -> indirect read
    appendCommandWithValues(streamBuilder, CommandStreamOpcode::ExecuteBarrierBatch,
                            {1u, 0u}); // Undefined -> ColorAttachment
    appendCommandWithValues(streamBuilder, CommandStreamOpcode::BeginRendering, {0u, 0u});
    appendCommandWithValues(streamBuilder, CommandStreamOpcode::BindGraphicsPipeline,
                            {0u, 0u});
    appendCommandWithValues(streamBuilder, CommandStreamOpcode::PushBufferDeviceAddress,
                            {0u, 0u}); // vertex buffer address -> graphics push
    appendSetViewport(streamBuilder);
    appendCommandWithValues(streamBuilder, CommandStreamOpcode::SetScissor,
                            {0u, 0u, 8u, 8u});
    streamBuilder.appendCommand(
        static_cast<std::uint16_t>(CommandStreamOpcode::DrawIndirect),
        makeDrawIndirectPayload(2u, 1u, 0u, 16u));
    streamBuilder.appendCommand(
        static_cast<std::uint16_t>(CommandStreamOpcode::EndRendering), {});
    appendCommandWithValues(streamBuilder, CommandStreamOpcode::ExecuteBarrierBatch,
                            {2u, 0u}); // ColorAttachment -> TransferSource
    streamBuilder.appendCommand(
        static_cast<std::uint16_t>(CommandStreamOpcode::CopyImageToBuffer),
        makeCopyImageToBufferPayload());
    const std::vector<std::byte> streamBytes = streamBuilder.build();
    const auto streamView = CommandStreamValidator::validate(streamBytes);
    REQUIRE(streamView.has_value());

    const VkCommandBuffer commandBuffer = harness->allocateCommandBuffer();
    REQUIRE(CommandBufferRecorder::record(
                commandBuffer, *streamView,
                {.bufferTable = &*bufferTable,
                 .barrierBatchTableView = &*barrierTableView,
                 .pipelineTable = &*computePipelineTable,
                 .imageTable = sharedImageTable.get(),
                 .imageViewTable = &*imageViewTable,
                 .renderingTemplateTableView = &*renderingTemplateTableView,
                 .graphicsPipelineTable = &*graphicsPipelineTable})
                .has_value());
    REQUIRE(harness->submitAndWait(commandBuffer));

    const std::byte* readbackBytes = bufferTable->slot(1u).mappedPointer;
    REQUIRE(readbackBytes != nullptr);
    for (std::uint32_t texelIndex = 0; texelIndex < 64u; ++texelIndex) {
        REQUIRE(readbackBytes[texelIndex * 4u + 0u] == std::byte{0x00}); // red
        REQUIRE(readbackBytes[texelIndex * 4u + 1u] == std::byte{0xFF}); // green
        REQUIRE(readbackBytes[texelIndex * 4u + 2u] == std::byte{0x00}); // blue
        REQUIRE(readbackBytes[texelIndex * 4u + 3u] == std::byte{0xFF}); // alpha
    }
}

// DrawIndirect recording rejects each rule violation with its precise failure code.
TEST_CASE("GPU-driven indirect draw recording rejects invalid streams with the precise failure",
          "[gpuDrivenDraw][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    if (!harness->supportsDynamicRendering()) {
        SKIP("driver does not support dynamic rendering (core Vulkan 1.3)");
    }
    if (!harness->supportsBufferDeviceAddress()) {
        SKIP("driver does not support bufferDeviceAddress (core Vulkan 1.2)");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};
    const GpuDrivenFixture fixture{};

    const auto bufferTableView =
        CommandStreamBufferHandleTableValidator::validate(fixture.bufferTableBytes);
    const auto imageTableView =
        CommandStreamImageHandleTableValidator::validate(fixture.imageTableBytes);
    const auto imageViewTableView =
        CommandStreamImageViewHandleTableValidator::validate(fixture.imageViewTableBytes);
    const auto renderingTemplateTableView =
        CommandStreamRenderingTemplateTableValidator::validate(
            fixture.renderingTemplateTableBytes);
    const auto shaderTableView =
        CommandStreamShaderModuleTableValidator::validate(fixture.shaderTableBytes);
    const auto graphicsPipelineTableView =
        CommandStreamGraphicsPipelineTableValidator::validate(
            fixture.graphicsPipelineTableBytes);
    REQUIRE(bufferTableView.has_value());
    REQUIRE(imageTableView.has_value());
    REQUIRE(imageViewTableView.has_value());
    REQUIRE(renderingTemplateTableView.has_value());
    REQUIRE(shaderTableView.has_value());
    REQUIRE(graphicsPipelineTableView.has_value());

    auto bufferTable = VulkanBufferTable::createFromTable(vulkanContext, *bufferTableView);
    auto imageTable = VulkanImageTable::createFromTable(vulkanContext, *imageTableView);
    REQUIRE(bufferTable.has_value());
    REQUIRE(imageTable.has_value());
    const auto sharedImageTable =
        std::make_shared<const VulkanImageTable>(std::move(*imageTable));
    auto imageViewTable =
        VulkanImageViewTable::createFromTable(sharedImageTable, *imageViewTableView);
    auto graphicsPipelineTable = VulkanGraphicsPipelineTable::createFromTables(
        vulkanContext, *graphicsPipelineTableView, *shaderTableView);
    REQUIRE(imageViewTable.has_value());
    REQUIRE(graphicsPipelineTable.has_value());

    // Records one DrawIndirect payload behind a fully prepared scope (open template 0,
    // bound pipeline, viewport and scissor set) and returns the recording result.
    const auto recordIndirectDraw = [&](std::vector<std::byte> drawIndirectPayload,
                                        bool prepareScope = true) {
        TestCommandStreamBuilder streamBuilder{0u};
        if (prepareScope) {
            appendCommandWithValues(streamBuilder, CommandStreamOpcode::BeginRendering,
                                    {0u, 0u});
            appendCommandWithValues(streamBuilder,
                                    CommandStreamOpcode::BindGraphicsPipeline, {0u, 0u});
            appendSetViewport(streamBuilder);
            appendCommandWithValues(streamBuilder, CommandStreamOpcode::SetScissor,
                                    {0u, 0u, 8u, 8u});
        }
        streamBuilder.appendCommand(
            static_cast<std::uint16_t>(CommandStreamOpcode::DrawIndirect),
            drawIndirectPayload);
        const std::vector<std::byte> streamBytes = streamBuilder.build();
        const auto streamView = CommandStreamValidator::validate(streamBytes);
        REQUIRE(streamView.has_value());
        return CommandBufferRecorder::record(
            harness->allocateCommandBuffer(), *streamView,
            {.bufferTable = &*bufferTable,
             .imageTable = sharedImageTable.get(),
             .imageViewTable = &*imageViewTable,
             .renderingTemplateTableView = &*renderingTemplateTableView,
             .graphicsPipelineTable = &*graphicsPipelineTable});
    };

    SECTION("outside a rendering scope") {
        const auto recordingResult = recordIndirectDraw(
            makeDrawIndirectPayload(2u, 1u, 0u, 16u), /*prepareScope=*/false);
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::GraphicsCommandOutsideRenderingScope);
    }

    SECTION("draw count other than one") {
        const auto recordingResult =
            recordIndirectDraw(makeDrawIndirectPayload(2u, 2u, 0u, 16u));
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::UnsupportedIndirectDrawCount);
    }

    SECTION("stride other than sixteen") {
        const auto recordingResult =
            recordIndirectDraw(makeDrawIndirectPayload(2u, 1u, 0u, 8u));
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::InvalidIndirectDrawStride);
    }

    SECTION("buffer without the Indirect usage bit") {
        // Slot 0 is the vertex buffer (DeviceAddress only).
        const auto recordingResult =
            recordIndirectDraw(makeDrawIndirectPayload(0u, 1u, 0u, 16u));
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::MissingIndirectUsage);
    }

    SECTION("misaligned arguments offset") {
        const auto recordingResult =
            recordIndirectDraw(makeDrawIndirectPayload(2u, 1u, 2u, 16u));
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::MisalignedIndirectOffset);
    }

    SECTION("arguments leaving the buffer") {
        // Offset 8 + one 16-byte structure leaves the 16-byte buffer.
        const auto recordingResult =
            recordIndirectDraw(makeDrawIndirectPayload(2u, 1u, 8u, 16u));
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::IndirectArgumentsOutOfBounds);
    }
}
