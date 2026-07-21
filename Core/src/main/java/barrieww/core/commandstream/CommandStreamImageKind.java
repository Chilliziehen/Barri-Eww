package barrieww.core.commandstream;

/**
 * @note ThreadSafety: Enum constants are immutable; fully thread-safe.
 * Image dimensionality of the ImageHandleTable schema (v0.1), mirroring the C++ side
 * (CommandStreamImageKind.hpp). The native loader maps the kind onto the concrete
 * API's image type.
 */
public enum CommandStreamImageKind {
    ONE_DIMENSIONAL(1),
    TWO_DIMENSIONAL(2),
    THREE_DIMENSIONAL(3);

    private final int m_rawImageKindValue;

    CommandStreamImageKind(int rawImageKindValue) {
        this.m_rawImageKindValue = rawImageKindValue;
    }

    /**
     * @note ThreadSafety: Thread-safe (immutable value read).
     * The raw 32-bit image kind value as written on the wire.
     *
     * @return int The image kind value
     */
    public int rawImageKindValue() {
        return m_rawImageKindValue;
    }
}
