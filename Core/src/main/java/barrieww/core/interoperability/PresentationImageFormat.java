package barrieww.core.interoperability;

/**
 * @note ThreadSafety: Immutable enum constants; safe to use from any thread.
 * Stable presentation-neutral UNORM image formats shared by the Java and Native Version 1 ABI.
 */
public enum PresentationImageFormat {
    /** Four 8-bit UNORM channels ordered red, green, blue and alpha. */
    R8G8B8A8_UNORM(1),
    /** Four 8-bit UNORM channels ordered blue, green, red and alpha. */
    B8G8R8A8_UNORM(2);

    private final int m_rawValue;

    /**
     * Creates one stable ABI image-format value.
     *
     * @param int rawValue Stable Version 1 ABI value
     */
    PresentationImageFormat(int rawValue) {
        m_rawValue = rawValue;
    }

    /** Returns the stable Version 1 ABI value. */
    public int rawValue() {
        return m_rawValue;
    }

    /**
     * Strictly decodes one stable Version 1 ABI value.
     *
     * @param int rawValue Stable Version 1 ABI value
     * @return PresentationImageFormat Matching image format
     * @throws IllegalArgumentException When rawValue is not assigned by Version 1
     */
    public static PresentationImageFormat fromRawValue(int rawValue) {
        return switch (rawValue) {
            case 1 -> R8G8B8A8_UNORM;
            case 2 -> B8G8R8A8_UNORM;
            default -> throw new IllegalArgumentException(
                    "Unknown presentation image format value: " + rawValue);
        };
    }
}
