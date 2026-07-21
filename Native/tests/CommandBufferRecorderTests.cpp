#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamBarrierBatchTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamBufferBarrierRecord.hpp"
#include "BarriEww/CommandStream/CommandStreamBufferHandleTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamOpcode.hpp"
#include "BarriEww/CommandStream/CommandStreamValidator.hpp"
#include "BarriEww/Vulkan/CommandBufferRecorder.hpp"
#include "BarriEww/Vulkan/CommandBufferRecordingError.hpp"
#include "BarriEww/Vulkan/VulkanBufferTable.hpp"
#include "BarriEww/Vulkan/VulkanContext.hpp"
#include "TestCommandStreamBuilder.hpp"
#include "TestVulkanDeviceHarness.hpp"

using barrieww::CommandBufferRecorder;
using barrieww::CommandBufferRecordingError;
using barrieww::CommandStreamBarrierBatchTableValidator;
using barrieww::CommandStreamBufferBarrierRecord;
using barrieww::CommandStreamBufferHandleTableValidator;
using barrieww::CommandStreamOpcode;
using barrieww::CommandStreamValidator;
using barrieww::VulkanBufferTable;
using barrieww::VulkanContext;
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

/** The pinned 32-byte CopyBuffer payload (mirrors CommandStreamWriter.appendCopyBuffer). */
std::vector<std::byte> makeCopyBufferPayload(std::uint32_t sourceBufferSlot,
                                             std::uint32_t destinationBufferSlot,
                                             std::uint64_t sourceByteOffset,
                                             std::uint64_t destinationByteOffset,
                                             std::uint64_t copyByteCount) {
    std::vector<std::byte> payloadBytes;
    appendValue(payloadBytes, sourceBufferSlot);
    appendValue(payloadBytes, destinationBufferSlot);
    appendValue(payloadBytes, sourceByteOffset);
    appendValue(payloadBytes, destinationByteOffset);
    appendValue(payloadBytes, copyByteCount);
    return payloadBytes;
}

/** Builds a schema-valid table: staging / device / readback, 64 bytes each. */
std::vector<std::byte> makeTransferChainTableBytes() {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{3});
    appendValue(tableBytes, std::uint32_t{0});
    // Slot 0: staging (TransferSource, HostVisiblePersistentMapped).
    appendValue(tableBytes, std::uint64_t{64});
    appendValue(tableBytes, std::uint32_t{0x1});
    appendValue(tableBytes, std::uint32_t{2});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0});
    // Slot 1: device-local bounce (TransferDestination | TransferSource).
    appendValue(tableBytes, std::uint64_t{64});
    appendValue(tableBytes, std::uint32_t{0x3});
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0});
    // Slot 2: readback (TransferDestination, HostVisibleReadback).
    appendValue(tableBytes, std::uint64_t{64});
    appendValue(tableBytes, std::uint32_t{0x2});
    appendValue(tableBytes, std::uint32_t{3});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0});
    return tableBytes;
}

/** One single-CopyBuffer lane stream. */
std::vector<std::byte> makeSingleCopyStream(std::uint32_t laneIndex,
                                            std::uint32_t sourceBufferSlot,
                                            std::uint32_t destinationBufferSlot,
                                            std::uint64_t copyByteCount) {
    TestCommandStreamBuilder streamBuilder{laneIndex};
    streamBuilder.appendCommand(
        static_cast<std::uint16_t>(CommandStreamOpcode::CopyBuffer),
        makeCopyBufferPayload(sourceBufferSlot, destinationBufferSlot, 0u, 0u,
                              copyByteCount));
    return streamBuilder.build();
}

/** The pinned 8-byte ExecuteBarrierBatch payload (slot + zero padding). */
std::vector<std::byte> makeExecuteBarrierBatchPayload(std::uint32_t barrierBatchSlot) {
    std::vector<std::byte> payloadBytes;
    appendValue(payloadBytes, barrierBatchSlot);
    appendValue(payloadBytes, std::uint32_t{0});
    return payloadBytes;
}

/**
 * A one-batch table with a single whole-size buffer barrier on bufferSlot, using the
 * given sync2 masks (transfer write -> transfer read by default).
 */
