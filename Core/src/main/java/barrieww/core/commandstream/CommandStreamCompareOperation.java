package barrieww.core.commandstream;

/**
 * @note ThreadSafety: Enum constants are immutable; fully thread-safe.
 * Backend-neutral compare operations of the GraphicsPipelineTable schema (v0.1; used by
 * depth testing), mirroring the C++ side (CommandStreamCompareOperation.hpp). Zero is
 * deliberately unassigned on the wire: a record with depth testing disabled carries 0,
 * so no usable operation can be confused with the disabled state.
 */
public enum CommandStreamCompareOperation {
    NEVER(1),
    LESS(2),
    EQUAL(3),
    LESS_OR_EQUAL(4),
    GREATER(5),
    NOT_EQUAL(6),
    GREATER_OR_EQUAL(7),
    ALWAYS(8);

    private final int m_rawCompareOperationValue;

    CommandStreamCompareOperation(int rawCompareOperationValue) {
        this.m_rawCompareOperationValue = rawCompareOperationValue;
    }

    /**
     * @note ThreadSafety: Thread-safe (immutable value read).
     * The raw 32-bit compare operation value as written on the wire.
     *
     * @return int The compare operation value
     */
    public int rawCompareOperationValue() {
        return m_rawCompareOperationValue;
    }
}
