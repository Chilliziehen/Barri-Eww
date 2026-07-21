#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamBarrierBatchTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamBufferHandleTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamImageBarrierRecord.hpp"
#include "BarriEww/CommandStream/CommandStreamImageHandleTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamOpcode.hpp"
#include "BarriEww/CommandStream/CommandStreamValidator.hpp"
#include "BarriEww/Vulkan/CommandBufferRecorder.hpp"
#include "BarriEww/Vulkan/CommandBufferRecordingError.hpp"
#include "BarriEww/Vulkan/VulkanBufferTable.hpp"
#include "BarriEww/Vulkan/VulkanContext.hpp"
#include "BarriEww/Vulkan/VulkanImageTable.hpp"
#include "TestCommandStreamBuilder.hpp"
#include "TestVulkanDeviceHarness.hpp"

using barrieww::CommandBufferRecorder;
using barrieww::CommandBufferRecordingError;
using barrieww::CommandStreamBarrierBatchTableValidator;
using barrieww::CommandStreamBufferHandleTableValidator;
using barrieww::CommandStreamImageBarrierRecord;
using barrieww::CommandStreamImageHandleTableValidator;
using barrieww::CommandStreamOpcode;
using barrieww::CommandStreamValidator;
using barrieww::VulkanBufferTable;
using barrieww::VulkanContext;
using barrieww::VulkanImageTable;
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

/** One created 8x8 R8G8B8A8Unorm image (TransferSource | TransferDestination). */
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
    appendValue(tableBytes, std::uint32_t{0x3});
    appendValue(tableBytes, std::uint32_t{0});
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

/** Appends one 64-byte image barrier record to a byte vector. */
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
    appendValue(tableBytes, std::uint32_t{0}); // imageSlot
    appendValue(tableBytes, std::uint32_t{0x1}); // Color aspect
    appendValue(tableBytes, oldLayoutValue);
    appendValue(tableBytes, newLayoutValue);
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, CommandStreamImageBarrierRecord::s_remainingCount);
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, CommandStreamImageBarrierRecord::s_remainingCount);
}

/**
 * Two image-barrier batches: batch 0 = Undefined -> TransferDestination
 * (TOP_OF_PIPE -> TRANSFER/WRITE), batch 1 = TransferDestination -> TransferSource
 * (TRANSFER/WRITE -> TRANSFER/READ). Directory ends at 8 + 2*24 = 56.
 */
std::vector<std::byte> makeLayoutTransitionBarrierTableBytes() {
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
    appendImageBarrierRecord(tableBytes, 0x1u, 0u, 0x1000u, 0x1000u, 0u, 6u);
    appendImageBarrierRecord(tableBytes, 0x1000u, 0x1000u, 0x1000u, 0x800u, 6u, 5u);
    return tableBytes;
}

/** Pinned 48-byte ClearColorImage payload (full color subresource). */
std::vector<std::byte> makeClearColorImagePayload(std::uint32_t imageSlot,
                                                  std::uint32_t imageLayoutValue,
                                                  std::uint32_t baseMipLevel = 0u) {
    std::vector<std::byte> payloadBytes;
    appendValue(payloadBytes, imageSlot);
    appendValue(payloadBytes, imageLayoutValue);
    appendValue(payloadBytes, 1.0f);
    appendValue(payloadBytes, 0.0f);
    appendValue(payloadBytes, 1.0f);
    appendValue(payloadBytes, 1.0f);
    appendValue(payloadBytes, std::uint32_t{0x1}); // Color aspect
    appendValue(payloadBytes, baseMipLevel);
    appendValue(payloadBytes, CommandStreamImageBarrierRecord::s_remainingCount);
    appendValue(payloadBytes, std::uint32_t{0});
    appendValue(payloadBytes, CommandStreamImageBarrierRecord::s_remainingCount);
    appendValue(payloadBytes, std::uint32_t{0});
    return payloadBytes;
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

} // namespace

