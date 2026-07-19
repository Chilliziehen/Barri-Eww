#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumeration; trivially thread-safe.
 * @brief Failure categories of BECS module load-time validation (ADR-0002 D5). Values
 *        are stable so the Java side can translate them into checked exceptions (§6.4).
 */
enum class CommandStreamModuleValidationError : std::uint32_t {
    /** The byte range is smaller than the 32-byte module header. */
    ModuleTooSmall = 1,
    /** The module base address is not 8-byte aligned (ADR-0002 D1). */
    MisalignedModuleBase = 2,
    /** header.totalByteSize does not equal the provided byte range size. */
    ModuleSizeMismatch = 3,
    /** The magic bytes are not "BECM". */
    InvalidModuleMagic = 4,
    /** versionMajor differs, or versionMinor is newer than this build supports (D6). */
    UnsupportedModuleVersion = 5,
    /** The directory (sectionCount entries) does not fit inside the module. */
    DirectoryOutOfBounds = 6,
    /** A directory entry's reserved flags are not zero. */
    NonZeroReservedFlags = 7,
    /** A directory entry names an unassigned section type. */
    UnknownSectionType = 8,
    /** A section's byte range leaves the module or intrudes into the directory. */
    SectionOutOfBounds = 9,
    /** A section's byteOffset is not 8-byte aligned. */
    MisalignedSectionOffset = 10,
    /** Two sections' byte ranges overlap. */
    SectionOverlap = 11,
    /** Two directory entries share the same (sectionType, sectionIndex) identity. */
    DuplicateSection = 12,
    /** header.laneStreamCount disagrees with the number of LaneStream sections. */
    LaneStreamCountMismatch = 13,
    /** Lane indices are not exactly 0..N-1, or a stream's laneIndex differs from its entry. */
    LaneIndexMismatch = 14,
    /** An embedded lane stream's graphHash differs from the module's. */
    GraphHashMismatch = 15,
    /** An embedded lane stream failed stream validation (nested failure attached). */
    LaneStreamInvalid = 16,
};

} // namespace barrieww
