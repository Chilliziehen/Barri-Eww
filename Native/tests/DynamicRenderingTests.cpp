#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamBarrierBatchTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamBufferHandleTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamImageBarrierRecord.hpp"
#include "BarriEww/CommandStream/CommandStreamImageHandleTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamImageViewHandleTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamOpcode.hpp"
#include "BarriEww/CommandStream/CommandStreamRenderingTemplateTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamValidator.hpp"
#include "BarriEww/Vulkan/CommandBufferRecorder.hpp"
#include "BarriEww/Vulkan/CommandBufferRecordingError.hpp"
#include "BarriEww/Vulkan/VulkanBufferTable.hpp"
#include "BarriEww/Vulkan/VulkanContext.hpp"
#include "BarriEww/Vulkan/VulkanImageTable.hpp"
#include "BarriEww/Vulkan/VulkanImageViewTable.hpp"
#include "TestCommandStreamBuilder.hpp"
#include "TestVulkanDeviceHarness.hpp"

using barrieww::CommandBufferRecorder;
using barrieww::CommandBufferRecordingError;
using barrieww::CommandStreamBarrierBatchTableValidator;
using barrieww::CommandStreamBufferHandleTableValidator;
using barrieww::CommandStreamImageBarrierRecord;
using barrieww::CommandStreamImageHandleTableValidator;
using barrieww::CommandStreamImageViewHandleTableValidator;
using barrieww::CommandStreamOpcode;
using barrieww::CommandStreamRenderingTemplateTableValidator;
using barrieww::CommandStreamValidator;
using barrieww::VulkanBufferTable;
using barrieww::VulkanContext;
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
    appendValue(tableBytes, std::uint32_t{0}); // imageSlot
    appendValue(tableBytes, std::uint32_t{2}); // TwoDimensional
    appendValue(tableBytes, std::uint32_t{1}); // R8G8B8A8Unorm
    appendValue(tableBytes, std::uint32_t{0x1}); // Color aspect
    appendValue(tableBytes, std::uint32_t{0}); // baseMipLevel
    appendValue(tableBytes, std::uint32_t{1}); // mipLevelCount
    appendValue(tableBytes, std::uint32_t{0}); // baseArrayLayer
    appendValue(tableBytes, std::uint32_t{1}); // arrayLayerCount
    return tableBytes;
}

/**
 * One rendering template: a single color attachment on image-view slot 0, layout
 * ColorAttachment, loadOp Clear to magenta {1,0,1,1}, storeOp Store, over an 8x8 area.
 * Directory ends at 8 + 40 = 48; one 32-byte attachment record at 48; total 80.
 */
std::vector<std::byte> makeRenderingTemplateTableBytes(std::uint32_t imageViewSlot = 0u) {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{1}); // templateCount
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{1}); // colorAttachmentCount
    appendValue(tableBytes, std::uint32_t{0}); // depthAttachmentPresent
    appendValue(tableBytes, std::int32_t{0});  // renderAreaOffsetX
    appendValue(tableBytes, std::int32_t{0});  // renderAreaOffsetY
    appendValue(tableBytes, std::uint32_t{8}); // renderAreaWidth
    appendValue(tableBytes, std::uint32_t{8}); // renderAreaHeight
    appendValue(tableBytes, std::uint32_t{1}); // layerCount
    appendValue(tableBytes, std::uint32_t{0}); // viewMask
    appendValue(tableBytes, std::uint64_t{48}); // attachmentsByteOffset
    appendValue(tableBytes, imageViewSlot);
    appendValue(tableBytes, std::uint32_t{2}); // ColorAttachment layout
    appendValue(tableBytes, std::uint32_t{1}); // Clear
    appendValue(tableBytes, std::uint32_t{0}); // Store
    appendValue(tableBytes, 1.0f); // clear R
    appendValue(tableBytes, 0.0f); // clear G
    appendValue(tableBytes, 1.0f); // clear B
    appendValue(tableBytes, 1.0f); // clear A
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
    appendValue(tableBytes, std::uint32_t{0});   // imageSlot
    appendValue(tableBytes, std::uint32_t{0x1}); // Color aspect
    appendValue(tableBytes, oldLayoutValue);
    appendValue(tableBytes, newLayoutValue);
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, CommandStreamImageBarrierRecord::s_remainingCount);
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, CommandStreamImageBarrierRecord::s_remainingCount);
}

