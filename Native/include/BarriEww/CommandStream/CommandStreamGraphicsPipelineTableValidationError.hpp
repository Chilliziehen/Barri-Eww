#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumeration; trivially thread-safe.
 * @brief Failure categories of GraphicsPipelineTable load-time validation (schema
 *        v0.1, §9.14). Values are stable so the Java side can translate them into
 *        checked exceptions (§6.4).
 */
enum class CommandStreamGraphicsPipelineTableValidationError : std::uint32_t {
    /** The byte range is smaller than the 8-byte table header. */
    TableTooSmall = 1,
    /** The table base address is not 8-byte aligned (ADR-0002 D1). */
    MisalignedTableBase = 2,
    /** The header reserved flags field is not zero. */
    NonZeroReservedFlags = 3,
    /** The byte range is not exactly 8 + pipelineCount * 80 bytes. */
    TableSizeMismatch = 4,
    /** A record's reserved flags field is not zero. */
    NonZeroRecordReservedFlags = 5,
    /** A record carries an unassigned primitive topology. */
    UnknownTopology = 6,
    /** A record carries an unassigned cull mode. */
    UnknownCullMode = 7,
    /** A record carries an unassigned front-face winding order. */
    UnknownFrontFace = 8,
    /** pushConstantByteSize is not a multiple of 4 or exceeds 128. */
    InvalidPushConstantByteSize = 9,
    /** colorAttachmentCount exceeds the fixed maximum of 8. */
    TooManyColorAttachments = 10,
    /** A record has neither a color attachment nor a depth attachment. */
    EmptyAttachmentSet = 11,
    /** A used color format index is unassigned or names a non-color format. */
    UnknownColorAttachmentFormat = 12,
    /** A color format index at or beyond colorAttachmentCount is not zero. */
    NonZeroUnusedColorFormat = 13,
    /** depthAttachmentFormatValue is neither zero nor an assigned depth-capable format. */
    InvalidDepthAttachmentFormat = 14,
    /** depthTestEnable or depthWriteEnable is a value other than 0 or 1. */
    InvalidDepthEnableFlag = 15,
    /** depthWriteEnable is 1 while depthTestEnable is 0. */
    DepthWriteWithoutDepthTest = 16,
    /** Depth testing is enabled but the record declares no depth attachment format. */
    DepthStateWithoutDepthAttachment = 17,
    /** depthTestEnable is 1 but the compare operation is unassigned. */
    UnknownDepthCompareOperation = 18,
    /** depthTestEnable is 0 but the compare operation is not zero. */
    NonZeroDisabledDepthCompareOperation = 19,
};

} // namespace barrieww
