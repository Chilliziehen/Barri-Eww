#pragma once

#include <cstddef>
#include <cstdint>

#include "BarriEww/CommandStream/CommandStreamHeader.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief The 32-byte BECS module container header (ADR-0002 D2). A module is the bake
 *        artifact handed to the native side once per graph: this header, then
 *        sectionCount directory entries, then the section byte ranges. The version pair
 *        is shared with lane streams (one format family, single source of truth in
 *        CommandStreamHeader). Layout is the wire format itself, pinned by the
 *        static_asserts below (little-endian, ADR-0002 D1).
 */
struct CommandStreamModuleHeader {
    /** Expected magic bytes "BECM" identifying a module container. */
    static constexpr std::uint8_t s_expectedMagicBytes[4] = {0x42u, 0x45u, 0x43u, 0x4Du};
    /** Module version equals the lane-stream version (one format family). */
    static constexpr std::uint16_t s_currentVersionMajor = CommandStreamHeader::s_currentVersionMajor;
    /** Module minor version, additive like the lane-stream minor (ADR-0002 D6). */
    static constexpr std::uint16_t s_currentVersionMinor = CommandStreamHeader::s_currentVersionMinor;

    std::uint8_t magicBytes[4];
    std::uint16_t versionMajor;
    std::uint16_t versionMinor;
    std::uint32_t sectionCount;
    std::uint32_t laneStreamCount;
    std::uint64_t totalByteSize;
    std::uint64_t graphHash;
};

static_assert(sizeof(CommandStreamModuleHeader) == 32u,
              "BECS module header must be exactly 32 bytes (ADR-0002 D2)");
static_assert(offsetof(CommandStreamModuleHeader, versionMajor) == 4u);
static_assert(offsetof(CommandStreamModuleHeader, versionMinor) == 6u);
static_assert(offsetof(CommandStreamModuleHeader, sectionCount) == 8u);
static_assert(offsetof(CommandStreamModuleHeader, laneStreamCount) == 12u);
static_assert(offsetof(CommandStreamModuleHeader, totalByteSize) == 16u);
static_assert(offsetof(CommandStreamModuleHeader, graphHash) == 24u);

} // namespace barrieww
