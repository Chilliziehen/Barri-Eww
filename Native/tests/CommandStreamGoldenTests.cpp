#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamOpcode.hpp"
#include "BarriEww/CommandStream/CommandStreamValidator.hpp"

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
