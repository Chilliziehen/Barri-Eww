package barrieww.core.commandstream;

/**
 * @note ThreadSafety: Enum constants are immutable; fully thread-safe.
 * Backend-neutral front-face winding orders of the GraphicsPipelineTable schema (v0.1),
 * mirroring the C++ side (CommandStreamFrontFace.hpp). The native loader maps them onto
 * the concrete API's front-face order.
 */
public enum CommandStreamFrontFace {
    COUNTER_CLOCKWISE(1),
    CLOCKWISE(2);

    private final int m_rawFrontFaceValue;

    CommandStreamFrontFace(int rawFrontFaceValue) {
        this.m_rawFrontFaceValue = rawFrontFaceValue;
    }

    /**
     * @note ThreadSafety: Thread-safe (immutable value read).
     * The raw 32-bit front-face value as written on the wire.
     *
     * @return int The front-face value
     */
    public int rawFrontFaceValue() {
        return m_rawFrontFaceValue;
    }
}
