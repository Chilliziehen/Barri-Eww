#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumeration; trivially thread-safe.
 * @brief Failure categories of load-time ImageViewHandleTable materialization. Values
 *        are stable so the Java side can translate them into checked exceptions (§6.4).
 */
enum class VulkanImageViewTableCreationError : std::uint32_t {
    /** The entry references no slot in the supplied source VulkanImageTable. */
    SourceImageSlotOutOfRange = 1,
    /** The referenced source slot is an unbound imported image placeholder. */
    SourceImageSlotUnbound = 2,
    /** v0.1 view format differs from the non-mutable source image format. */
    SourceImageFormatMismatch = 3,
    /** The view aspect mask is incompatible with the source image format. */
    SourceImageAspectMismatch = 4,
    /** The v0.1 image-view kind does not match the source image kind. */
    SourceImageKindMismatch = 5,
    /** The explicit mip or layer range falls outside the source image description. */
    SourceImageSubresourceRangeOutOfBounds = 6,
    /** The source image has no usage that permits image-view creation in v0.1. */
    SourceImageUsageDoesNotSupportImageViews = 7,
    /** vkCreateImageView failed for a schema-compatible entry. */
    ImageViewCreationFailed = 8,
    /** No source image-table owner was supplied. */
    MissingSourceImageTable = 9,
};

} // namespace barrieww
