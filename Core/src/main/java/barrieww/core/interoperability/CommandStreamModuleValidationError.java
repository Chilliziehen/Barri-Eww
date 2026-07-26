package barrieww.core.interoperability;

/**
 * @note ThreadSafety: Enumeration; trivially thread-safe.
 * Stable Native module-validation error values mirrored from the Version 1 FFM ABI.
 */
public enum CommandStreamModuleValidationError {
    MODULE_TOO_SMALL(1),
    MISALIGNED_MODULE_BASE(2),
    MODULE_SIZE_MISMATCH(3),
    INVALID_MODULE_MAGIC(4),
    UNSUPPORTED_MODULE_VERSION(5),
    DIRECTORY_OUT_OF_BOUNDS(6),
    NON_ZERO_RESERVED_FLAGS(7),
    UNKNOWN_SECTION_TYPE(8),
    SECTION_OUT_OF_BOUNDS(9),
    MISALIGNED_SECTION_OFFSET(10),
    SECTION_OVERLAP(11),
    DUPLICATE_SECTION(12),
    LANE_STREAM_COUNT_MISMATCH(13),
    LANE_INDEX_MISMATCH(14),
    GRAPH_HASH_MISMATCH(15),
    LANE_STREAM_INVALID(16);

    private final int m_code;

    CommandStreamModuleValidationError(int code) {
        m_code = code;
    }

    /** Returns the stable Version 1 native integer code. */
    public int code() {
        return m_code;
    }

    /**
     * Converts one stable Version 1 integer code to its semantic value.
     *
     * @param int code Native module-validation error code
     * @return CommandStreamModuleValidationError Matching semantic value
     * @throws IllegalArgumentException When code is not assigned by Version 1
     */
    public static CommandStreamModuleValidationError fromCode(int code) {
        return switch (code) {
            case 1 -> MODULE_TOO_SMALL;
            case 2 -> MISALIGNED_MODULE_BASE;
            case 3 -> MODULE_SIZE_MISMATCH;
            case 4 -> INVALID_MODULE_MAGIC;
            case 5 -> UNSUPPORTED_MODULE_VERSION;
            case 6 -> DIRECTORY_OUT_OF_BOUNDS;
            case 7 -> NON_ZERO_RESERVED_FLAGS;
            case 8 -> UNKNOWN_SECTION_TYPE;
            case 9 -> SECTION_OUT_OF_BOUNDS;
            case 10 -> MISALIGNED_SECTION_OFFSET;
            case 11 -> SECTION_OVERLAP;
            case 12 -> DUPLICATE_SECTION;
            case 13 -> LANE_STREAM_COUNT_MISMATCH;
            case 14 -> LANE_INDEX_MISMATCH;
            case 15 -> GRAPH_HASH_MISMATCH;
            case 16 -> LANE_STREAM_INVALID;
            default -> throw new IllegalArgumentException(
                    "Unknown native module-validation error code: " + code);
        };
    }
}