/**
 * Two image-barrier batches: batch 0 = Undefined -> ColorAttachment (TOP_OF_PIPE ->
 * COLOR_ATTACHMENT_OUTPUT/WRITE), batch 1 = ColorAttachment -> TransferSource
 * (COLOR_ATTACHMENT_OUTPUT/WRITE -> TRANSFER/READ). Directory ends at 8 + 2*24 = 56.
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
    // Undefined(0) -> ColorAttachment(2): TOP_OF_PIPE -> COLOR_ATTACHMENT_OUTPUT/WRITE.
    appendImageBarrierRecord(tableBytes, 0x1u, 0u, 0x400u, 0x100u, 0u, 2u);
    // ColorAttachment(2) -> TransferSource(5): COLOR_ATTACHMENT_OUTPUT/WRITE -> TRANSFER/READ.
    appendImageBarrierRecord(tableBytes, 0x400u, 0x100u, 0x1000u, 0x800u, 2u, 5u);
    return tableBytes;
}

/** Pinned 56-byte CopyImageToBuffer payload (mip 0, layer 0, full 8x8 extent). */
std::vector<std::byte> makeCopyImageToBufferPayload(std::uint32_t imageSlot,
                                                    std::uint32_t bufferSlot) {
    std::vector<std::byte> payloadBytes;
    appendValue(payloadBytes, imageSlot);
    appendValue(payloadBytes, bufferSlot);
    appendValue(payloadBytes, std::uint32_t{5});   // TransferSource layout
    appendValue(payloadBytes, std::uint32_t{0x1}); // Color aspect
    appendValue(payloadBytes, std::uint32_t{0});   // mipLevel
    appendValue(payloadBytes, std::uint32_t{0});   // baseArrayLayer
    appendValue(payloadBytes, std::uint32_t{1});   // arrayLayerCount
    appendValue(payloadBytes, std::uint32_t{0});
    appendValue(payloadBytes, std::uint64_t{0});   // bufferByteOffset
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

} // namespace

