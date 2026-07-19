#pragma once

#include <cstddef>
#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief One 24-byte BECS module directory entry: which section, its instance index
 *        (the lane index for LaneStream sections, 0 for singleton sections), and the
 *        byte range it occupies within the module. byteOffset is relative to the module
 *        base and must be 8-byte aligned (ADR-0002 D1). Layout is the wire format,
 *        pinned by the static_asserts below.
 */
struct CommandStreamModuleSectionEntry {
    std::uint16_t sectionTypeValue;
    std::uint16_t reservedFlags;
    std::uint32_t sectionIndex;
    std::uint64_t byteOffset;
    std::uint64_t byteSize;
};

static_assert(sizeof(CommandStreamModuleSectionEntry) == 24u,
              "BECS module directory entries must be exactly 24 bytes");
static_assert(offsetof(CommandStreamModuleSectionEntry, reservedFlags) == 2u);
static_assert(offsetof(CommandStreamModuleSectionEntry, sectionIndex) == 4u);
static_assert(offsetof(CommandStreamModuleSectionEntry, byteOffset) == 8u);
static_assert(offsetof(CommandStreamModuleSectionEntry, byteSize) == 16u);

} // namespace barrieww
