package barrieww.core.interoperability;

/**
 * @note ThreadSafety: Enumeration; trivially thread-safe.
 * Stable per-frame outcome values mirrored from the Version 1 presentation FFM ABI; ordinary
 * swapchain recreation is a status, not an exception.
 */
public enum PresentationFrameStatus {
    SUCCESS(0),
    SURFACE_UNAVAILABLE(1),
    RECREATE_REQUIRED(2),
    SUBOPTIMAL(3);

    private final int m_code;

    PresentationFrameStatus(int code) {
        m_code = code;
    }

    /** Returns the stable Version 1 native integer value. */
    public int code() {
        return m_code;
    }

    /**
     * Converts one stable Version 1 integer value to its semantic status.
     *
     * @param int code Native frame-status value
     * @return PresentationFrameStatus Matching semantic status
     * @throws IllegalArgumentException When the value is not assigned by Version 1
     */
    public static PresentationFrameStatus fromCode(int code) {
        return switch (code) {
            case 0 -> SUCCESS;
            case 1 -> SURFACE_UNAVAILABLE;
            case 2 -> RECREATE_REQUIRED;
            case 3 -> SUBOPTIMAL;
            default -> throw new IllegalArgumentException(
                    "Unknown native presentation frame status: " + code);
        };
    }
}
