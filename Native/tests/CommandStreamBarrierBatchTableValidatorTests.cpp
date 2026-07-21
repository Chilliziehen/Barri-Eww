#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamBarrierBatchTableValidationError.hpp"
#include "BarriEww/CommandStream/CommandStreamBarrierBatchTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamBufferBarrierRecord.hpp"
#include "BarriEww/CommandStream/CommandStreamImageBarrierRecord.hpp"

using barrieww::CommandStreamBarrierBatchTableValidationError;
using barrieww::CommandStreamBarrierBatchTableValidator;
using barrieww::CommandStreamBufferBarrierRecord;
using barrieww::CommandStreamImageBarrierRecord;

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

/**
 * A three-batch table exercising every record type: batch 0 = one global barrier
 * (@80), batch 1 = one buffer barrier (@112), batch 2 = one image barrier (@168,
 * Undefined -> TransferDestination on image slot 0). Directory ends at 8 + 3*24 = 80;
 * total 168 + 64 = 232.
 */
std::vector<std::byte> makeThreeBatchTable(std::uint32_t imageOldLayoutValue = 0u,
                                           std::uint32_t imageAspectMaskValue = 0x1u,
                                           std::uint32_t imageMipLevelCount = 1u) {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{3});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{1}); // batch 0: 1 global @80
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint64_t{80});
    appendValue(tableBytes, std::uint32_t{0}); // batch 1: 1 buffer @112
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint64_t{112});
    appendValue(tableBytes, std::uint32_t{0}); // batch 2: 1 image @168
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint64_t{168});
    // Global record @80.
    appendValue(tableBytes, transferStageMask);
    appendValue(tableBytes, transferWriteAccessMask);
    appendValue(tableBytes, transferStageMask);
    appendValue(tableBytes, transferReadAccessMask);
    // Buffer record @112.
    appendValue(tableBytes, transferStageMask);
    appendValue(tableBytes, transferWriteAccessMask);
    appendValue(tableBytes, transferStageMask);
    appendValue(tableBytes, transferReadAccessMask);
    appendValue(tableBytes, std::uint32_t{1});  // bufferSlot
    appendValue(tableBytes, std::uint32_t{0});  // reservedFlags
    appendValue(tableBytes, std::uint64_t{0});  // byteOffset
    appendValue(tableBytes, CommandStreamBufferBarrierRecord::s_wholeByteCount);
    // Image record @168.
    appendValue(tableBytes, std::uint64_t{0x1}); // TOP_OF_PIPE source stage
    appendValue(tableBytes, std::uint64_t{0});   // no source access
    appendValue(tableBytes, transferStageMask);
    appendValue(tableBytes, transferWriteAccessMask);
    appendValue(tableBytes, std::uint32_t{0});   // imageSlot
    appendValue(tableBytes, imageAspectMaskValue);
    appendValue(tableBytes, imageOldLayoutValue);
    appendValue(tableBytes, std::uint32_t{6});   // newLayout TransferDestination
    appendValue(tableBytes, std::uint32_t{0});   // baseMipLevel
    appendValue(tableBytes, imageMipLevelCount);
    appendValue(tableBytes, std::uint32_t{0});   // baseArrayLayer
    appendValue(tableBytes, CommandStreamImageBarrierRecord::s_remainingCount);
    return tableBytes;
}

} // namespace

