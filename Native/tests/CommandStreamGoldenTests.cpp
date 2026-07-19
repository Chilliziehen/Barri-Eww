#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamModuleSectionType.hpp"
#include "BarriEww/CommandStream/CommandStreamModuleValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamOpcode.hpp"
#include "BarriEww/CommandStream/CommandStreamValidator.hpp"

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

// Module-level arbiter: the same committed golden module must be produced byte-exactly
// by the Java CommandStreamModuleWriter (Core test) and accepted + decoded correctly
// here. Layout: BufferHandleTable (16 pattern bytes) + lane 0 (Draw+Dispatch) +
// lane 1 (single Draw), shared graphHash.
TEST_CASE("Committed golden module validates and decodes as authored",
          "[commandStream][golden]") {
    const std::vector<std::byte> goldenBytes =
        readTestDataFile("CommandStream/BufferTableAndTwoLaneModule.becs");
    REQUIRE(goldenBytes.size() == 256u);

    const auto validationResult = CommandStreamModuleValidator::validate(goldenBytes);
    REQUIRE(validationResult.has_value());
    REQUIRE(validationResult->graphHash() == 0x0102030405060708ull);
    REQUIRE(validationResult->laneStreamCount() == 2u);
    REQUIRE(validationResult->sectionEntries().size() == 3u);

    const auto bufferTableSection =
        validationResult->findSection(CommandStreamModuleSectionType::BufferHandleTable);
    REQUIRE(bufferTableSection.has_value());
    REQUIRE(bufferTableSection->size() == 16u);
    REQUIRE((*bufferTableSection)[0] == std::byte{0xA0});
    REQUIRE((*bufferTableSection)[15] == std::byte{0xAF});

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
