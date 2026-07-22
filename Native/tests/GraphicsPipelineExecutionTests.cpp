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
#include "BarriEww/Vulkan/VulkanGraphicsPipelineTableCreationError.hpp"
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
using barrieww::VulkanGraphicsPipelineTableCreationError;
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
    // Directory ends at 8 + 2*16 = 40 (8-byte aligned); fragment blob starts at an
    // 8-byte-aligned offset because SPIR-V blob sizes are multiples of 4 — round up.
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
 * One graphics pipeline record: vertex slot 0 + fragment slot 1, TriangleList, no
 * culling, counter-clockwise, colorAttachmentCount attachments of colorFormatValue, no
 * push constants. The parameters shape the format/count/depth-mismatch rejection tests
 * and the depth materialization test (depth testing stays disabled throughout).
 */
std::vector<std::byte> makeGraphicsPipelineTableBytes(std::uint32_t colorFormatValue = 1u,
                                                      std::uint32_t colorAttachmentCount = 1u,
                                                      std::uint32_t depthFormatValue = 0u) {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0}); // vertexShaderModuleSlot
    appendValue(tableBytes, std::uint32_t{1}); // fragmentShaderModuleSlot
    appendValue(tableBytes, std::uint32_t{0}); // pushConstantByteSize
    appendValue(tableBytes, std::uint32_t{4}); // TriangleList
    appendValue(tableBytes, colorAttachmentCount);
    appendValue(tableBytes, depthFormatValue);
    appendValue(tableBytes, std::uint32_t{0}); // depthTestEnable
    appendValue(tableBytes, std::uint32_t{0}); // depthWriteEnable
    appendValue(tableBytes, std::uint32_t{0}); // depthCompareOperationValue
    appendValue(tableBytes, std::uint32_t{1}); // CullMode::None
    appendValue(tableBytes, std::uint32_t{1}); // FrontFace::CounterClockwise
    appendValue(tableBytes, std::uint32_t{0}); // reservedFlags
    for (std::uint32_t formatIndex = 0; formatIndex < 8u; ++formatIndex) {
        appendValue(tableBytes,
                    formatIndex < colorAttachmentCount ? colorFormatValue
                                                       : std::uint32_t{0});
    }
    return tableBytes;
}

/** One 8x8 R8G8B8A8Unorm image usable as color attachment and transfer source. */
std::vector<std::byte> makeImageTableBytes() {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{2}); // TwoDimensional
    appendValue(tableBytes, std::uint32_t{1}); // R8G8B8A8Unorm
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
    appendValue(tableBytes, std::uint32_t{0});   // imageSlot
    appendValue(tableBytes, std::uint32_t{2});   // TwoDimensional
    appendValue(tableBytes, std::uint32_t{1});   // R8G8B8A8Unorm
    appendValue(tableBytes, std::uint32_t{0x1}); // Color aspect
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{1});
    return tableBytes;
}

/**
 * One rendering template: a single color attachment on image-view slot 0, layout
 * ColorAttachment, loadOp Clear to opaque black, storeOp Store, over an 8x8 area.
 */
std::vector<std::byte> makeRenderingTemplateTableBytes() {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{1});  // colorAttachmentCount
    appendValue(tableBytes, std::uint32_t{0});  // depthAttachmentPresent
    appendValue(tableBytes, std::int32_t{0});
    appendValue(tableBytes, std::int32_t{0});
    appendValue(tableBytes, std::uint32_t{8});
    appendValue(tableBytes, std::uint32_t{8});
    appendValue(tableBytes, std::uint32_t{1});  // layerCount
    appendValue(tableBytes, std::uint32_t{0});  // viewMask
    appendValue(tableBytes, std::uint64_t{48}); // attachmentsByteOffset
    appendValue(tableBytes, std::uint32_t{0});  // imageViewSlot
    appendValue(tableBytes, std::uint32_t{2});  // ColorAttachment layout
    appendValue(tableBytes, std::uint32_t{1});  // Clear
    appendValue(tableBytes, std::uint32_t{0});  // Store
    appendValue(tableBytes, 0.0f);              // clear R (black background)
    appendValue(tableBytes, 0.0f);              // clear G
    appendValue(tableBytes, 0.0f);              // clear B
    appendValue(tableBytes, 1.0f);              // clear A
    return tableBytes;
}

