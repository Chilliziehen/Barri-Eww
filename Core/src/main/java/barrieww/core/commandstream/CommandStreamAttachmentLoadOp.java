package barrieww.core.commandstream;

/**
 * @note ThreadSafety: Enum constants are immutable; fully thread-safe.
 * Backend-neutral attachment load operations of the RenderingTemplateTable schema
 * (v0.1), mirroring the C++ side (CommandStreamAttachmentLoadOp.hpp). The native
 * recorder maps them onto the concrete API's load operation.
 */
public enum CommandStreamAttachmentLoadOp {
    LOAD(0),
    CLEAR(1),
    DONT_CARE(2);

    private final int m_rawLoadOpValue;

    CommandStreamAttachmentLoadOp(int rawLoadOpValue) {
        this.m_rawLoadOpValue = rawLoadOpValue;
    }

    /**
     * @note ThreadSafety: Thread-safe (immutable value read).
     * The raw 32-bit load operation value as written on the wire.
     *
     * @return int The load operation value
     */
    public int rawLoadOpValue() {
        return m_rawLoadOpValue;
    }
}
