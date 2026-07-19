package barrieww.core.commandstream;

/**
 * @note ThreadSafety: Enum constants are immutable; fully thread-safe.
 * Section types of a BECS module container (ADR-0002 D2), mirroring the C++ side
 * (CommandStreamModuleSectionType.hpp). Handle tables carry bake-time descriptions that
 * native init resolves into dense handle arrays; template tables carry baked barrier /
 * rendering / push-descriptor blueprints; LANE_STREAM sections embed one lane command
 * stream each.
 */
public enum CommandStreamModuleSectionType {
    PIPELINE_HANDLE_TABLE(0x0001),
    BUFFER_HANDLE_TABLE(0x0002),
    IMAGE_HANDLE_TABLE(0x0003),
    IMAGE_VIEW_HANDLE_TABLE(0x0004),
    SAMPLER_HANDLE_TABLE(0x0005),

    BARRIER_BATCH_TABLE(0x0010),
    RENDERING_TEMPLATE_TABLE(0x0011),
    PUSH_DESCRIPTOR_TEMPLATE_TABLE(0x0012),

    LANE_STREAM(0x0020);

    private final int m_rawSectionTypeValue;

    CommandStreamModuleSectionType(int rawSectionTypeValue) {
        this.m_rawSectionTypeValue = rawSectionTypeValue;
    }

    /**
     * @note ThreadSafety: Thread-safe (immutable value read).
     * The raw 16-bit section type value as written on the wire.
     *
     * @return int The section type value in [0x0000, 0xFFFF]
     */
    public int rawSectionTypeValue() {
        return m_rawSectionTypeValue;
    }
}