/** One 256-byte readback buffer (8*8 texels * 4 bytes). */
std::vector<std::byte> makeReadbackBufferTableBytes() {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint64_t{256});
    appendValue(tableBytes, std::uint32_t{0x2}); // TransferDestination
    appendValue(tableBytes, std::uint32_t{3});   // HostVisibleReadback
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0});
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

/** Pinned 56-byte CopyImageToBuffer payload (mip 0, layer 0, full 8x8 extent). */
std::vector<std::byte> makeCopyImageToBufferPayload() {
    std::vector<std::byte> payloadBytes;
    appendValue(payloadBytes, std::uint32_t{0});   // imageSlot
    appendValue(payloadBytes, std::uint32_t{0});   // bufferSlot
    appendValue(payloadBytes, std::uint32_t{5});   // TransferSource layout
    appendValue(payloadBytes, std::uint32_t{0x1}); // Color aspect
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

/** Appends an ExecuteBarrierBatch command referencing one batch slot. */
void appendExecuteBarrierBatch(TestCommandStreamBuilder& streamBuilder,
                               std::uint32_t batchSlot) {
    std::vector<std::byte> payloadBytes;
    appendValue(payloadBytes, batchSlot);
    appendValue(payloadBytes, std::uint32_t{0});
    streamBuilder.appendCommand(
        static_cast<std::uint16_t>(CommandStreamOpcode::ExecuteBarrierBatch), payloadBytes);
}

/** Appends a BeginRendering command referencing template slot 0. */
void appendBeginRendering(TestCommandStreamBuilder& streamBuilder) {
    std::vector<std::byte> payloadBytes;
    appendValue(payloadBytes, std::uint32_t{0});
    appendValue(payloadBytes, std::uint32_t{0});
    streamBuilder.appendCommand(
        static_cast<std::uint16_t>(CommandStreamOpcode::BeginRendering), payloadBytes);
}

/** Appends a BindGraphicsPipeline command referencing one pipeline slot. */
void appendBindGraphicsPipeline(TestCommandStreamBuilder& streamBuilder,
                                std::uint32_t graphicsPipelineSlot) {
    std::vector<std::byte> payloadBytes;
    appendValue(payloadBytes, graphicsPipelineSlot);
    appendValue(payloadBytes, std::uint32_t{0});
    streamBuilder.appendCommand(
        static_cast<std::uint16_t>(CommandStreamOpcode::BindGraphicsPipeline), payloadBytes);
}

/** Appends a SetViewport command covering the full 8x8 target. */
void appendSetViewport(TestCommandStreamBuilder& streamBuilder) {
    std::vector<std::byte> payloadBytes;
    appendValue(payloadBytes, 0.0f); // x
    appendValue(payloadBytes, 0.0f); // y
    appendValue(payloadBytes, 8.0f); // width
    appendValue(payloadBytes, 8.0f); // height
    appendValue(payloadBytes, 0.0f); // minDepth
    appendValue(payloadBytes, 1.0f); // maxDepth
    streamBuilder.appendCommand(
        static_cast<std::uint16_t>(CommandStreamOpcode::SetViewport), payloadBytes);
}

/** Appends a SetScissor command covering the full 8x8 target. */
void appendSetScissor(TestCommandStreamBuilder& streamBuilder) {
    std::vector<std::byte> payloadBytes;
    appendValue(payloadBytes, std::int32_t{0});
    appendValue(payloadBytes, std::int32_t{0});
    appendValue(payloadBytes, std::uint32_t{8});
    appendValue(payloadBytes, std::uint32_t{8});
    streamBuilder.appendCommand(
        static_cast<std::uint16_t>(CommandStreamOpcode::SetScissor), payloadBytes);
}

/** Appends a Draw command for one non-instanced triangle. */
void appendDraw(TestCommandStreamBuilder& streamBuilder) {
    std::vector<std::byte> payloadBytes;
    appendValue(payloadBytes, std::uint32_t{3}); // vertexCount
    appendValue(payloadBytes, std::uint32_t{1}); // instanceCount
    appendValue(payloadBytes, std::uint32_t{0}); // firstVertex
    appendValue(payloadBytes, std::uint32_t{0}); // firstInstance
    streamBuilder.appendCommand(static_cast<std::uint16_t>(CommandStreamOpcode::Draw),
                                payloadBytes);
}

/** Everything one graphics recording test materializes once (all tables green). */
struct GraphicsExecutionFixture {
    std::vector<std::byte> imageTableBytes = makeImageTableBytes();
    std::vector<std::byte> imageViewTableBytes = makeImageViewTableBytes();
    std::vector<std::byte> renderingTemplateTableBytes = makeRenderingTemplateTableBytes();
    std::vector<std::byte> bufferTableBytes = makeReadbackBufferTableBytes();
    std::vector<std::byte> barrierTableBytes = makeRenderTransitionBarrierTableBytes();
    std::vector<std::byte> vertexBlob = readShaderBlob("Shaders/FullscreenTriangle.vert.spv");
    std::vector<std::byte> fragmentBlob = readShaderBlob("Shaders/SolidGreen.frag.spv");
    std::vector<std::byte> shaderTableBytes = makeShaderTableBytes(vertexBlob, fragmentBlob);
    std::vector<std::byte> graphicsPipelineTableBytes = makeGraphicsPipelineTableBytes();
};

} // namespace

