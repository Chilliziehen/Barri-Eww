#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamBufferHandleTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamOpcode.hpp"
#include "BarriEww/CommandStream/CommandStreamValidator.hpp"
#include "BarriEww/Vulkan/CommandBufferRecorder.hpp"
#include "BarriEww/Vulkan/VulkanBufferTable.hpp"
#include "BarriEww/Vulkan/VulkanContext.hpp"
#include "BarriEww/Vulkan/VulkanFrameLoop.hpp"
#include "BarriEww/Vulkan/VulkanFrameLoopError.hpp"
#include "TestCommandStreamBuilder.hpp"
#include "TestVulkanDeviceHarness.hpp"

using barrieww::CommandBufferRecorder;
using barrieww::CommandStreamBufferHandleTableValidator;
using barrieww::CommandStreamOpcode;
using barrieww::CommandStreamValidator;
using barrieww::VulkanBufferTable;
using barrieww::VulkanContext;
using barrieww::VulkanFrameLoop;
using barrieww::VulkanFrameLoopError;
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

/** Per-slot transfer pairs: staging slots 0..1 (mapped source), readback slots 2..3. */
std::vector<std::byte> makeTwoSlotPairTableBytes() {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{4});
    appendValue(tableBytes, std::uint32_t{0});
    for (std::uint32_t stagingIndex = 0; stagingIndex < 2u; ++stagingIndex) {
        appendValue(tableBytes, std::uint64_t{64});
        appendValue(tableBytes, std::uint32_t{0x1}); // TransferSource
        appendValue(tableBytes, std::uint32_t{2});   // HostVisiblePersistentMapped
        appendValue(tableBytes, std::uint32_t{0});
        appendValue(tableBytes, std::uint32_t{0});
    }
    for (std::uint32_t readbackIndex = 0; readbackIndex < 2u; ++readbackIndex) {
        appendValue(tableBytes, std::uint64_t{64});
        appendValue(tableBytes, std::uint32_t{0x2}); // TransferDestination
        appendValue(tableBytes, std::uint32_t{3});   // HostVisibleReadback
        appendValue(tableBytes, std::uint32_t{0});
        appendValue(tableBytes, std::uint32_t{0});
    }
    return tableBytes;
}

/** CopyBuffer payload as pinned by CommandStreamWriter.appendCopyBuffer. */
std::vector<std::byte> makeCopyBufferPayload(std::uint32_t sourceBufferSlot,
                                             std::uint32_t destinationBufferSlot) {
    std::vector<std::byte> payloadBytes;
    appendValue(payloadBytes, sourceBufferSlot);
    appendValue(payloadBytes, destinationBufferSlot);
    appendValue(payloadBytes, std::uint64_t{0});
    appendValue(payloadBytes, std::uint64_t{0});
    appendValue(payloadBytes, std::uint64_t{64});
    return payloadBytes;
}

} // namespace

// The complete ADR-0003 per-frame execution shape, six frames long: two prerecorded
// command buffers reused three times each (record once, submit N times), per-frame
// parameters written through slot-owned persistent mappings only after beginFrame
// returned that slot (the fence contract), and one O(1) submit per frame. After the
// loop drains, each slot's readback holds the LAST frame that used it.
TEST_CASE("Frame loop paces six frames over two slots with prerecorded buffers",
          "[vulkanFrameLoop][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};

    const std::vector<std::byte> tableBytes = makeTwoSlotPairTableBytes();
    const auto tableValidationResult =
        CommandStreamBufferHandleTableValidator::validate(tableBytes);
    REQUIRE(tableValidationResult.has_value());
    auto bufferTableResult =
        VulkanBufferTable::createFromTable(vulkanContext, *tableValidationResult);
    REQUIRE(bufferTableResult.has_value());
    const VulkanBufferTable& bufferTable = *bufferTableResult;

    // Record once: slot k's buffer copies staging[k] -> readback[k + 2].
    std::array<VkCommandBuffer, 2> frameCommandBuffers{};
    for (std::uint32_t frameSlot = 0; frameSlot < 2u; ++frameSlot) {
        TestCommandStreamBuilder streamBuilder{frameSlot};
        streamBuilder.appendCommand(
            static_cast<std::uint16_t>(CommandStreamOpcode::CopyBuffer),
            makeCopyBufferPayload(frameSlot, frameSlot + 2u));
        const std::vector<std::byte> streamBytes = streamBuilder.build();
        const auto streamView = CommandStreamValidator::validate(streamBytes);
        REQUIRE(streamView.has_value());
        frameCommandBuffers[frameSlot] = harness->allocateCommandBuffer();
        REQUIRE(CommandBufferRecorder::record(frameCommandBuffers[frameSlot], *streamView,
            {.bufferTable = &bufferTable})
                    .has_value());
    }

    {
        auto frameLoopResult = VulkanFrameLoop::create(vulkanContext, 2u);
        REQUIRE(frameLoopResult.has_value());
        VulkanFrameLoop& frameLoop = *frameLoopResult;
        REQUIRE(frameLoop.framesInFlightCount() == 2u);

        for (std::uint32_t frameIndex = 0; frameIndex < 6u; ++frameIndex) {
            const auto beginResult = frameLoop.beginFrame();
            REQUIRE(beginResult.has_value());
            const std::uint32_t frameSlot = *beginResult;
            REQUIRE(frameSlot == frameIndex % 2u);

            // Per-frame parameter write, legal exactly now (slot fence has signalled).
            std::byte* stagingBytes = bufferTable.slot(frameSlot).mappedPointer;
            for (std::uint32_t byteIndex = 0; byteIndex < 64u; ++byteIndex) {
                stagingBytes[byteIndex] = static_cast<std::byte>(0x20u + frameIndex);
            }

            REQUIRE(frameLoop
                        .submitFrame(std::span<const VkCommandBuffer>{
                            &frameCommandBuffers[frameSlot], 1u})
                        .has_value());
        }
    } // Frame loop destructor drains the queue: all six frames have completed here.

    for (std::uint32_t frameSlot = 0; frameSlot < 2u; ++frameSlot) {
        const std::byte* readbackBytes = bufferTable.slot(frameSlot + 2u).mappedPointer;
        const auto expectedPatternByte =
            static_cast<std::byte>(0x20u + 4u + frameSlot); // last frame on this slot
        for (std::uint32_t byteIndex = 0; byteIndex < 64u; ++byteIndex) {
            REQUIRE(readbackBytes[byteIndex] == expectedPatternByte);
        }
    }
}