// The toolchain's first dynamic-rendering execution, prerecorded as ONE command buffer
// and WITHOUT a graphics pipeline: transition Undefined -> ColorAttachment, open a
// rendering scope whose single color attachment clears (loadOp) the view to magenta,
// close it, transition to TransferSource, copy the mip into the readback buffer — then
// assert all 64 texels are byte-exactly {255, 0, 255, 255}. loadOp=Clear is what lets a
// rendering scope produce observable pixels with no pipeline (ADR-0003 microkernel E2E).
TEST_CASE("Recorded dynamic-rendering clear round-trips image texels",
          "[dynamicRendering][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    if (!harness->supportsDynamicRendering()) {
        SKIP("driver does not support dynamic rendering (core Vulkan 1.3)");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};

    const std::vector<std::byte> imageTableBytes = makeImageTableBytes();
    const std::vector<std::byte> imageViewTableBytes = makeImageViewTableBytes();
    const std::vector<std::byte> renderingTemplateTableBytes =
        makeRenderingTemplateTableBytes();
    const std::vector<std::byte> bufferTableBytes = makeReadbackBufferTableBytes();
    const std::vector<std::byte> barrierTableBytes = makeRenderTransitionBarrierTableBytes();

    const auto imageTableView =
        CommandStreamImageHandleTableValidator::validate(imageTableBytes);
    const auto imageViewTableView =
        CommandStreamImageViewHandleTableValidator::validate(imageViewTableBytes);
    const auto renderingTemplateTableView =
        CommandStreamRenderingTemplateTableValidator::validate(renderingTemplateTableBytes);
    const auto bufferTableView =
        CommandStreamBufferHandleTableValidator::validate(bufferTableBytes);
    const auto barrierTableView =
        CommandStreamBarrierBatchTableValidator::validate(barrierTableBytes);
    REQUIRE(imageTableView.has_value());
    REQUIRE(imageViewTableView.has_value());
    REQUIRE(renderingTemplateTableView.has_value());
    REQUIRE(bufferTableView.has_value());
    REQUIRE(barrierTableView.has_value());

    auto imageTable = VulkanImageTable::createFromTable(vulkanContext, *imageTableView);
    REQUIRE(imageTable.has_value());
    REQUIRE(imageTable->slot(0u).isBound);
    const auto sharedImageTable =
        std::make_shared<const VulkanImageTable>(std::move(*imageTable));
    auto imageViewTable =
        VulkanImageViewTable::createFromTable(sharedImageTable, *imageViewTableView);
    REQUIRE(imageViewTable.has_value());
    auto bufferTable = VulkanBufferTable::createFromTable(vulkanContext, *bufferTableView);
    REQUIRE(bufferTable.has_value());

    TestCommandStreamBuilder streamBuilder{0u};
    appendExecuteBarrierBatch(streamBuilder, 0u); // Undefined -> ColorAttachment
    {
        std::vector<std::byte> payloadBytes;
        appendValue(payloadBytes, std::uint32_t{0}); // renderingTemplateSlot
        appendValue(payloadBytes, std::uint32_t{0});
        streamBuilder.appendCommand(
            static_cast<std::uint16_t>(CommandStreamOpcode::BeginRendering), payloadBytes);
    }
    streamBuilder.appendCommand(
        static_cast<std::uint16_t>(CommandStreamOpcode::EndRendering), {});
    appendExecuteBarrierBatch(streamBuilder, 1u); // ColorAttachment -> TransferSource
    streamBuilder.appendCommand(
        static_cast<std::uint16_t>(CommandStreamOpcode::CopyImageToBuffer),
        makeCopyImageToBufferPayload(0u, 0u));
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
                 .renderingTemplateTableView = &*renderingTemplateTableView})
                .has_value());
    REQUIRE(harness->submitAndWait(commandBuffer));

    const std::byte* readbackBytes = bufferTable->slot(0u).mappedPointer;
    REQUIRE(readbackBytes != nullptr);
    for (std::uint32_t texelIndex = 0; texelIndex < 64u; ++texelIndex) {
        REQUIRE(readbackBytes[texelIndex * 4u + 0u] == std::byte{0xFF}); // red
        REQUIRE(readbackBytes[texelIndex * 4u + 1u] == std::byte{0x00}); // green
        REQUIRE(readbackBytes[texelIndex * 4u + 2u] == std::byte{0xFF}); // blue
        REQUIRE(readbackBytes[texelIndex * 4u + 3u] == std::byte{0xFF}); // alpha
    }
}

