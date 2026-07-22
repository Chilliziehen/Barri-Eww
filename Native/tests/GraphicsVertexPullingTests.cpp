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
using barrieww::CommandStreamRenderingTemplateTableValidator;
using barrieww::CommandStreamShaderModuleTableValidator;
using barrieww::CommandStreamValidator;
using barrieww::VulkanBufferTable;
using barrieww::VulkanContext;
using barrieww::VulkanGraphicsPipelineTable;
using barrieww::VulkanImageTable;
using barrieww::VulkanImageViewTable;
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

/** A two-blob shader table: slot 0 = vertex blob, slot 1 = fragment blob. */
std::vector<std::byte> makeShaderTableBytes(const std::vector<std::byte>& vertexBlob,
                                            const std::vector<std::byte>& fragmentBlob) {
    const std::uint64_t vertexBlobOffset = 40u;
    const std::uint64_t fragmentBlobOffset =
        (vertexBlobOffset + vertexBlob.size() + 7u) & ~std::uint64_t{7u};
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{2});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, vertexBlobOffset);
    appendValue(tableBytes, static_cast<std::uint64_t>(vertexBlob.size()));
    appendValue(tableBytes, fragmentBlobOffset);
    appendValue(tableBytes, static_cast<std::uint64_t>(fragmentBlob.size()));
    tableBytes.insert(tableBytes.end(), vertexBlob.begin(), vertexBlob.end());
    tableBytes.resize(fragmentBlobOffset);
    tableBytes.insert(tableBytes.end(), fragmentBlob.begin(), fragmentBlob.end());
    return tableBytes;
}

/**
 * One graphics pipeline: vertex-pulling vertex stage (slot 0) + solid-color fragment
 * (slot 1), an 8-byte push range carrying the vertex buffer's device address,
 * TriangleList, no culling, one R8G8B8A8Unorm color attachment, no depth.
 */
std::vector<std::byte> makeGraphicsPipelineTableBytes() {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0}); // vertexShaderModuleSlot
    appendValue(tableBytes, std::uint32_t{1}); // fragmentShaderModuleSlot
    appendValue(tableBytes, std::uint32_t{8}); // pushConstantByteSize (device address)
    appendValue(tableBytes, std::uint32_t{4}); // TriangleList
    appendValue(tableBytes, std::uint32_t{1}); // colorAttachmentCount
    appendValue(tableBytes, std::uint32_t{0}); // no depth attachment
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{1}); // CullMode::None
    appendValue(tableBytes, std::uint32_t{1}); // FrontFace::CounterClockwise
    appendValue(tableBytes, std::uint32_t{0}); // reservedFlags
    appendValue(tableBytes, std::uint32_t{1}); // R8G8B8A8Unorm
    for (std::uint32_t unusedIndex = 1; unusedIndex < 8u; ++unusedIndex) {
        appendValue(tableBytes, std::uint32_t{0});
    }
    return tableBytes;
}

/**
 * Two buffers: slot 0 = the 24-byte vertex buffer the shader pulls through its device
 * address (persistently mapped so the test can seed positions from the CPU), slot 1 =
 * the 256-byte readback target.
 */
std::vector<std::byte> makeBufferTableBytes() {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{2});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint64_t{24});
    appendValue(tableBytes, std::uint32_t{0x80}); // DeviceAddress
    appendValue(tableBytes, std::uint32_t{2});    // HostVisiblePersistentMapped
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint64_t{256});
    appendValue(tableBytes, std::uint32_t{0x2}); // TransferDestination
    appendValue(tableBytes, std::uint32_t{3});   // HostVisibleReadback
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
    appendValue(tableBytes, std::uint32_t{0x11}); // ColorAttachment | TransferSource
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
    appendValue(tableBytes, std::uint32_t{2}); // ColorAttachment layout
    appendValue(tableBytes, std::uint32_t{1}); // Clear
    appendValue(tableBytes, std::uint32_t{0}); // Store
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
 * Two image-barrier batches: batch 0 = Undefined -> ColorAttachment, batch 1 =
 * ColorAttachment -> TransferSource. Directory ends at 8 + 2*24 = 56.
 */
std::vector<std::byte> makeRenderTransitionBarrierTableBytes() {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{2});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint64_t{56});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint64_t{120});
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

} // namespace

