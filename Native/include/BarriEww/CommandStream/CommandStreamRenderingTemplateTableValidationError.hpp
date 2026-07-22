#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumeration; trivially thread-safe.
 * @brief Failure categories of RenderingTemplateTable load-time validation (schema
 *        v0.1). Values are stable so the Java side can translate them into checked
 *        exceptions (§6.4).
 */
enum class CommandStreamRenderingTemplateTableValidationError : std::uint32_t {
    /** The byte range is smaller than the 8-byte table header. */
    TableTooSmall = 1,
    /** The table base address is not 8-byte aligned (ADR-0002 D1). */
    MisalignedTableBase = 2,
    /** The header reserved flags field is not zero. */
    NonZeroReservedFlags = 3,
    /** The template directory does not fit inside the table. */
    DirectoryOutOfBounds = 4,
    /** A template's attachmentsByteOffset is not 8-byte aligned. */
    MisalignedAttachmentRegion = 5,
    /** A template's attachment records leave the table or intrude into the directory. */
    AttachmentRegionOutOfBounds = 6,
    /** depthAttachmentPresent is a value other than 0 or 1. */
    InvalidDepthAttachmentFlag = 7,
    /** A template has a zero-area render area. */
    EmptyRenderArea = 8,
    /** A template has a zero layer count. */
    ZeroLayerCount = 9,
    /** A template has neither a color attachment nor a depth attachment. */
    EmptyAttachmentSet = 10,
    /** An attachment record carries an unassigned image layout value. */
    UnknownAttachmentLayout = 11,
    /** An attachment record carries an unassigned load operation. */
    UnknownAttachmentLoadOp = 12,
    /** An attachment record carries an unassigned store operation. */
    UnknownAttachmentStoreOp = 13,
};

} // namespace barrieww