// First image execution of the toolchain, prerecorded as ONE command buffer: transition
// Undefined -> TransferDestination, clear to magenta {1,0,1,1}, transition to
// TransferSource, copy the full mip into the readback buffer — then assert all 64
// texels byte-exactly ({255, 0, 255, 255} in R8G8B8A8). The compile-time barrier pass's
// output (the batch table) is what makes the image legally usable at all.
TEST_CASE("Recorded clear and copy chain round-trips image texels",
          "[imageExecution][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};

    const std::vector<std::byte> imageTableBytes = makeImageTableBytes();
    const std::vector<std::byte> bufferTableBytes = makeReadbackBufferTableBytes();
    const std::vector<std::byte> barrierTableBytes = makeLayoutTransitionBarrierTableBytes();
    const auto imageTableView =
        CommandStreamImageHandleTableValidator::validate(imageTableBytes);
    const auto bufferTableView =
        CommandStreamBufferHandleTableValidator::validate(bufferTableBytes);
    const auto barrierTableView =
        CommandStreamBarrierBatchTableValidator::validate(barrierTableBytes);
    REQUIRE(imageTableView.has_value());
    REQUIRE(bufferTableView.has_value());
    REQUIRE(barrierTableView.has_value());

    auto imageTable = VulkanImageTable::createFromTable(vulkanContext, *imageTableView);
    REQUIRE(imageTable.has_value());
    REQUIRE(imageTable->slot(0u).isBound);
    auto bufferTable = VulkanBufferTable::createFromTable(vulkanContext, *bufferTableView);
    REQUIRE(bufferTable.has_value());

    TestCommandStreamBuilder streamBuilder{0u};
    {
        std::vector<std::byte> payloadBytes;
        appendValue(payloadBytes, std::uint32_t{0});
        appendValue(payloadBytes, std::uint32_t{0});
        streamBuilder.appendCommand(
            static_cast<std::uint16_t>(CommandStreamOpcode::ExecuteBarrierBatch),
            payloadBytes);
    }
    streamBuilder.appendCommand(
        static_cast<std::uint16_t>(CommandStreamOpcode::ClearColorImage),
        makeClearColorImagePayload(0u, /*TransferDestination*/ 6u));
    {
        std::vector<std::byte> payloadBytes;
        appendValue(payloadBytes, std::uint32_t{1});
        appendValue(payloadBytes, std::uint32_t{0});
        streamBuilder.appendCommand(
            static_cast<std::uint16_t>(CommandStreamOpcode::ExecuteBarrierBatch),
            payloadBytes);
    }
    streamBuilder.appendCommand(
        static_cast<std::uint16_t>(CommandStreamOpcode::CopyImageToBuffer),
        makeCopyImageToBufferPayload(0u, 0u));
    const std::vector<std::byte> streamBytes = streamBuilder.build();
    const auto streamView = CommandStreamValidator::validate(streamBytes);
    REQUIRE(streamView.has_value());

    const VkCommandBuffer commandBuffer = harness->allocateCommandBuffer();
    REQUIRE(CommandBufferRecorder::record(commandBuffer, *streamView,
            {.bufferTable = &*bufferTable, .barrierBatchTableView = &*barrierTableView, .imageTable = &*imageTable})
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

TEST_CASE("Image command recording rejects invalid streams with the precise failure",
          "[imageExecution][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};

    const std::vector<std::byte> imageTableBytes = makeImageTableBytes();
    const std::vector<std::byte> bufferTableBytes = makeReadbackBufferTableBytes();
    const auto imageTableView =
        CommandStreamImageHandleTableValidator::validate(imageTableBytes);
    const auto bufferTableView =
        CommandStreamBufferHandleTableValidator::validate(bufferTableBytes);
    REQUIRE(imageTableView.has_value());
    REQUIRE(bufferTableView.has_value());
    auto imageTable = VulkanImageTable::createFromTable(vulkanContext, *imageTableView);
    auto bufferTable = VulkanBufferTable::createFromTable(vulkanContext, *bufferTableView);
    REQUIRE(imageTable.has_value());
    REQUIRE(bufferTable.has_value());

    const auto recordSingleClear = [&](std::vector<std::byte> payloadBytes,
                                       const VulkanImageTable* imageTablePointer) {
        TestCommandStreamBuilder streamBuilder{0u};
        streamBuilder.appendCommand(
            static_cast<std::uint16_t>(CommandStreamOpcode::ClearColorImage),
            payloadBytes);
        const std::vector<std::byte> streamBytes = streamBuilder.build();
        const auto streamView = CommandStreamValidator::validate(streamBytes);
        REQUIRE(streamView.has_value());
        return CommandBufferRecorder::record(harness->allocateCommandBuffer(), *streamView,
            {.bufferTable = &*bufferTable, .imageTable = imageTablePointer});
    };

    SECTION("missing image table") {
        const auto recordingResult =
            recordSingleClear(makeClearColorImagePayload(0u, 6u), nullptr);
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::MissingImageTable);
    }

    SECTION("image slot outside the table") {
        const auto recordingResult =
            recordSingleClear(makeClearColorImagePayload(9u, 6u), &*imageTable);
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::ImageSlotOutOfRange);
    }

    SECTION("unassigned layout value") {
        const auto recordingResult =
            recordSingleClear(makeClearColorImagePayload(0u, 99u), &*imageTable);
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::UnknownImageLayoutValue);
    }

    SECTION("subresource range outside the image") {
        const auto recordingResult = recordSingleClear(
            makeClearColorImagePayload(0u, 6u, /*baseMipLevel=*/5u), &*imageTable);
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::ImageSubresourceOutOfRange);
    }
}
