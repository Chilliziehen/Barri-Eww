package barrieww.core.interoperability;

/**
 * @note ThreadSafety: Enumeration; trivially thread-safe.
 * Stable embedded-lane validation error values mirrored from the Version 1 FFM ABI.
 */
public enum CommandStreamValidationError {
    STREAM_TOO_SMALL(1),
    MISALIGNED_STREAM_BASE(2),
    STREAM_SIZE_MISMATCH(3),
    INVALID_MAGIC(4),
    UNSUPPORTED_VERSION(5),
    TRUNCATED_COMMAND(6),
    MISALIGNED_COMMAND_SIZE(7),
    NON_ZERO_RESERVED_FLAGS(8),
    UNKNOWN_OPCODE(9),
    COMMAND_CHAIN_MISMATCH(10);

    private final int m_code;

    CommandStreamValidationError(int code) {
        m_code = code;
    }

    /** Returns the stable Version 1 native integer code. */
    public int code() {
        return m_code;
    }

    /**
     * Converts one stable Version 1 integer code to its semantic value.
     *
     * @param int code Native lane-validation error code
     * @return CommandStreamValidationError Matching semantic value
     * @throws IllegalArgumentException When code is not assigned by Version 1
     */
    public static CommandStreamValidationError fromCode(int code) {
        return switch (code) {
            case 1 -> STREAM_TOO_SMALL;
            case 2 -> MISALIGNED_STREAM_BASE;
            case 3 -> STREAM_SIZE_MISMATCH;
            case 4 -> INVALID_MAGIC;
            case 5 -> UNSUPPORTED_VERSION;
            case 6 -> TRUNCATED_COMMAND;
            case 7 -> MISALIGNED_COMMAND_SIZE;
            case 8 -> NON_ZERO_RESERVED_FLAGS;
            case 9 -> UNKNOWN_OPCODE;
            case 10 -> COMMAND_CHAIN_MISMATCH;
            default -> throw new IllegalArgumentException(
                    "Unknown native lane-validation error code: " + code);
        };
    }
}