std::vector<std::byte> makeSingleBufferBarrierTable(std::uint32_t bufferSlot,
                                                    std::uint64_t stageMask = 0x1000u) {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0}); // 0 global barriers
    appendValue(tableBytes, std::uint32_t{1}); // 1 buffer barrier
    appendValue(tableBytes, std::uint32_t{0}); // 0 image barriers
    appendValue(tableBytes, std::uint32_t{0}); // reservedFlags
    appendValue(tableBytes, std::uint64_t{32}); // records right after the directory
    appendValue(tableBytes, stageMask);                    // sourceStageMask
    appendValue(tableBytes, std::uint64_t{0x1000u});       // TRANSFER_WRITE
    appendValue(tableBytes, stageMask);                    // destinationStageMask
    appendValue(tableBytes, std::uint64_t{0x800u});        // TRANSFER_READ
    appendValue(tableBytes, bufferSlot);
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint64_t{0});
    appendValue(tableBytes, CommandStreamBufferBarrierRecord::s_wholeByteCount);
    return tableBytes;
}

} // namespace

// First real GPU execution of the toolchain: BufferHandleTable -> VulkanBufferTable
// materialization -> two prerecorded command buffers (staging->device, device->readback,
// hazard handled by the submission boundary) -> submit -> byte-exact readback. This is
// the ADR-0003 spine: everything decided before execution; per-frame equivalent cost
// would be submission only.
TEST_CASE("Recorded CopyBuffer chain executes on the GPU and round-trips bytes",
          "[commandBufferRecorder][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};

    const std::vector<std::byte> tableBytes = makeTransferChainTableBytes();
    const auto tableValidationResult =
        CommandStreamBufferHandleTableValidator::validate(tableBytes);
    REQUIRE(tableValidationResult.has_value());

    auto bufferTableResult =
        VulkanBufferTable::createFromTable(vulkanContext, *tableValidationResult);
    REQUIRE(bufferTableResult.has_value());
    const VulkanBufferTable& bufferTable = *bufferTableResult;

    // Author 64 pattern bytes through the persistent mapping of the staging slot.
    REQUIRE(bufferTable.slot(0u).mappedPointer != nullptr);
    for (std::uint32_t byteIndex = 0; byteIndex < 64u; ++byteIndex) {
        bufferTable.slot(0u).mappedPointer[byteIndex] =
            static_cast<std::byte>(0x5A ^ byteIndex);
    }

    // Two lanes; the hazard between the copies is ordered by the submission boundary.
    const std::vector<std::byte> firstStreamBytes = makeSingleCopyStream(0u, 0u, 1u, 64u);
    const std::vector<std::byte> secondStreamBytes = makeSingleCopyStream(1u, 1u, 2u, 64u);
    const auto firstStreamView = CommandStreamValidator::validate(firstStreamBytes);
    const auto secondStreamView = CommandStreamValidator::validate(secondStreamBytes);
    REQUIRE(firstStreamView.has_value());
    REQUIRE(secondStreamView.has_value());

    const VkCommandBuffer firstCommandBuffer = harness->allocateCommandBuffer();
    const VkCommandBuffer secondCommandBuffer = harness->allocateCommandBuffer();
    REQUIRE(CommandBufferRecorder::record(firstCommandBuffer, *firstStreamView,
            {.bufferTable = &bufferTable})
                .has_value());
    REQUIRE(CommandBufferRecorder::record(secondCommandBuffer, *secondStreamView,
            {.bufferTable = &bufferTable})
                .has_value());

    REQUIRE(harness->submitAndWait(firstCommandBuffer));
    REQUIRE(harness->submitAndWait(secondCommandBuffer));

    REQUIRE(bufferTable.slot(2u).mappedPointer != nullptr);
    for (std::uint32_t byteIndex = 0; byteIndex < 64u; ++byteIndex) {
        REQUIRE(bufferTable.slot(2u).mappedPointer[byteIndex]
                == static_cast<std::byte>(0x5A ^ byteIndex));
    }
}

