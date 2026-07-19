#pragma once

#include <cstddef>
#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief The 32-byte BECS lane-stream header (ADR-0002 D3). The layout below is the
 *        wire format itself: field order, sizes and natural alignment match the byte
 *        layout written by the Java side (little-endian, ADR-0002 D1), which the
 *        static_asserts at the bottom pin down at compile time (T0[1]).
 */
struct CommandStreamHeader {
    /** Expected magic bytes "BECS" identifying a lane stream. */
    static constexpr std::uint8_t s_expectedMagicBytes[4] = {0x42u, 0x45u, 0x43u, 0x53u};
    /** Current format major version; replayers accept exactly this major (ADR-0002 D6). */
    static constexpr std::uint16_t s_currentVersionMajor = 0u;
    /** Current format minor version; minors are additive, streams newer than this are rejected. */
    static constexpr std::uint16_t s_currentVersionMinor = 1u;
    /** Required alignment of every command and of the stream base address. */
    static constexpr std::size_t s_commandAlignment = 8u;

    std::uint8_t magicBytes[4];
    std::uint16_t versionMajor;
    std::uint16_t versionMinor;
    std::uint32_t laneIndex;
    std::uint32_t commandCount;
    std::uint64_t totalByteSize;
    std::uint64_t graphHash;
};

static_assert(sizeof(CommandStreamHeader) == 32u,
              "BECS lane-stream header must be exactly 32 bytes (ADR-0002 D3)");
static_assert(offsetof(CommandStreamHeader, versionMajor) == 4u);
static_assert(offsetof(CommandStreamHeader, versionMinor) == 6u);
static_assert(offsetof(CommandStreamHeader, laneIndex) == 8u);
static_assert(offsetof(CommandStreamHeader, commandCount) == 12u);
static_assert(offsetof(CommandStreamHeader, totalByteSize) == 16u);
static_assert(offsetof(CommandStreamHeader, graphHash) == 24u);

} // namespace barrieww