// The toolchain's first REAL geometry: the vertex stage pulls three vec2 positions out
// of an actual vertex buffer through its device address (pushed as an 8-byte push
// constant into the graphics layout's vertex+fragment range), assembling a fullscreen
// triangle over an 8x8 target cleared to black — then readback asserts all 64 texels
// are the fragment green. This closes the §9.14 geometry path (no vertex input state;
// resources by device address) with pipeline-rasterized pixels fed by CPU-seeded data.
TEST_CASE("Recorded graphics draw pulls vertices through a buffer device address",
          "[graphicsVertexPulling][gpu]") {
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

    const std::vector<std::byte> bufferTableBytes = makeBufferTableBytes();
    const std::vector<std::byte> imageTableBytes = makeImageTableBytes();
    const std::vector<std::byte> imageViewTableBytes = makeImageViewTableBytes();
    const std::vector<std::byte> renderingTemplateTableBytes =
        makeRenderingTemplateTableBytes();
    const std::vector<std::byte> barrierTableBytes = makeRenderTransitionBarrierTableBytes();
    const std::vector<std::byte> vertexBlob =
        readShaderBlob("Shaders/VertexPullByDeviceAddress.vert.spv");
    const std::vector<std::byte> fragmentBlob = readShaderBlob("Shaders/SolidGreen.frag.spv");
    const std::vector<std::byte> shaderTableBytes =
        makeShaderTableBytes(vertexBlob, fragmentBlob);
    const std::vector<std::byte> graphicsPipelineTableBytes =
        makeGraphicsPipelineTableBytes();

    const auto bufferTableView =
        CommandStreamBufferHandleTableValidator::validate(bufferTableBytes);
    const auto imageTableView =
        CommandStreamImageHandleTableValidator::validate(imageTableBytes);
    const auto imageViewTableView =
        CommandStreamImageViewHandleTableValidator::validate(imageViewTableBytes);
    const auto renderingTemplateTableView =
        CommandStreamRenderingTemplateTableValidator::validate(renderingTemplateTableBytes);
    const auto barrierTableView =
        CommandStreamBarrierBatchTableValidator::validate(barrierTableBytes);
    const auto shaderTableView =
        CommandStreamShaderModuleTableValidator::validate(shaderTableBytes);
    const auto graphicsPipelineTableView =
        CommandStreamGraphicsPipelineTableValidator::validate(graphicsPipelineTableBytes);
    REQUIRE(bufferTableView.has_value());
    REQUIRE(imageTableView.has_value());
    REQUIRE(imageViewTableView.has_value());
    REQUIRE(renderingTemplateTableView.has_value());
    REQUIRE(barrierTableView.has_value());
    REQUIRE(shaderTableView.has_value());
    REQUIRE(graphicsPipelineTableView.has_value());

    auto bufferTable = VulkanBufferTable::createFromTable(vulkanContext, *bufferTableView);
    REQUIRE(bufferTable.has_value());
    REQUIRE(bufferTable->slot(0u).deviceAddress != 0u);
    auto imageTable = VulkanImageTable::createFromTable(vulkanContext, *imageTableView);
    REQUIRE(imageTable.has_value());
    const auto sharedImageTable =
        std::make_shared<const VulkanImageTable>(std::move(*imageTable));
    auto imageViewTable =
        VulkanImageViewTable::createFromTable(sharedImageTable, *imageViewTableView);
    REQUIRE(imageViewTable.has_value());
    auto graphicsPipelineTable = VulkanGraphicsPipelineTable::createFromTables(
        vulkanContext, *graphicsPipelineTableView, *shaderTableView);
    REQUIRE(graphicsPipelineTable.has_value());

    // Seed the fullscreen triangle from the CPU through the persistent mapping — the
    // GPU sees it only through the pushed device address.
    const float trianglePositions[6] = {-1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f};
    std::byte* vertexBufferBytes = bufferTable->slot(0u).mappedPointer;
    REQUIRE(vertexBufferBytes != nullptr);
    std::memcpy(vertexBufferBytes, trianglePositions, sizeof trianglePositions);

    TestCommandStreamBuilder streamBuilder{0u};
    appendCommandWithValues(streamBuilder, CommandStreamOpcode::ExecuteBarrierBatch,
                            {0u, 0u});
    appendCommandWithValues(streamBuilder, CommandStreamOpcode::BeginRendering, {0u, 0u});
    appendCommandWithValues(streamBuilder, CommandStreamOpcode::BindGraphicsPipeline,
                            {0u, 0u});
    appendCommandWithValues(streamBuilder, CommandStreamOpcode::PushBufferDeviceAddress,
                            {0u, 0u}); // vertex buffer slot 0 -> push offset 0
    appendSetViewport(streamBuilder);
    appendCommandWithValues(streamBuilder, CommandStreamOpcode::SetScissor,
                            {0u, 0u, 8u, 8u});
    appendCommandWithValues(streamBuilder, CommandStreamOpcode::Draw, {3u, 1u, 0u, 0u});
    streamBuilder.appendCommand(
        static_cast<std::uint16_t>(CommandStreamOpcode::EndRendering), {});
    appendCommandWithValues(streamBuilder, CommandStreamOpcode::ExecuteBarrierBatch,
                            {1u, 0u});
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

// The graphics push path enforces the same range discipline as the compute path: the
// pushed 8 bytes must fit the graphics layout's declared range.
TEST_CASE("Graphics push constant recording rejects range violations with the precise failure",
          "[graphicsVertexPulling][gpu]") {
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

    const std::vector<std::byte> bufferTableBytes = makeBufferTableBytes();
    const std::vector<std::byte> imageTableBytes = makeImageTableBytes();
    const std::vector<std::byte> imageViewTableBytes = makeImageViewTableBytes();
    const std::vector<std::byte> renderingTemplateTableBytes =
        makeRenderingTemplateTableBytes();
    const std::vector<std::byte> vertexBlob =
        readShaderBlob("Shaders/VertexPullByDeviceAddress.vert.spv");
    const std::vector<std::byte> fragmentBlob = readShaderBlob("Shaders/SolidGreen.frag.spv");
    const std::vector<std::byte> shaderTableBytes =
        makeShaderTableBytes(vertexBlob, fragmentBlob);
    const std::vector<std::byte> graphicsPipelineTableBytes =
        makeGraphicsPipelineTableBytes();

    const auto bufferTableView =
        CommandStreamBufferHandleTableValidator::validate(bufferTableBytes);
    const auto imageTableView =
        CommandStreamImageHandleTableValidator::validate(imageTableBytes);
    const auto imageViewTableView =
        CommandStreamImageViewHandleTableValidator::validate(imageViewTableBytes);
    const auto renderingTemplateTableView =
        CommandStreamRenderingTemplateTableValidator::validate(renderingTemplateTableBytes);
    const auto shaderTableView =
        CommandStreamShaderModuleTableValidator::validate(shaderTableBytes);
    const auto graphicsPipelineTableView =
        CommandStreamGraphicsPipelineTableValidator::validate(graphicsPipelineTableBytes);
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

    // Push offset 4 with the 8-byte address leaves the pipeline's declared 8-byte range.
    TestCommandStreamBuilder streamBuilder{0u};
    appendCommandWithValues(streamBuilder, CommandStreamOpcode::BeginRendering, {0u, 0u});
    appendCommandWithValues(streamBuilder, CommandStreamOpcode::BindGraphicsPipeline,
                            {0u, 0u});
    appendCommandWithValues(streamBuilder, CommandStreamOpcode::PushBufferDeviceAddress,
                            {0u, 4u});
    const std::vector<std::byte> streamBytes = streamBuilder.build();
    const auto streamView = CommandStreamValidator::validate(streamBytes);
    REQUIRE(streamView.has_value());

    const auto recordingResult = CommandBufferRecorder::record(
        harness->allocateCommandBuffer(), *streamView,
        {.bufferTable = &*bufferTable,
         .imageTable = sharedImageTable.get(),
         .imageViewTable = &*imageViewTable,
         .renderingTemplateTableView = &*renderingTemplateTableView,
         .graphicsPipelineTable = &*graphicsPipelineTable});
    REQUIRE_FALSE(recordingResult.has_value());
    REQUIRE(recordingResult.error().error
            == CommandBufferRecordingError::PushConstantRangeExceeded);
}