// The toolchain's first PIPELINE-produced pixels, prerecorded as ONE command buffer:
// transition Undefined -> ColorAttachment, open the rendering scope (loadOp clears to
// opaque black), bind the graphics pipeline, set the dynamic viewport/scissor, draw a
// fullscreen triangle whose fragment shader writes opaque green, close the scope,
// transition to TransferSource, copy the mip into the readback buffer — then assert all
// 64 texels are byte-exactly {0, 255, 0, 255}: green proves rasterization ran (a failed
// draw would leave the black clear), and the vertex stage pulled its triangle from
// gl_VertexIndex alone (no vertex input state, §9.14).
TEST_CASE("Recorded graphics pipeline draw round-trips rasterized texels",
          "[graphicsPipeline][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    if (!harness->supportsDynamicRendering()) {
        SKIP("driver does not support dynamic rendering (core Vulkan 1.3)");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};
    const GraphicsExecutionFixture fixture{};

    const auto imageTableView =
        CommandStreamImageHandleTableValidator::validate(fixture.imageTableBytes);
    const auto imageViewTableView =
        CommandStreamImageViewHandleTableValidator::validate(fixture.imageViewTableBytes);
    const auto renderingTemplateTableView =
        CommandStreamRenderingTemplateTableValidator::validate(
            fixture.renderingTemplateTableBytes);
    const auto bufferTableView =
        CommandStreamBufferHandleTableValidator::validate(fixture.bufferTableBytes);
    const auto barrierTableView =
        CommandStreamBarrierBatchTableValidator::validate(fixture.barrierTableBytes);
    const auto shaderTableView =
        CommandStreamShaderModuleTableValidator::validate(fixture.shaderTableBytes);
    const auto graphicsPipelineTableView =
        CommandStreamGraphicsPipelineTableValidator::validate(
            fixture.graphicsPipelineTableBytes);
    REQUIRE(imageTableView.has_value());
    REQUIRE(imageViewTableView.has_value());
    REQUIRE(renderingTemplateTableView.has_value());
    REQUIRE(bufferTableView.has_value());
    REQUIRE(barrierTableView.has_value());
    REQUIRE(shaderTableView.has_value());
    REQUIRE(graphicsPipelineTableView.has_value());

    auto imageTable = VulkanImageTable::createFromTable(vulkanContext, *imageTableView);
    REQUIRE(imageTable.has_value());
    const auto sharedImageTable =
        std::make_shared<const VulkanImageTable>(std::move(*imageTable));
    auto imageViewTable =
        VulkanImageViewTable::createFromTable(sharedImageTable, *imageViewTableView);
    REQUIRE(imageViewTable.has_value());
    auto bufferTable = VulkanBufferTable::createFromTable(vulkanContext, *bufferTableView);
    REQUIRE(bufferTable.has_value());
    auto graphicsPipelineTable = VulkanGraphicsPipelineTable::createFromTables(
        vulkanContext, *graphicsPipelineTableView, *shaderTableView);
    REQUIRE(graphicsPipelineTable.has_value());
    REQUIRE(graphicsPipelineTable->slotCount() == 1u);
    REQUIRE(graphicsPipelineTable->slot(0u).pipeline != VK_NULL_HANDLE);

    TestCommandStreamBuilder streamBuilder{0u};
    appendExecuteBarrierBatch(streamBuilder, 0u); // Undefined -> ColorAttachment
    appendBeginRendering(streamBuilder);
    appendBindGraphicsPipeline(streamBuilder, 0u);
    appendSetViewport(streamBuilder);
    appendSetScissor(streamBuilder);
    appendDraw(streamBuilder);
    streamBuilder.appendCommand(
        static_cast<std::uint16_t>(CommandStreamOpcode::EndRendering), {});
    appendExecuteBarrierBatch(streamBuilder, 1u); // ColorAttachment -> TransferSource
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

    const std::byte* readbackBytes = bufferTable->slot(0u).mappedPointer;
    REQUIRE(readbackBytes != nullptr);
    for (std::uint32_t texelIndex = 0; texelIndex < 64u; ++texelIndex) {
        REQUIRE(readbackBytes[texelIndex * 4u + 0u] == std::byte{0x00}); // red
        REQUIRE(readbackBytes[texelIndex * 4u + 1u] == std::byte{0xFF}); // green
        REQUIRE(readbackBytes[texelIndex * 4u + 2u] == std::byte{0x00}); // blue
        REQUIRE(readbackBytes[texelIndex * 4u + 3u] == std::byte{0xFF}); // alpha
    }
}