TEST_CASE("Barrier batch table validator accepts and decodes a three-batch table",
          "[commandStream][barrierBatchTable]") {
    const std::vector<std::byte> tableBytes = makeThreeBatchTable();
    REQUIRE(tableBytes.size() == 232u);

    const auto validationResult =
        CommandStreamBarrierBatchTableValidator::validate(tableBytes);
    REQUIRE(validationResult.has_value());
    REQUIRE(validationResult->batchCount() == 3u);

    const auto globalBatchRecord = validationResult->batch(0u);
    REQUIRE(globalBatchRecord.globalBarrierCount == 1u);
    REQUIRE(globalBatchRecord.imageBarrierCount == 0u);
    const auto globalRecord = validationResult->globalBarrier(0u, 0u);
    REQUIRE(globalRecord.sourceAccessMask == transferWriteAccessMask);

    const auto bufferRecord = validationResult->bufferBarrier(1u, 0u);
    REQUIRE(bufferRecord.bufferSlot == 1u);
    REQUIRE(bufferRecord.byteCount == CommandStreamBufferBarrierRecord::s_wholeByteCount);

    const auto imageRecord = validationResult->imageBarrier(2u, 0u);
    REQUIRE(imageRecord.imageSlot == 0u);
    REQUIRE(imageRecord.oldLayoutValue == 0u);
    REQUIRE(imageRecord.newLayoutValue == 6u);
    REQUIRE(imageRecord.arrayLayerCount == CommandStreamImageBarrierRecord::s_remainingCount);
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
        std::vector<std::byte> tableBytes = makeThreeBatchTable();
        // Batch 0's barriersByteOffset lives at 8 + 16 = 24; make it odd.
        const std::uint64_t oddOffset = 81;
        std::memcpy(tableBytes.data() + 24u, &oddOffset, sizeof oddOffset);
        const auto validationResult =
            CommandStreamBarrierBatchTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamBarrierBatchTableValidationError::MisalignedBarrierRegion);
    }

    SECTION("barrier region leaving the table") {
        std::vector<std::byte> tableBytes = makeThreeBatchTable();
        const std::uint64_t overrunOffset = 224; // 224 + 32 > 232
        std::memcpy(tableBytes.data() + 24u, &overrunOffset, sizeof overrunOffset);
        const auto validationResult =
            CommandStreamBarrierBatchTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamBarrierBatchTableValidationError::BarrierRegionOutOfBounds);
    }

    SECTION("non-zero reserved flags in a directory record") {
        std::vector<std::byte> tableBytes = makeThreeBatchTable();
        const std::uint32_t nonZeroFlags = 7;
        // Batch 0's reservedFlags lives at 8 + 12 = 20.
        std::memcpy(tableBytes.data() + 20u, &nonZeroFlags, sizeof nonZeroFlags);
        const auto validationResult =
            CommandStreamBarrierBatchTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamBarrierBatchTableValidationError::NonZeroReservedFlags);
    }

    SECTION("zero source stage mask") {
        std::vector<std::byte> tableBytes = makeThreeBatchTable();
        const std::uint64_t zeroMask = 0;
        std::memcpy(tableBytes.data() + 80u, &zeroMask, sizeof zeroMask);
        const auto validationResult =
            CommandStreamBarrierBatchTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamBarrierBatchTableValidationError::ZeroStageMask);
        REQUIRE(validationResult.error().byteOffset == 80u);
    }

    SECTION("image barrier with an unassigned layout") {
        const std::vector<std::byte> tableBytes =
            makeThreeBatchTable(/*imageOldLayoutValue=*/99u);
        const auto validationResult =
            CommandStreamBarrierBatchTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamBarrierBatchTableValidationError::UnknownImageLayout);
    }

    SECTION("image barrier with an unusable aspect mask") {
        const std::vector<std::byte> tableBytes =
            makeThreeBatchTable(0u, /*imageAspectMaskValue=*/0u);
        const auto validationResult =
            CommandStreamBarrierBatchTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamBarrierBatchTableValidationError::UnusableImageAspectMask);
    }

    SECTION("image barrier with an empty subresource range") {
        const std::vector<std::byte> tableBytes =
            makeThreeBatchTable(0u, 0x1u, /*imageMipLevelCount=*/0u);
        const auto validationResult =
            CommandStreamBarrierBatchTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamBarrierBatchTableValidationError::EmptyImageSubresourceRange);
    }
}