// Recording rejects a dynamic-rendering stream whose tables are missing or whose slots
// fall out of range — the precise failure code that the Java side maps to an exception.
TEST_CASE("Dynamic-rendering recording rejects invalid streams with the precise failure",
          "[dynamicRendering][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};

    const std::vector<std::byte> imageTableBytes = makeImageTableBytes();
    const std::vector<std::byte> imageViewTableBytes = makeImageViewTableBytes();
    const std::vector<std::byte> renderingTemplateTableBytes =
        makeRenderingTemplateTableBytes();
    const std::vector<std::byte> bufferTableBytes = makeReadbackBufferTableBytes();

    const auto imageViewTableView =
        CommandStreamImageViewHandleTableValidator::validate(imageViewTableBytes);
    const auto renderingTemplateTableView =
        CommandStreamRenderingTemplateTableValidator::validate(renderingTemplateTableBytes);
    const auto imageTableView =
        CommandStreamImageHandleTableValidator::validate(imageTableBytes);
    const auto bufferTableView =
        CommandStreamBufferHandleTableValidator::validate(bufferTableBytes);
    REQUIRE(imageViewTableView.has_value());
    REQUIRE(renderingTemplateTableView.has_value());
    REQUIRE(imageTableView.has_value());
    REQUIRE(bufferTableView.has_value());

    auto imageTable = VulkanImageTable::createFromTable(vulkanContext, *imageTableView);
    REQUIRE(imageTable.has_value());
    const auto sharedImageTable =
        std::make_shared<const VulkanImageTable>(std::move(*imageTable));
    auto imageViewTable =
        VulkanImageViewTable::createFromTable(sharedImageTable, *imageViewTableView);
    auto bufferTable = VulkanBufferTable::createFromTable(vulkanContext, *bufferTableView);
    REQUIRE(imageViewTable.has_value());
    REQUIRE(bufferTable.has_value());

    const auto recordBeginRendering =
        [&](std::uint32_t renderingTemplateSlot,
            const barrieww::CommandStreamRenderingTemplateTableView* templateTablePointer,
            const VulkanImageViewTable* imageViewTablePointer) {
            TestCommandStreamBuilder streamBuilder{0u};
            std::vector<std::byte> payloadBytes;
            appendValue(payloadBytes, renderingTemplateSlot);
            appendValue(payloadBytes, std::uint32_t{0});
            streamBuilder.appendCommand(
                static_cast<std::uint16_t>(CommandStreamOpcode::BeginRendering), payloadBytes);
            const std::vector<std::byte> streamBytes = streamBuilder.build();
            const auto streamView = CommandStreamValidator::validate(streamBytes);
            REQUIRE(streamView.has_value());
            return CommandBufferRecorder::record(
                harness->allocateCommandBuffer(), *streamView,
                {.bufferTable = &*bufferTable,
                 .imageTable = sharedImageTable.get(),
                 .imageViewTable = imageViewTablePointer,
                 .renderingTemplateTableView = templateTablePointer});
        };

    SECTION("missing rendering template table") {
        const auto recordingResult =
            recordBeginRendering(0u, nullptr, &*imageViewTable);
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::MissingRenderingTemplateTable);
    }

    SECTION("rendering template slot outside the table") {
        const auto recordingResult =
            recordBeginRendering(9u, &*renderingTemplateTableView, &*imageViewTable);
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::RenderingTemplateSlotOutOfRange);
    }

    SECTION("missing image view table") {
        const auto recordingResult =
            recordBeginRendering(0u, &*renderingTemplateTableView, nullptr);
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::MissingImageViewTable);
    }

    SECTION("nested rendering scope") {
        TestCommandStreamBuilder streamBuilder{0u};
        for (std::uint32_t beginIndex = 0; beginIndex < 2u; ++beginIndex) {
            std::vector<std::byte> payloadBytes;
            appendValue(payloadBytes, std::uint32_t{0});
            appendValue(payloadBytes, std::uint32_t{0});
            streamBuilder.appendCommand(
                static_cast<std::uint16_t>(CommandStreamOpcode::BeginRendering),
                payloadBytes);
        }
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
                == CommandBufferRecordingError::NestedRenderingScope);
        REQUIRE(recordingResult.error().commandIndex == 1u);
    }

    SECTION("attachment referencing an image-view slot outside the table") {
        const std::vector<std::byte> outOfRangeTemplateTableBytes =
            makeRenderingTemplateTableBytes(/*imageViewSlot=*/9u);
        const auto outOfRangeTemplateTableView =
            CommandStreamRenderingTemplateTableValidator::validate(
                outOfRangeTemplateTableBytes);
        REQUIRE(outOfRangeTemplateTableView.has_value());
        const auto recordingResult =
            recordBeginRendering(0u, &*outOfRangeTemplateTableView, &*imageViewTable);
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::ImageViewSlotOutOfRange);
    }

    SECTION("end rendering without an open scope") {
        TestCommandStreamBuilder streamBuilder{0u};
        streamBuilder.appendCommand(
            static_cast<std::uint16_t>(CommandStreamOpcode::EndRendering), {});
        const std::vector<std::byte> streamBytes = streamBuilder.build();
        const auto streamView = CommandStreamValidator::validate(streamBytes);
        REQUIRE(streamView.has_value());
        const auto recordingResult = CommandBufferRecorder::record(
            harness->allocateCommandBuffer(), *streamView, {.bufferTable = &*bufferTable});
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::RenderingScopeNotOpen);
    }
}