// Depth-carrying records exercise the materialization branches the color-only E2E
// never reaches: VkPipelineRenderingCreateInfo depth (and stencil, for D24S8) formats,
// the depth-stencil state block and the compare-operation mapping.
TEST_CASE("Graphics pipeline materialization creates depth-carrying pipelines",
          "[graphicsPipeline][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    if (!harness->supportsDynamicRendering()) {
        SKIP("driver does not support dynamic rendering (core Vulkan 1.3)");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};
    const GraphicsExecutionFixture fixture{};
    const auto shaderTableView =
        CommandStreamShaderModuleTableValidator::validate(fixture.shaderTableBytes);
    REQUIRE(shaderTableView.has_value());

    SECTION("depth-tested D32Float pipeline") {
        std::vector<std::byte> tableBytes;
        appendValue(tableBytes, std::uint32_t{1});
        appendValue(tableBytes, std::uint32_t{0});
        appendValue(tableBytes, std::uint32_t{0}); // vertexShaderModuleSlot
        appendValue(tableBytes, std::uint32_t{1}); // fragmentShaderModuleSlot
        appendValue(tableBytes, std::uint32_t{0}); // pushConstantByteSize
        appendValue(tableBytes, std::uint32_t{4}); // TriangleList
        appendValue(tableBytes, std::uint32_t{1}); // colorAttachmentCount
        appendValue(tableBytes, std::uint32_t{5}); // D32Float depth attachment
        appendValue(tableBytes, std::uint32_t{1}); // depthTestEnable
        appendValue(tableBytes, std::uint32_t{1}); // depthWriteEnable
        appendValue(tableBytes, std::uint32_t{4}); // LessOrEqual
        appendValue(tableBytes, std::uint32_t{3}); // CullMode::Back
        appendValue(tableBytes, std::uint32_t{1}); // FrontFace::CounterClockwise
        appendValue(tableBytes, std::uint32_t{0}); // reservedFlags
        appendValue(tableBytes, std::uint32_t{1}); // R8G8B8A8Unorm
        for (std::uint32_t unusedIndex = 1; unusedIndex < 8u; ++unusedIndex) {
            appendValue(tableBytes, std::uint32_t{0});
        }
        const auto tableView =
            CommandStreamGraphicsPipelineTableValidator::validate(tableBytes);
        REQUIRE(tableView.has_value());
        const auto creationResult = VulkanGraphicsPipelineTable::createFromTables(
            vulkanContext, *tableView, *shaderTableView);
        REQUIRE(creationResult.has_value());
        REQUIRE(creationResult->slot(0u).pipeline != VK_NULL_HANDLE);
    }

    SECTION("D24UnormS8Uint pipeline chains the stencil attachment format") {
        const std::vector<std::byte> tableBytes =
            makeGraphicsPipelineTableBytes(1u, 1u, /*depthFormatValue=*/6u);
        const auto tableView =
            CommandStreamGraphicsPipelineTableValidator::validate(tableBytes);
        REQUIRE(tableView.has_value());
        const auto creationResult = VulkanGraphicsPipelineTable::createFromTables(
            vulkanContext, *tableView, *shaderTableView);
        REQUIRE(creationResult.has_value());
        REQUIRE(creationResult->slot(0u).pipeline != VK_NULL_HANDLE);
        REQUIRE(creationResult->pipelineRecord(0u).depthAttachmentFormatValue == 6u);
    }
}

