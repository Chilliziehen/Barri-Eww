package barrieww.core.commandstream;

/**
 * @note ThreadSafety: Enum constants are immutable; fully thread-safe.
 * Backend-neutral memory placement of a created buffer (BufferHandleTable schema v0.1),
 * mirroring the C++ side (CommandStreamBufferMemoryKind.hpp). NONE is reserved for
 * imported entries, whose memory is owned by the host that provides the handle (§6.3).
 */
public enum CommandStreamBufferMemoryKind {
    /** No placement: the entry is imported; the provider owns the memory. */
    NONE(0),
    /** Device-local memory; written through transfers or GPU work. */
    DEVICE_LOCAL(1),
    /** Host-visible, persistently mapped for per-frame parameter writes (ADR-0001). */
    HOST_VISIBLE_PERSISTENT_MAPPED(2),
    /** Host-visible, cached, for GPU-to-host readback. */
    HOST_VISIBLE_READBACK(3);

    private final int m_rawMemoryKindValue;

    CommandStreamBufferMemoryKind(int rawMemoryKindValue) {
        this.m_rawMemoryKindValue = rawMemoryKindValue;
    }

    /**
     * @note ThreadSafety: Thread-safe (immutable value read).
     * The raw 32-bit memory kind value as written on the wire.
     *
     * @return int The memory kind value
     */
    public int rawMemoryKindValue() {
        return m_rawMemoryKindValue;
    }
}
