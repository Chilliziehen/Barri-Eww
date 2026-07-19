#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumeration; trivially thread-safe.
 * @brief Failure categories of BECS load-time validation (ADR-0002 D5). Values are
 *        stable so the Java side can translate them into checked exceptions (§6.4).
 */
enum class CommandStreamValidationError : std::uint32_t {
    /** The byte range is smaller than the 32-byte stream header. */
    StreamTooSmall = 1,
    /** The stream base address is not 8-byte aligned (ADR-0002 D1). */
    MisalignedStreamBase = 2,
    /** header.totalByteSize does not equal the provided byte range size. */
    StreamSizeMismatch = 3,
    /** The magic bytes are not "BECS". */
    InvalidMagic = 4,
    /** versionMajor differs, or versionMinor is newer than this replayer supports (D6). */
    UnsupportedVersion = 5,
    /** A command header or payload runs past the end of the stream. */
    TruncatedCommand = 6,
    /** A command byteSize is below the 8-byte header or not a multiple of 8 (D1/D3). */
    MisalignedCommandSize = 7,
    /** The reserved commandFlags field is not zero (D3). */
    NonZeroReservedFlags = 8,
    /** An opcode value is not part of the assigned catalog (appendix A). */
    UnknownOpcode = 9,
    /** Command walk and header disagree (count or total size, D5 chain check). */
    CommandChainMismatch = 10,
};

} // namespace barrieww