// Single-lane, single-submit variant of the transfer chain: the hazard between the two
// copies is ordered by an ExecuteBarrierBatch (transfer write -> transfer read buffer
// barrier from the BarrierBatchTable) instead of a submission boundary — the compile-
// time barrier pass's output executing inside one prerecorded command buffer.
TEST_CASE("Recorded copy-barrier-copy chain executes in one submission",
          "[commandBufferRecorder][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};

    const std::vector<std::byte> tableBytes = makeTransferChainTableBytes();
    const auto tableValidationResult =
        CommandStreamBufferHandleTableValidator::validate(tableBytes);
    REQUIRE(tableValidationResult.has_value());
    auto bufferTableResult =
        VulkanBufferTable::createFromTable(vulkanContext, *tableValidationResult);
    REQUIRE(bufferTableResult.has_value());
    const VulkanBufferTable& bufferTable = *bufferTableResult;

    const std::vector<std::byte> barrierTableBytes = makeSingleBufferBarrierTable(1u);
    const auto barrierTableView =
        CommandStreamBarrierBatchTableValidator::validate(barrierTableBytes);
    REQUIRE(barrierTableView.has_value());

    for (std::uint32_t byteIndex = 0; byteIndex < 64u; ++byteIndex) {
        bufferTable.slot(0u).mappedPointer[byteIndex] =
            static_cast<std::byte>(0xC3 ^ byteIndex);
    }

    TestCommandStreamBuilder streamBuilder{0u};
    streamBuilder.appendCommand(static_cast<std::uint16_t>(CommandStreamOpcode::CopyBuffer),
                                makeCopyBufferPayload(0u, 1u, 0u, 0u, 64u));
    streamBuilder.appendCommand(
        static_cast<std::uint16_t>(CommandStreamOpcode::ExecuteBarrierBatch),
        makeExecuteBarrierBatchPayload(0u));
    streamBuilder.appendCommand(static_cast<std::uint16_t>(CommandStreamOpcode::CopyBuffer),
                                makeCopyBufferPayload(1u, 2u, 0u, 0u, 64u));
    const std::vector<std::byte> streamBytes = streamBuilder.build();
    const auto streamView = CommandStreamValidator::validate(streamBytes);
    REQUIRE(streamView.has_value());

    const VkCommandBuffer commandBuffer = harness->allocateCommandBuffer();
    REQUIRE(CommandBufferRecorder::record(commandBuffer, *streamView,
            {.bufferTable = &bufferTable, .barrierBatchTableView = &barrierTableView.value()})
                .has_value());
    REQUIRE(harness->submitAndWait(commandBuffer));

    for (std::uint32_t byteIndex = 0; byteIndex < 64u; ++byteIndex) {
        REQUIRE(bufferTable.slot(2u).mappedPointer[byteIndex]
                == static_cast<std::byte>(0xC3 ^ byteIndex));
    }
}

TEST_CASE("Recorder rejects invalid barrier batches with the precise failure",
          "[commandBufferRecorder][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};
    const std::vector<std::byte> tableBytes = makeTransferChainTableBytes();
    const auto tableValidationResult =
        CommandStreamBufferHandleTableValidator::validate(tableBytes);
    REQUIRE(tableValidationResult.has_value());
    auto bufferTableResult =
        VulkanBufferTable::createFromTable(vulkanContext, *tableValidationResult);
    REQUIRE(bufferTableResult.has_value());

    TestCommandStreamBuilder streamBuilder{0u};
    streamBuilder.appendCommand(
        static_cast<std::uint16_t>(CommandStreamOpcode::ExecuteBarrierBatch),
        makeExecuteBarrierBatchPayload(0u));
    const std::vector<std::byte> streamBytes = streamBuilder.build();
    const auto streamView = CommandStreamValidator::validate(streamBytes);
    REQUIRE(streamView.has_value());

    SECTION("stream with barriers but no barrier table provided") {
        const auto recordingResult = CommandBufferRecorder::record(harness->allocateCommandBuffer(), *streamView,
            {.bufferTable = &*bufferTableResult});
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::MissingBarrierBatchTable);
    }

    SECTION("barrier batch slot outside the table") {
        const std::vector<std::byte> barrierTableBytes = makeSingleBufferBarrierTable(1u);
        const auto barrierTableView =
            CommandStreamBarrierBatchTableValidator::validate(barrierTableBytes);
        REQUIRE(barrierTableView.has_value());
        TestCommandStreamBuilder outOfRangeStreamBuilder{0u};
        outOfRangeStreamBuilder.appendCommand(
            static_cast<std::uint16_t>(CommandStreamOpcode::ExecuteBarrierBatch),
            makeExecuteBarrierBatchPayload(5u));
        const std::vector<std::byte> outOfRangeStreamBytes = outOfRangeStreamBuilder.build();
        const auto outOfRangeStreamView =
            CommandStreamValidator::validate(outOfRangeStreamBytes);
        REQUIRE(outOfRangeStreamView.has_value());
        const auto recordingResult = CommandBufferRecorder::record(harness->allocateCommandBuffer(), *outOfRangeStreamView,
            {.bufferTable = &*bufferTableResult, .barrierBatchTableView = &barrierTableView.value()});
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::BarrierBatchSlotOutOfRange);
    }

    SECTION("sync2-only stage bits are rejected by the legacy-mapping recorder") {
        // VK_PIPELINE_STAGE_2_COPY_BIT (0x100000000) has no sync1 equivalent.
        const std::vector<std::byte> barrierTableBytes =
            makeSingleBufferBarrierTable(1u, 0x100000000ull);
        const auto barrierTableView =
            CommandStreamBarrierBatchTableValidator::validate(barrierTableBytes);
        REQUIRE(barrierTableView.has_value());
        const auto recordingResult = CommandBufferRecorder::record(harness->allocateCommandBuffer(), *streamView,
            {.bufferTable = &*bufferTableResult, .barrierBatchTableView = &barrierTableView.value()});
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::UnmappableSynchronizationScope);
    }
}

