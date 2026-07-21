package barrieww.core.commandstream;

/**
 * @note ThreadSafety: Enum constants are immutable; fully thread-safe.
 * Backend-neutral image formats of the ImageHandleTable schema (v0.1 minimal set,
 * additive growth per ADR-0002 D6), mirroring the C++ side
 * (CommandStreamImageFormat.hpp). The native loader maps them onto concrete API
 * formats.
 */
public enum CommandStreamImageFormat {
    R8G8B8A8_UNORM(1),
    B8G8R8A8_UNORM(2),
    R16G16B16A16_FLOAT(3),
    R32_UINT(4),
    D32_FLOAT(5),
    D24_UNORM_S8_UINT(6);

    private final int m_rawImageFormatValue;

    CommandStreamImageFormat(int rawImageFormatValue) {
        this.m_rawImageFormatValue = rawImageFormatValue;
    }

    /**
     * @note ThreadSafety: Thread-safe (immutable value read).
     * The raw 32-bit image format value as written on the wire.
     *
     * @return int The image format value
     */
    public int rawImageFormatValue() {
        return m_rawImageFormatValue;
    }
}
