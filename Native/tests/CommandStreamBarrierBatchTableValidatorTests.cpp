#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamBarrierBatchTableValidationError.hpp"
#include "BarriEww/CommandStream/CommandStreamBarrierBatchTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamBufferBarrierRecord.hpp"

using barrieww::CommandStreamBarrierBatchTableValidationError;
using barrieww::CommandStreamBarrierBatchTableValidator;
using barrieww::CommandStreamBufferBarrierRecord;

namespace {

constexpr std::uint64_t transferStageMask = 0x1000u;
constexpr std::uint64_t transferWriteAccessMask = 0x1000u;
constexpr std::uint64_t transferReadAccessMask = 0x800u;

/** Appends one little-endian value to a byte vector. */
template <typename ValueType>
void appendValue(std::vector<std::byte>& targetBytes, ValueType value) {
    const std::size_t writeOffset = targetBytes.size();
    targetBytes.resize(writeOffset + sizeof value);
    std::memcpy(targetBytes.data() + writeOffset, &value, sizeof value);
}

/** The golden-equivalent two-batch table: one global batch, one buffer batch. */
std::vector<std::byte> makeTwoBatchTable() {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{2});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{1}); // batch 0: 1 global, 0 buffer, @40
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint64_t{40});
    appendValue(tableBytes, std::uint32_t{0}); // batch 1: 0 global, 1 buffer, @72
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint64_t{72});
    appendValue(tableBytes, transferStageMask); // global record @40
    appendValue(tableBytes, transferWriteAccessMask);
    appendValue(tableBytes, transferStageMask);
    appendValue(tableBytes, transferReadAccessMask);
    appendValue(tableBytes, transferStageMask); // buffer record @72
    appendValue(tableBytes, transferWriteAccessMask);
    appendValue(tableBytes, transferStageMask);
    appendValue(tableBytes, transferReadAccessMask);
    appendValue(tableBytes, std::uint32_t{1});  // bufferSlot
    appendValue(tableBytes, std::uint32_t{0});  // reservedFlags
    appendValue(tableBytes, std::uint64_t{0});  // byteOffset
    appendValue(tableBytes, CommandStreamBufferBarrierRecord::s_wholeByteCount);
    return tableBytes;
}

} // namespace

TEST_CASE("Barrier batch table validator accepts and decodes a two-batch table",
          "[commandStream][barrierBatchTable]") {
    const std::vector<std::byte> tableBytes = makeTwoBatchTable();
    REQUIRE(tableBytes.size() == 128u);

    const auto validationResult =
        CommandStreamBarrierBatchTableValidator::validate(tableBytes);
    REQUIRE(validationResult.has_value());
    REQUIRE(validationResult->batchCount() == 2u);

    const auto globalBatchRecord = validationResult->batch(0u);
    REQUIRE(globalBatchRecord.globalBarrierCount == 1u);
    REQUIRE(globalBatchRecord.bufferBarrierCount == 0u);
    const auto globalRecord = validationResult->globalBarrier(0u, 0u);
    REQUIRE(globalRecord.sourceAccessMask == transferWriteAccessMask);
    REQUIRE(globalRecord.destinationAccessMask == transferReadAccessMask);

    const auto bufferRecord = validationResult->bufferBarrier(1u, 0u);
    REQUIRE(bufferRecord.bufferSlot == 1u);
    REQUIRE(bufferRecord.byteCount == CommandStreamBufferBarrierRecord::s_wholeByteCount);
}

TEST_CASE("Barrier batch table validator rejects malformed tables with the precise failure",
          "[commandStream][barrierBatchTable]") {
    SECTION("directory that does not fit") {
        std::vector<std::byte> tableBytes;
        appendValue(tableBytes, std::uint32_t{5});
        appendValue(tableBytes, std::uint32_t{0});
        const auto validationResult =
            CommandStreamBarrierBatchTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamBarrierBatchTableValidationError::DirectoryOutOfBounds);
    }

    SECTION("misaligned barrier region offset") {
        std::vector<std::byte> tableBytes = makeTwoBatchTable();
        // Batch 0's barriersByteOffset lives at 8 + 8 = 16; make it odd.
        const std::uint64_t oddOffset = 41;
        std::memcpy(tableBytes.data() + 16u, &oddOffset, sizeof oddOffset);
        const auto validationResult =
            CommandStreamBarrierBatchTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamBarrierBatchTableValidationError::MisalignedBarrierRegion);
    }

    SECTION("barrier region leaving the table") {
        std::vector<std::byte> tableBytes = makeTwoBatchTable();
        const std::uint64_t overrunOffset = 120; // 120 + 32 > 128
        std::memcpy(tableBytes.data() + 16u, &overrunOffset, sizeof overrunOffset);
        const auto validationResult =
            CommandStreamBarrierBatchTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamBarrierBatchTableValidationError::BarrierRegionOutOfBounds);
    }

    SECTION("barrier region intruding into the directory") {
        std::vector<std::byte> tableBytes = makeTwoBatchTable();
        const std::uint64_t directoryOffset = 8;
        std::memcpy(tableBytes.data() + 16u, &directoryOffset, sizeof directoryOffset);
        const auto validationResult =
            CommandStreamBarrierBatchTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamBarrierBatchTableValidationError::BarrierRegionOutOfBounds);
    }

    SECTION("zero source stage mask") {
        std::vector<std::byte> tableBytes = makeTwoBatchTable();
        const std::uint64_t zeroMask = 0;
        std::memcpy(tableBytes.data() + 40u, &zeroMask, sizeof zeroMask);
        const auto validationResult =
            CommandStreamBarrierBatchTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamBarrierBatchTableValidationError::ZeroStageMask);
        REQUIRE(validationResult.error().byteOffset == 40u);
    }

    SECTION("non-zero reserved flags in a buffer record") {
        std::vector<std::byte> tableBytes = makeTwoBatchTable();
        const std::uint32_t nonZeroFlags = 3;
        // Buffer record starts at 72; reservedFlags at +36.
        std::memcpy(tableBytes.data() + 72u + 36u, &nonZeroFlags, sizeof nonZeroFlags);
        const auto validationResult =
            CommandStreamBarrierBatchTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamBarrierBatchTableValidationError::NonZeroReservedFlags);
    }
}