TEST_CASE("Recorder rejects invalid streams with the precise failure",
          "[commandBufferRecorder][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};

    const std::vector<std::byte> tableBytes = makeTransferChainTableBytes();
    const auto tableValidationResult =
        CommandStreamBufferHandleTableValidator::validate(tableBytes);
    REQUIRE(tableValidationResult.has_value());
    auto bufferTableResult =
        VulkanBufferTable::createFromTable(vulkanContext, *tableValidationResult);
    REQUIRE(bufferTableResult.has_value());

    SECTION("buffer slot outside the table") {
        const std::vector<std::byte> streamBytes = makeSingleCopyStream(0u, 0u, 9u, 64u);
        const auto streamView = CommandStreamValidator::validate(streamBytes);
        REQUIRE(streamView.has_value());
        const auto recordingResult = CommandBufferRecorder::record(harness->allocateCommandBuffer(), *streamView,
            {.bufferTable = &*bufferTableResult});
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::BufferSlotOutOfRange);
        REQUIRE(recordingResult.error().commandIndex == 0u);
    }

    SECTION("copy range leaving the buffer") {
        const std::vector<std::byte> streamBytes = makeSingleCopyStream(0u, 0u, 1u, 128u);
        const auto streamView = CommandStreamValidator::validate(streamBytes);
        REQUIRE(streamView.has_value());
        const auto recordingResult = CommandBufferRecorder::record(harness->allocateCommandBuffer(), *streamView,
            {.bufferTable = &*bufferTableResult});
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::CopyRangeOutOfBounds);
    }

    SECTION("assigned but unimplemented opcode fails loudly") {
        TestCommandStreamBuilder streamBuilder{0u};
        streamBuilder.appendCommand(static_cast<std::uint16_t>(CommandStreamOpcode::Draw),
                                    std::vector<std::byte>(16u, std::byte{0}));
        const std::vector<std::byte> streamBytes = streamBuilder.build();
        const auto streamView = CommandStreamValidator::validate(streamBytes);
        REQUIRE(streamView.has_value());
        const auto recordingResult = CommandBufferRecorder::record(harness->allocateCommandBuffer(), *streamView,
            {.bufferTable = &*bufferTableResult});
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::UnsupportedOpcode);
    }

    SECTION("unbound imported slot") {
        std::vector<std::byte> importedTableBytes;
        appendValue(importedTableBytes, std::uint32_t{2});
        appendValue(importedTableBytes, std::uint32_t{0});
        appendValue(importedTableBytes, std::uint64_t{64}); // slot 0: created staging
        appendValue(importedTableBytes, std::uint32_t{0x1});
        appendValue(importedTableBytes, std::uint32_t{2});
        appendValue(importedTableBytes, std::uint32_t{0});
        appendValue(importedTableBytes, std::uint32_t{0});
        appendValue(importedTableBytes, std::uint64_t{0}); // slot 1: imported, unbound
        appendValue(importedTableBytes, std::uint32_t{0});
        appendValue(importedTableBytes, std::uint32_t{0});
        appendValue(importedTableBytes, std::uint32_t{4242});
        appendValue(importedTableBytes, std::uint32_t{0});
        const auto importedTableView =
            CommandStreamBufferHandleTableValidator::validate(importedTableBytes);
        REQUIRE(importedTableView.has_value());
        auto importedBufferTable =
            VulkanBufferTable::createFromTable(vulkanContext, *importedTableView);
        REQUIRE(importedBufferTable.has_value());
        REQUIRE_FALSE(importedBufferTable->slot(1u).isBound);

        const std::vector<std::byte> streamBytes = makeSingleCopyStream(0u, 0u, 1u, 64u);
        const auto streamView = CommandStreamValidator::validate(streamBytes);
        REQUIRE(streamView.has_value());
        const auto recordingResult = CommandBufferRecorder::record(harness->allocateCommandBuffer(), *streamView,
            {.bufferTable = &*importedBufferTable});
        REQUIRE_FALSE(recordingResult.has_value());
        REQUIRE(recordingResult.error().error
                == CommandBufferRecordingError::UnboundImportedBuffer);
    }
}
