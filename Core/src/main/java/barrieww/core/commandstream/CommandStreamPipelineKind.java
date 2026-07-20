package barrieww.core.commandstream;

/**
 * @note ThreadSafety: Enum constants are immutable; fully thread-safe.
 * Pipeline kinds of the PipelineHandleTable schema (v0.1), mirroring the C++ side
 * (CommandStreamPipelineKind.hpp). COMPUTE is the only kind of the initial catalog;
 * GRAPHICS arrives with the rendering increments (its entries will need a richer
 * description reached through this kind discriminator, additive per ADR-0002 D6).
 */
public enum CommandStreamPipelineKind {
    COMPUTE(1);

    private final int m_rawPipelineKindValue;

    CommandStreamPipelineKind(int rawPipelineKindValue) {
        this.m_rawPipelineKindValue = rawPipelineKindValue;
    }

    /**
     * @note ThreadSafety: Thread-safe (immutable value read).
     * The raw 32-bit pipeline kind value as written on the wire.
     *
     * @return int The pipeline kind value
     */
    public int rawPipelineKindValue() {
        return m_rawPipelineKindValue;
    }
}
