package barrieww.core.commandstream;

/**
 * @note ThreadSafety: Enum constants are immutable; fully thread-safe.
 * Backend-neutral primitive topologies of the GraphicsPipelineTable schema (v0.1),
 * mirroring the C++ side (CommandStreamPrimitiveTopology.hpp). The native loader maps
 * them onto the concrete API's topology.
 */
public enum CommandStreamPrimitiveTopology {
    POINT_LIST(1),
    LINE_LIST(2),
    LINE_STRIP(3),
    TRIANGLE_LIST(4),
    TRIANGLE_STRIP(5);

    private final int m_rawTopologyValue;

    CommandStreamPrimitiveTopology(int rawTopologyValue) {
        this.m_rawTopologyValue = rawTopologyValue;
    }

    /**
     * @note ThreadSafety: Thread-safe (immutable value read).
     * The raw 32-bit topology value as written on the wire.
     *
     * @return int The topology value
     */
    public int rawTopologyValue() {
        return m_rawTopologyValue;
    }
}
