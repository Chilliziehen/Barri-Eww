package barrieww.core.commandstream;

/**
 * @note ThreadSafety: Enum constants are immutable; fully thread-safe.
 * Backend-neutral image layouts (v0.1), mirroring the C++ side
 * (CommandStreamImageLayout.hpp). Used by image barrier descriptions and by image
 * commands that must state the layout the image is in; the native recorder maps them
 * onto concrete API layouts.
 */
public enum CommandStreamImageLayout {
    UNDEFINED(0),
    GENERAL(1),
    COLOR_ATTACHMENT(2),
    DEPTH_STENCIL_ATTACHMENT(3),
    SHADER_READ_ONLY(4),
    TRANSFER_SOURCE(5),
    TRANSFER_DESTINATION(6),
    PRESENT(7);

    private final int m_rawImageLayoutValue;

    CommandStreamImageLayout(int rawImageLayoutValue) {
        this.m_rawImageLayoutValue = rawImageLayoutValue;
    }

    /**
     * @note ThreadSafety: Thread-safe (immutable value read).
     * The raw 32-bit image layout value as written on the wire.
     *
     * @return int The image layout value
     */
    public int rawImageLayoutValue() {
        return m_rawImageLayoutValue;
    }
}
