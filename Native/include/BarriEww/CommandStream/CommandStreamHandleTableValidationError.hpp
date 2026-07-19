#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumeration; trivially thread-safe.
 * @brief Failure categories of handle-table load-time validation, shared by every
 *        table schema (buffer now; image/imageView/sampler as they land) so callers
 *        translate one error space per §6.4. Values are stable.
 */
enum class CommandStreamHandleTableValidationError : std::uint32_t {
    /** The byte range is smaller than the 8-byte table header. */
    TableTooSmall = 1,
    /** The table base address is not 8-byte aligned (ADR-0002 D1). */
    MisalignedTableBase = 2,
    /** The byte range does not equal header size + entryCount entries exactly. */
    TableSizeMismatch = 3,
    /** A reserved flags field (header or entry) is not zero. */
    NonZeroReservedFlags = 4,
    /** An entry's memory kind value is not assigned in this schema version. */
    UnknownMemoryKind = 5,
    /** An entry's usage mask contains bits not assigned in this schema version. */
    UnknownUsageFlags = 6,
    /** A created entry (importIdentifier 0) has zero byteSize, zero usage, or kind None. */
    InvalidCreatedBufferDescription = 7,
    /** An imported entry (importIdentifier != 0) has a memory kind other than None. */
    InvalidImportedBufferDescription = 8,
};

} // namespace barrieww
