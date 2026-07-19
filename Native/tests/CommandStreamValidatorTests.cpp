#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamHeader.hpp"
#include "BarriEww/CommandStream/CommandStreamOpcode.hpp"
#include "BarriEww/CommandStream/CommandStreamValidationError.hpp"
#include "BarriEww/CommandStream/CommandStreamValidator.hpp"

using barrieww::CommandStreamHeader;
using barrieww::CommandStreamOpcode;
using barrieww::CommandStreamValidationError;
using barrieww::CommandStreamValidator;

namespace {

/**
 * @note ThreadSafety: Not thread-safe; each test builds its own instance.
 * @brief Test-only BECS stream builder. In production streams are written exclusively
 *        by the Java side (ADR-0001); this builder exists so the C++ validator and view
 *        can be exercised without a JVM (the testability payoff of ADR-0002).
 */
class TestCommandStreamBuilder {
public:
    explicit TestCommandStreamBuilder(std::uint32_t laneIndex = 0u,
                                      std::uint64_t graphHash = 0xABCDEF0123456789ull) {
        m_streamBytes.resize(sizeof(CommandStreamHeader), std::byte{0});
        m_laneIndex = laneIndex;
        m_graphHash = graphHash;
    }

    /** Appends one command; payloadBytes must already be padded to the 8-byte rule. */
    void appendCommand(std::uint16_t rawOpcodeValue,
                       const std::vector<std::byte>& payloadBytes,
                       std::uint16_t reservedFlags = 0u) {
        const std::uint32_t commandByteSize =
            static_cast<std::uint32_t>(8u + payloadBytes.size());
        appendValue(rawOpcodeValue);
        appendValue(reservedFlags);
        appendValue(commandByteSize);
        m_streamBytes.insert(m_streamBytes.end(), payloadBytes.begin(), payloadBytes.end());
        ++m_commandCount;
    }

    /** Finalizes the header and returns the aligned stream bytes. */
    [[nodiscard]] std::vector<std::byte> build(bool overrideCommandCount = false,
                                               std::uint32_t forcedCommandCount = 0u,
                                               bool overrideTotalByteSize = false,
                                               std::uint64_t forcedTotalByteSize = 0u) {
        CommandStreamHeader header{};
        std::memcpy(header.magicBytes, CommandStreamHeader::s_expectedMagicBytes, 4u);
        header.versionMajor = CommandStreamHeader::s_currentVersionMajor;
        header.versionMinor = CommandStreamHeader::s_currentVersionMinor;
        header.laneIndex = m_laneIndex;
        header.commandCount = overrideCommandCount ? forcedCommandCount : m_commandCount;
        header.totalByteSize =
            overrideTotalByteSize ? forcedTotalByteSize : m_streamBytes.size();
        header.graphHash = m_graphHash;
        std::memcpy(m_streamBytes.data(), &header, sizeof header);
        return m_streamBytes;
    }

    /** Direct mutation hook for corruption tests (byte offset into the stream). */
    void corruptByte(std::size_t byteOffset, std::uint8_t newValue) {
        m_streamBytes.at(byteOffset) = static_cast<std::byte>(newValue);
    }

private:
    template <typename ValueType>
    void appendValue(ValueType value) {
        const std::size_t writeOffset = m_streamBytes.size();
        m_streamBytes.resize(writeOffset + sizeof value);
        std::memcpy(m_streamBytes.data() + writeOffset, &value, sizeof value);
    }

    std::vector<std::byte> m_streamBytes;
    std::uint32_t m_commandCount = 0;
    std::uint32_t m_laneIndex = 0;
    std::uint64_t m_graphHash = 0;
};

/** Payload of the given byte count filled with an incrementing pattern. */
std::vector<std::byte> makePatternPayload(std::size_t byteCount) {
    std::vector<std::byte> payloadBytes(byteCount);
    for (std::size_t byteIndex = 0; byteIndex < byteCount; ++byteIndex) {
        payloadBytes[byteIndex] = static_cast<std::byte>(byteIndex & 0xFFu);
    }
    return payloadBytes;
}

std::uint16_t rawValue(CommandStreamOpcode opcode) {
    return static_cast<std::uint16_t>(opcode);
}

} // namespace

TEST_CASE("Validator accepts an empty stream and exposes header fields",
          "[commandStream]") {
    TestCommandStreamBuilder builder{7u, 0x1122334455667788ull};
    const std::vector<std::byte> streamBytes = builder.build();

    const auto validationResult = CommandStreamValidator::validate(streamBytes);
    REQUIRE(validationResult.has_value());
    REQUIRE(validationResult->laneIndex() == 7u);
    REQUIRE(validationResult->commandCount() == 0u);
    REQUIRE(validationResult->graphHash() == 0x1122334455667788ull);
    REQUIRE(validationResult->begin() == validationResult->end());
}