TEST_CASE("Frame loop rejects misuse with the precise failure", "[vulkanFrameLoop][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};

    SECTION("zero frames in flight") {
        const auto frameLoopResult = VulkanFrameLoop::create(vulkanContext, 0u);
        REQUIRE_FALSE(frameLoopResult.has_value());
        REQUIRE(frameLoopResult.error().error
                == VulkanFrameLoopError::InvalidFramesInFlightCount);
        REQUIRE(frameLoopResult.error().frameSlot == UINT32_MAX);
        REQUIRE(frameLoopResult.error().resultValue == 0);
    }

    SECTION("submit without an open frame") {
        auto frameLoopResult = VulkanFrameLoop::create(vulkanContext, 2u);
        REQUIRE(frameLoopResult.has_value());
        const auto submitResult = frameLoopResult->submitFrame({});
        REQUIRE_FALSE(submitResult.has_value());
        REQUIRE(submitResult.error().error == VulkanFrameLoopError::FrameNotOpen);
        REQUIRE(submitResult.error().frameSlot == 0u);
        REQUIRE(submitResult.error().resultValue == 0);
    }

    SECTION("double beginFrame") {
        auto frameLoopResult = VulkanFrameLoop::create(vulkanContext, 2u);
        REQUIRE(frameLoopResult.has_value());
        REQUIRE(frameLoopResult->beginFrame().has_value());
        const auto secondBeginResult = frameLoopResult->beginFrame();
        REQUIRE_FALSE(secondBeginResult.has_value());
        REQUIRE(secondBeginResult.error().error == VulkanFrameLoopError::FrameAlreadyOpen);
        REQUIRE(secondBeginResult.error().frameSlot == 0u);
        REQUIRE(secondBeginResult.error().resultValue == 0);
        // Close the frame so the destructor's drain has a signalled fence to meet.
        REQUIRE(frameLoopResult->submitFrame({}).has_value());
    }
}

TEST_CASE("Frame loop advances empty submissions and transfers fence ownership",
          "[vulkanFrameLoop][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};

    auto frameLoopResult = VulkanFrameLoop::create(vulkanContext, 3u);
    REQUIRE(frameLoopResult.has_value());
    VulkanFrameLoop movedFrameLoop{std::move(*frameLoopResult)};
    REQUIRE(frameLoopResult->framesInFlightCount() == 0u);
    REQUIRE(movedFrameLoop.framesInFlightCount() == 3u);

    constexpr std::array<std::uint32_t, 4> expectedFrameSlots{0u, 1u, 2u, 0u};
    for (std::uint32_t expectedFrameSlot : expectedFrameSlots) {
        const auto beginResult = movedFrameLoop.beginFrame();
        REQUIRE(beginResult.has_value());
        REQUIRE(*beginResult == expectedFrameSlot);
        REQUIRE(movedFrameLoop.submitFrame({}).has_value());
    }
}
