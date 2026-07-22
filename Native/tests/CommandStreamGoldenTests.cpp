#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamBarrierBatchTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamBufferBarrierRecord.hpp"
#include "BarriEww/CommandStream/CommandStreamBufferHandleTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamBufferMemoryKind.hpp"
#include "BarriEww/CommandStream/CommandStreamBufferUsage.hpp"
#include "BarriEww/CommandStream/CommandStreamImageViewHandleTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamModuleSectionType.hpp"
#include "BarriEww/CommandStream/CommandStreamModuleValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamOpcode.hpp"
#include "BarriEww/CommandStream/CommandStreamValidator.hpp"

using barrieww::CommandStreamBarrierBatchTableValidator;
using barrieww::CommandStreamBufferHandleTableValidator;
using barrieww::CommandStreamBufferMemoryKind;
using barrieww::CommandStreamBufferUsage;
using barrieww::CommandStreamImageViewHandleTableValidator;
using barrieww::CommandStreamModuleSectionType;
using barrieww::CommandStreamModuleValidator;
using barrieww::CommandStreamOpcode;
using barrieww::CommandStreamValidator;

namespace {

/** Reads a whole binary file from the shared repository fixture directory. */
std::vector<std::byte> readTestDataFile(const char* relativeFilePath) {
    const std::string absoluteFilePath =
        std::string(BARRIEWW_TEST_DATA_DIRECTORY) + "/" + relativeFilePath;
    std::ifstream fileStream(absoluteFilePath, std::ios::binary | std::ios::ate);
    REQUIRE(fileStream.is_open());
    const std::streamsize fileByteCount = fileStream.tellg();
    fileStream.seekg(0);
    std::vector<std::byte> fileBytes(static_cast<std::size_t>(fileByteCount));
    fileStream.read(reinterpret_cast<char*>(fileBytes.data()), fileByteCount);
    return fileBytes;
}

/** Reads one little-endian u32 out of a payload span. */
std::uint32_t readUnsignedInteger(std::span<const std::byte> payloadBytes,
                                  std::size_t byteOffset) {
    std::uint32_t value = 0;
    std::memcpy(&value, payloadBytes.data() + byteOffset, sizeof value);
    return value;
}

} // namespace

// Cross-language arbiter (ADR-0002): the same committed golden file must be produced
// byte-exactly by the Java CommandStreamWriter (Core test) and accepted + decoded
// correctly by this C++ validator. A disagreement here means the ABI drifted.
TEST_CASE("Committed golden lane stream validates and decodes as authored",
          "[commandStream][golden]") {
    const std::vector<std::byte> goldenBytes =
        readTestDataFile("CommandStream/TwoCommandLaneStream.becs");
    REQUIRE(goldenBytes.size() == 80u);

    const auto validationResult = CommandStreamValidator::validate(goldenBytes);
    REQUIRE(validationResult.has_value());
    REQUIRE(validationResult->laneIndex() == 0u);
    REQUIRE(validationResult->commandCount() == 2u);
    REQUIRE(validationResult->graphHash() == 0x0102030405060708ull);

    auto commandIterator = validationResult->begin();
    const auto drawRecord = *commandIterator;
    REQUIRE(drawRecord.opcode == CommandStreamOpcode::Draw);
    REQUIRE(drawRecord.payloadBytes.size() == 16u);
    REQUIRE(readUnsignedInteger(drawRecord.payloadBytes, 0u) == 3u);  // vertexCount
    REQUIRE(readUnsignedInteger(drawRecord.payloadBytes, 4u) == 1u);  // instanceCount
    REQUIRE(readUnsignedInteger(drawRecord.payloadBytes, 8u) == 0u);  // firstVertex
    REQUIRE(readUnsignedInteger(drawRecord.payloadBytes, 12u) == 0u); // firstInstance

    ++commandIterator;
    const auto dispatchRecord = *commandIterator;
    REQUIRE(dispatchRecord.opcode == CommandStreamOpcode::Dispatch);
    REQUIRE(readUnsignedInteger(dispatchRecord.payloadBytes, 0u) == 1u); // groupCountX
    REQUIRE(readUnsignedInteger(dispatchRecord.payloadBytes, 4u) == 2u); // groupCountY
    REQUIRE(readUnsignedInteger(dispatchRecord.payloadBytes, 8u) == 3u); // groupCountZ

    ++commandIterator;
    REQUIRE(commandIterator == validationResult->end());
}

// ImageViewHandleTable ABI arbiter: Java emits the committed table byte-exactly, and this
// Native test validates and decodes the same fixture. The source slots are deliberately
// not resolved here; that is VulkanImageViewTable's cross-table responsibility.
TEST_CASE("Committed ImageView table golden validates and decodes as authored",
          "[commandStream][golden][imageViewHandleTable]") {
    const std::vector<std::byte> goldenBytes =
        readTestDataFile("CommandStream/ImageViewHandleTable.becs");
    REQUIRE(goldenBytes.size() == 72u);

    const auto validationResult =
        CommandStreamImageViewHandleTableValidator::validate(goldenBytes);
    REQUIRE(validationResult.has_value());
    REQUIRE(validationResult->entryCount() == 2u);

    const auto firstEntry = validationResult->entry(0u);
    REQUIRE(firstEntry.imageSlot == 4u);
    REQUIRE(firstEntry.imageViewKindValue == 2u);
    REQUIRE(firstEntry.formatValue == 1u);
    REQUIRE(firstEntry.aspectMaskValue == 0x1u);
    REQUIRE(firstEntry.baseMipLevel == 1u);
    REQUIRE(firstEntry.mipLevelCount == 2u);
    REQUIRE(firstEntry.baseArrayLayer == 3u);
    REQUIRE(firstEntry.arrayLayerCount == 1u);

    const auto secondEntry = validationResult->entry(1u);
    REQUIRE(secondEntry.imageSlot == 8u);
    REQUIRE(secondEntry.imageViewKindValue == 3u);
    REQUIRE(secondEntry.formatValue == 5u);
    REQUIRE(secondEntry.aspectMaskValue == 0x2u);
    REQUIRE(secondEntry.baseMipLevel == 0u);
    REQUIRE(secondEntry.mipLevelCount == 1u);
    REQUIRE(secondEntry.baseArrayLayer == 0u);
    REQUIRE(secondEntry.arrayLayerCount == 1u);
}