TEST_CASE("Graphics pipeline materialization rejects a shader slot outside the table",
          "[graphicsPipeline][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    if (!harness->supportsDynamicRendering()) {
        SKIP("driver does not support dynamic rendering (core Vulkan 1.3)");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};
    const GraphicsExecutionFixture fixture{};

    // A one-blob shader table: the pipeline's fragment slot 1 falls outside it.
    const std::vector<std::byte> shortShaderTableBytes = [&] {
        std::vector<std::byte> tableBytes;
        appendValue(tableBytes, std::uint32_t{1});
        appendValue(tableBytes, std::uint32_t{0});
        appendValue(tableBytes, std::uint64_t{24});
        appendValue(tableBytes, static_cast<std::uint64_t>(fixture.vertexBlob.size()));
        tableBytes.insert(tableBytes.end(), fixture.vertexBlob.begin(),
                          fixture.vertexBlob.end());
        return tableBytes;
    }();
    const auto shaderTableView =
        CommandStreamShaderModuleTableValidator::validate(shortShaderTableBytes);
    const auto graphicsPipelineTableView =
        CommandStreamGraphicsPipelineTableValidator::validate(
            fixture.graphicsPipelineTableBytes);
    REQUIRE(shaderTableView.has_value());
    REQUIRE(graphicsPipelineTableView.has_value());

    const auto creationResult = VulkanGraphicsPipelineTable::createFromTables(
        vulkanContext, *graphicsPipelineTableView, *shaderTableView);
    REQUIRE_FALSE(creationResult.has_value());
    REQUIRE(creationResult.error().error
            == VulkanGraphicsPipelineTableCreationError::ShaderModuleSlotOutOfRange);
}

