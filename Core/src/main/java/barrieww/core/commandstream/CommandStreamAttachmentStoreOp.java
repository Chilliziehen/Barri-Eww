package barrieww.core.commandstream;

/**
 * @note ThreadSafety: Enum constants are immutable; fully thread-safe.
 * Backend-neutral attachment store operations of the RenderingTemplateTable schema
 * (v0.1), mirroring the C++ side (CommandStreamAttachmentStoreOp.hpp). The native
 * recorder maps them onto the concrete API's store operation.
 */
public enum CommandStreamAttachmentStoreOp {
    STORE(0),
    DONT_CARE(1);

    private final int m_rawStoreOpValue;

    CommandStreamAttachmentStoreOp(int rawStoreOpValue) {
        this.m_rawStoreOpValue = rawStoreOpValue;
    }

    /**
     * @note ThreadSafety: Thread-safe (immutable value read).
     * The raw 32-bit store operation value as written on the wire.
     *
     * @return int The store operation value
     */
    public int rawStoreOpValue() {
        return m_rawStoreOpValue;
    }
}