// Module-level arbiter: the same committed golden module must be produced byte-exactly
// by the Java CommandStreamModuleWriter (Core test) and accepted + decoded correctly
// here. Layout: schema-valid BufferHandleTable (staging + device + imported) +
// lane 0 (Draw+Dispatch) + lane 1 (single Draw), shared graphHash.
TEST_CASE("Committed golden module validates and decodes as authored",
          "[commandStream][golden]") {
    const std::vector<std::byte> goldenBytes =
        readTestDataFile("CommandStream/BufferTableAndTwoLaneModule.becs");
    REQUIRE(goldenBytes.size() == 488u);

    const auto validationResult = CommandStreamModuleValidator::validate(goldenBytes);
    REQUIRE(validationResult.has_value());
    REQUIRE(validationResult->graphHash() == 0x0102030405060708ull);
    REQUIRE(validationResult->laneStreamCount() == 2u);
    REQUIRE(validationResult->sectionEntries().size() == 4u);

    const auto barrierTableSection =
        validationResult->findSection(CommandStreamModuleSectionType::BarrierBatchTable);
    REQUIRE(barrierTableSection.has_value());
    const auto barrierTableValidationResult =
        CommandStreamBarrierBatchTableValidator::validate(*barrierTableSection);
    REQUIRE(barrierTableValidationResult.has_value());
    REQUIRE(barrierTableValidationResult->batchCount() == 2u);
    const auto goldenGlobalBarrier = barrierTableValidationResult->globalBarrier(0u, 0u);
    REQUIRE(goldenGlobalBarrier.sourceAccessMask == 0x1000u);   // TRANSFER_WRITE
    REQUIRE(goldenGlobalBarrier.destinationAccessMask == 0x800u); // TRANSFER_READ
    const auto goldenBufferBarrier = barrierTableValidationResult->bufferBarrier(1u, 0u);
    REQUIRE(goldenBufferBarrier.bufferSlot == 1u);
    REQUIRE(goldenBufferBarrier.byteCount
            == barrieww::CommandStreamBufferBarrierRecord::s_wholeByteCount);

    const auto bufferTableSection =
        validationResult->findSection(CommandStreamModuleSectionType::BufferHandleTable);
    REQUIRE(bufferTableSection.has_value());
    const auto tableValidationResult =
        CommandStreamBufferHandleTableValidator::validate(*bufferTableSection);
    REQUIRE(tableValidationResult.has_value());
    REQUIRE(tableValidationResult->entryCount() == 3u);

    const auto stagingEntry = tableValidationResult->entry(0u);
    REQUIRE(stagingEntry.byteSize == 64u);
    REQUIRE(stagingEntry.usageFlags
            == static_cast<std::uint32_t>(CommandStreamBufferUsage::TransferSource));
    REQUIRE(stagingEntry.memoryKindValue
            == static_cast<std::uint32_t>(
                   CommandStreamBufferMemoryKind::HostVisiblePersistentMapped));
    const auto deviceEntry = tableValidationResult->entry(1u);
    REQUIRE(deviceEntry.byteSize == 64u);
    REQUIRE(deviceEntry.usageFlags
            == static_cast<std::uint32_t>(CommandStreamBufferUsage::TransferDestination));
    REQUIRE(deviceEntry.memoryKindValue
            == static_cast<std::uint32_t>(CommandStreamBufferMemoryKind::DeviceLocal));
    const auto importedEntry = tableValidationResult->entry(2u);
    REQUIRE(importedEntry.isImported());
    REQUIRE(importedEntry.importIdentifier == 1001u);

    const auto& laneZeroView = validationResult->laneStream(0u);
    REQUIRE(laneZeroView.commandCount() == 2u);
    auto laneZeroIterator = laneZeroView.begin();
    REQUIRE((*laneZeroIterator).opcode == CommandStreamOpcode::Draw);
    REQUIRE(readUnsignedInteger((*laneZeroIterator).payloadBytes, 0u) == 3u);
    ++laneZeroIterator;
    REQUIRE((*laneZeroIterator).opcode == CommandStreamOpcode::Dispatch);
    REQUIRE(readUnsignedInteger((*laneZeroIterator).payloadBytes, 8u) == 3u); // groupCountZ

    const auto& laneOneView = validationResult->laneStream(1u);
    REQUIRE(laneOneView.commandCount() == 1u);
    const auto laneOneDrawRecord = *laneOneView.begin();
    REQUIRE(laneOneDrawRecord.opcode == CommandStreamOpcode::Draw);
    REQUIRE(readUnsignedInteger(laneOneDrawRecord.payloadBytes, 0u) == 6u); // vertexCount
}
