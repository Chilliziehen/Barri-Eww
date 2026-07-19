#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamHeader.hpp"

namespace barrieww::testing {

/**
 * @note ThreadSafety: Not thread-safe; each test builds its own instance.
 * @brief Test-only BECS lane-stream builder. In production streams are written
 *        exclusively by the Java side (ADR-0001); this builder exists so the C++
 *        validator and view can be exercised without a JVM (ADR-0002 testability).
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

    /** Finalizes the header and returns the stream bytes. */
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

} // namespace barrieww::testing
