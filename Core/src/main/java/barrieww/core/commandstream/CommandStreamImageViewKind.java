package barrieww.core.commandstream;

/**
 * @note ThreadSafety: Enum constants are immutable; fully thread-safe.
 * Image-view dimensionality of the ImageViewHandleTable schema (v0.1), mirroring the
 * C++ side (CommandStreamImageViewKind.hpp). It is deliberately distinct from
 * CommandStreamImageKind: future minor versions can add array and cube view kinds
 * without changing image resource kinds.
 */
public enum CommandStreamImageViewKind {
    ONE_DIMENSIONAL(1),
    TWO_DIMENSIONAL(2),
    THREE_DIMENSIONAL(3);

    private final int m_rawImageViewKindValue;

    CommandStreamImageViewKind(int rawImageViewKindValue) {
        this.m_rawImageViewKindValue = rawImageViewKindValue;
    }

    /**
     * @note ThreadSafety: Thread-safe (immutable value read).
     * The raw 32-bit image-view kind value as written on the wire.
     *
     * @return int The image-view kind value
     */
    public int rawImageViewKindValue() {
        return m_rawImageViewKindValue;
    }
}
