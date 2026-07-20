#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumeration; trivially thread-safe.
 * @brief Failure categories of BarrierBatchTable load-time validation (schema v0.1).
 *        Values are stable so the Java side can translate them into checked
 *        exceptions (§6.4).
 */
enum class CommandStreamBarrierBatchTableValidationError : std::uint32_t {
    /** The byte range is smaller than the 8-byte table header. */
    TableTooSmall = 1,
    /** The table base address is not 8-byte aligned (ADR-0002 D1). */
    MisalignedTableBase = 2,
    /** A reserved flags field (header or buffer barrier record) is not zero. */
    NonZeroReservedFlags = 3,
    /** The batch directory does not fit inside the table. */
    DirectoryOutOfBounds = 4,
    /** A batch's barriersByteOffset is not 8-byte aligned. */
    MisalignedBarrierRegion = 5,
    /** A batch's barrier records leave the table or intrude into the directory. */
    BarrierRegionOutOfBounds = 6,
    /** A barrier record has an empty source or destination stage mask. */
    ZeroStageMask = 7,
};

} // namespace barrieww