TEST_CASE("Validator accepts a two-command stream and the view decodes it",
          "[commandStream]") {
    TestCommandStreamBuilder builder;
    // Draw: 16-byte payload (4×u32); Dispatch: 16-byte payload (3×u32 + 4 padding).
    builder.appendCommand(rawValue(CommandStreamOpcode::Draw), makePatternPayload(16u));
    builder.appendCommand(rawValue(CommandStreamOpcode::Dispatch), makePatternPayload(16u));
    const std::vector<std::byte> streamBytes = builder.build();

    const auto validationResult = CommandStreamValidator::validate(streamBytes);
    REQUIRE(validationResult.has_value());
    REQUIRE(validationResult->commandCount() == 2u);

    auto commandIterator = validationResult->begin();
    REQUIRE((*commandIterator).opcode == CommandStreamOpcode::Draw);
    REQUIRE((*commandIterator).payloadBytes.size() == 16u);
    REQUIRE((*commandIterator).payloadBytes[1] == std::byte{1});
    ++commandIterator;
    REQUIRE((*commandIterator).opcode == CommandStreamOpcode::Dispatch);
    REQUIRE((*commandIterator).payloadBytes.size() == 16u);
    ++commandIterator;
    REQUIRE(commandIterator == validationResult->end());
}

TEST_CASE("Validator rejects malformed streams with the precise failure",
          "[commandStream]") {
    SECTION("stream smaller than the header") {
        const std::vector<std::byte> tinyBytes(16u, std::byte{0});
        const auto validationResult = CommandStreamValidator::validate(tinyBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error == CommandStreamValidationError::StreamTooSmall);
    }

    SECTION("corrupted magic") {
        TestCommandStreamBuilder builder;
        auto streamBytes = builder.build();
        streamBytes[0] = std::byte{0x00};
        const auto validationResult = CommandStreamValidator::validate(streamBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error == CommandStreamValidationError::InvalidMagic);
    }

    SECTION("future minor version is rejected") {
        TestCommandStreamBuilder builder;
        auto streamBytes = builder.build();
        // versionMinor lives at offset 6 (CommandStreamHeader static_asserts).
        streamBytes[6] = std::byte{0x63};
        const auto validationResult = CommandStreamValidator::validate(streamBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamValidationError::UnsupportedVersion);
    }

    SECTION("totalByteSize disagreeing with the byte range") {
        TestCommandStreamBuilder builder;
        const auto streamBytes =
            builder.build(false, 0u, /*overrideTotalByteSize=*/true, /*forced=*/64u);
        const auto validationResult = CommandStreamValidator::validate(streamBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamValidationError::StreamSizeMismatch);
    }

    SECTION("misaligned command size") {
        TestCommandStreamBuilder builder;
        builder.appendCommand(rawValue(CommandStreamOpcode::Draw), makePatternPayload(16u));
        auto streamBytes = builder.build();
        // Command byteSize lives at header(32) + 4; force 20 (not a multiple of 8)...
        streamBytes[36] = std::byte{20u};
        // ...and grow the forced size mismatch away by rebuilding total: simpler to
        // just expect MisalignedCommandSize before any chain check fires.
        const auto validationResult = CommandStreamValidator::validate(streamBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamValidationError::MisalignedCommandSize);
        REQUIRE(validationResult.error().byteOffset == 32u);
    }

    SECTION("non-zero reserved flags") {
        TestCommandStreamBuilder builder;
        builder.appendCommand(rawValue(CommandStreamOpcode::Draw), makePatternPayload(16u),
                              /*reservedFlags=*/1u);
        const auto streamBytes = builder.build();
        const auto validationResult = CommandStreamValidator::validate(streamBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamValidationError::NonZeroReservedFlags);
    }

    SECTION("opcode from a reserved backend range (DX12) is rejected") {
        TestCommandStreamBuilder builder;
        builder.appendCommand(/*rawOpcodeValue=*/0x2000u, makePatternPayload(8u));
        const auto streamBytes = builder.build();
        const auto validationResult = CommandStreamValidator::validate(streamBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error == CommandStreamValidationError::UnknownOpcode);
        REQUIRE(validationResult.error().byteOffset == 32u);
    }

    SECTION("commandCount larger than the actual chain") {
        TestCommandStreamBuilder builder;
        builder.appendCommand(rawValue(CommandStreamOpcode::EndRendering), {});
        const auto streamBytes = builder.build(/*overrideCommandCount=*/true, /*forced=*/2u);
        const auto validationResult = CommandStreamValidator::validate(streamBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error == CommandStreamValidationError::TruncatedCommand);
    }

    SECTION("commandCount smaller than the actual chain") {
        TestCommandStreamBuilder builder;
        builder.appendCommand(rawValue(CommandStreamOpcode::EndRendering), {});
        builder.appendCommand(rawValue(CommandStreamOpcode::EndRendering), {});
        const auto streamBytes = builder.build(/*overrideCommandCount=*/true, /*forced=*/1u);
        const auto validationResult = CommandStreamValidator::validate(streamBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamValidationError::CommandChainMismatch);
    }

    SECTION("command running past the end of the stream") {
        TestCommandStreamBuilder builder;
        builder.appendCommand(rawValue(CommandStreamOpcode::Draw), makePatternPayload(16u));
        auto streamBytes = builder.build();
        // Inflate the command's byteSize to 64 (aligned, but past totalByteSize).
        streamBytes[36] = std::byte{64u};
        const auto validationResult = CommandStreamValidator::validate(streamBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error == CommandStreamValidationError::TruncatedCommand);
    }
}
