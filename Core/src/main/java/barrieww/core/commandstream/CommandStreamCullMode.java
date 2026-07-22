package barrieww.core.commandstream;

/**
 * @note ThreadSafety: Enum constants are immutable; fully thread-safe.
 * Backend-neutral cull modes of the GraphicsPipelineTable schema (v0.1), mirroring the
 * C++ side (CommandStreamCullMode.hpp). The native loader maps them onto the concrete
 * API's cull mode.
 */
public enum CommandStreamCullMode {
    NONE(1),
    FRONT(2),
    BACK(3),
    FRONT_AND_BACK(4);

    private final int m_rawCullModeValue;

    CommandStreamCullMode(int rawCullModeValue) {
        this.m_rawCullModeValue = rawCullModeValue;
    }

    /**
     * @note ThreadSafety: Thread-safe (immutable value read).
     * The raw 32-bit cull mode value as written on the wire.
     *
     * @return int The cull mode value
     */
    public int rawCullModeValue() {
        return m_rawCullModeValue;
    }
}