// Recording rejects graphics streams that break the §9.11 rules — the precise failure
// code that the Java side maps to an exception.
TEST_CASE("Graphics recording rejects invalid streams with the precise failure",
          "[graphicsPipeline][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    if (!harness->supportsDynamicRendering()) {
        SKIP("driver does not support dynamic rendering (core Vulkan 1.3)");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};
    const GraphicsExecutionFixture fixture{};

    const auto imageTableView =
        CommandStreamImageHandleTableValidator::validate(fixture.imageTableBytes);
    const auto imageViewTableView =
        CommandStreamImageViewHandleTableValidator::validate(fixture.imageViewTableBytes);
    const auto renderingTemplateTableView =
        CommandStreamRenderingTemplateTableValidator::validate(
            fixture.renderingTemplateTableBytes);
    const auto bufferTableView =
        CommandStreamBufferHandleTableValidator::validate(fixture.bufferTableBytes);
    const auto shaderTableView =
        CommandStreamShaderModuleTableValidator::validate(fixture.shaderTableBytes);
    const auto graphicsPipelineTableView =
        CommandStreamGraphicsPipelineTableValidator::validate(
            fixture.graphicsPipelineTableBytes);
    REQUIRE(imageTableView.has_value());
    REQUIRE(imageViewTableView.has_value());
    REQUIRE(renderingTemplateTableView.has_value());
    REQUIRE(bufferTableView.has_value());
    REQUIRE(shaderTableView.has_value());
    REQUIRE(graphicsPipelineTableView.has_value());

    auto imageTable = VulkanImageTable::createFromTable(vulkanContext, *imageTableView);
    REQUIRE(imageTable.has_value());
    const auto sharedImageTable =
        std::make_shared<const VulkanImageTable>(std::move(*imageTable));
    auto imageViewTable =
        VulkanImageViewTable::createFromTable(sharedImageTable, *imageViewTableView);
    auto bufferTable = VulkanBufferTable::createFromTable(vulkanContext, *bufferTableView);
    auto graphicsPipelineTable = VulkanGraphicsPipelineTable::createFromTables(
        vulkanContext, *graphicsPipelineTableView, *shaderTableView);
    REQUIRE(imageViewTable.has_value());
    REQUIRE(bufferTable.has_value());
    REQUIRE(graphicsPipelineTable.has_value());

    // Records the commands appendScopeCommands emits into a scope opened on template 0
    // and returns the recording result (tables always fully supplied).
    const auto recordInScope = [&](auto&& appendScopeCommands, bool openScope = true) {
        TestCommandStreamBuilder streamBuilder{0u};
        if (openScope) {
            appendBeginRendering(streamBuilder);
        }
        appendScopeCommands(streamBuilder);
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

    SECTION("graphics command outside a rendering scope") {
        const auto recordingResult = recordInScope(
            [](TestCommandStreamBuilder& streamBuilder) {
                appendBindGraphicsPipeline(streamBuilder, 0u);
            },
            /*openScope=*/false);
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::GraphicsCommandOutsideRenderingScope);
    }

    SECTION("graphics pipeline slot outside the table") {
        const auto recordingResult =
            recordInScope([](TestCommandStreamBuilder& streamBuilder) {
                appendBindGraphicsPipeline(streamBuilder, 9u);
            });
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::GraphicsPipelineSlotOutOfRange);
    }

    SECTION("draw without a bound graphics pipeline") {
        const auto recordingResult =
            recordInScope([](TestCommandStreamBuilder& streamBuilder) {
                appendSetViewport(streamBuilder);
                appendSetScissor(streamBuilder);
                appendDraw(streamBuilder);
            });
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::NoBoundGraphicsPipeline);
    }

    SECTION("draw before viewport and scissor are set") {
        const auto recordingResult =
            recordInScope([](TestCommandStreamBuilder& streamBuilder) {
                appendBindGraphicsPipeline(streamBuilder, 0u);
                appendDraw(streamBuilder);
            });
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::ViewportOrScissorNotSet);
    }

    // Materializes the given pipeline table and records BeginRendering(template 0) +
    // BindGraphicsPipeline(0) against it, returning the recording result. Exercises
    // one branch of the bind-time template-compatibility check per call.
    const auto recordBindAgainstScope = [&](std::vector<std::byte> pipelineTableBytes) {
        const auto pipelineTableViewResult =
            CommandStreamGraphicsPipelineTableValidator::validate(pipelineTableBytes);
        REQUIRE(pipelineTableViewResult.has_value());
        auto boundPipelineTable = VulkanGraphicsPipelineTable::createFromTables(
            vulkanContext, *pipelineTableViewResult, *shaderTableView);
        REQUIRE(boundPipelineTable.has_value());

        TestCommandStreamBuilder streamBuilder{0u};
        appendBeginRendering(streamBuilder);
        appendBindGraphicsPipeline(streamBuilder, 0u);
        const std::vector<std::byte> streamBytes = streamBuilder.build();
        const auto streamView = CommandStreamValidator::validate(streamBytes);
        REQUIRE(streamView.has_value());
        return CommandBufferRecorder::record(
            harness->allocateCommandBuffer(), *streamView,
            {.bufferTable = &*bufferTable,
             .imageTable = sharedImageTable.get(),
             .imageViewTable = &*imageViewTable,
             .renderingTemplateTableView = &*renderingTemplateTableView,
             .graphicsPipelineTable = &*boundPipelineTable});
    };

    SECTION("pipeline attachment format not matching the scope's template") {
        // Same pipeline shape but declaring B8G8R8A8Unorm against the R8G8B8A8Unorm view.
        const auto recordingResult =
            recordBindAgainstScope(makeGraphicsPipelineTableBytes(/*colorFormatValue=*/2u));
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::AttachmentFormatMismatch);
    }

    SECTION("pipeline color attachment count not matching the scope's template") {
        // Two declared color attachments against the template's single one.
        const auto recordingResult = recordBindAgainstScope(
            makeGraphicsPipelineTableBytes(1u, /*colorAttachmentCount=*/2u));
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::AttachmentFormatMismatch);
    }

    SECTION("pipeline depth attachment against a depth-less template") {
        // A declared D32Float depth attachment against the color-only template.
        const auto recordingResult = recordBindAgainstScope(
            makeGraphicsPipelineTableBytes(1u, 1u, /*depthFormatValue=*/5u));
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::AttachmentFormatMismatch);
    }

    SECTION("missing graphics pipeline table") {
        TestCommandStreamBuilder streamBuilder{0u};
        appendBeginRendering(streamBuilder);
        appendBindGraphicsPipeline(streamBuilder, 0u);
        const std::vector<std::byte> streamBytes = streamBuilder.build();
        const auto streamView = CommandStreamValidator::validate(streamBytes);
        REQUIRE(streamView.has_value());
        const auto recordingResult = CommandBufferRecorder::record(
            harness->allocateCommandBuffer(), *streamView,
            {.bufferTable = &*bufferTable,
             .imageTable = sharedImageTable.get(),
             .imageViewTable = &*imageViewTable,
             .renderingTemplateTableView = &*renderingTemplateTableView});
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::MissingGraphicsPipelineTable);
    }
}
