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
    /** A pipeline entry's kind value is not assigned in this schema version. */
    UnknownPipelineKind = 9,
    /** A pipeline entry's push constant size is not a multiple of 4 or exceeds 128. */
    InvalidPushConstantByteSize = 10,
    /** A shader blob region leaves the table or intrudes into the directory. */
    ShaderBlobOutOfBounds = 11,
    /** A shader blob offset is not 8-byte aligned. */
    MisalignedShaderBlob = 12,
    /** A shader blob does not start with the SPIR-V magic word. */
    InvalidShaderBlobMagic = 13,
    /** A shader blob size is below the SPIR-V minimum or not a multiple of 4. */
    InvalidShaderBlobByteSize = 14,
    /** An image entry's kind value is not assigned in this schema version. */
    UnknownImageKind = 15,
    /** An image entry's format value is not assigned in this schema version. */
    UnknownImageFormat = 16,
    /** An image entry's sample count is not 1, 2, 4 or 8. */
    UnknownSampleCount = 17,
    /** An image entry has a zero dimension/count, or a 3D image with layers != 1. */
    InvalidImageDimensions = 18,
    /** A created image entry (importIdentifier 0) has an empty usage mask. */
    InvalidCreatedImageDescription = 19,
    /** An image-view entry's kind value is not assigned in this schema version. */
    UnknownImageViewKind = 20,
    /** An image-view entry's aspect mask is empty or contains unassigned bits. */
    UnusableImageAspectMask = 21,
    /** An image-view entry has an invalid explicit mip/layer range for its view kind. */
    InvalidImageViewSubresourceCounts = 22,
};

} // namespace barrieww
