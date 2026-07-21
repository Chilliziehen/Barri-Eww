package barrieww.core.commandstream;

/**
 * @note ThreadSafety: Enum constants are immutable; fully thread-safe.
 * BECS command opcodes, v0.1 catalog (ADR-0002 appendix A), mirroring the C++ side
 * (CommandStreamOpcode.hpp). The 16-bit opcode space is partitioned by backend
 * (ADR-0002 D4): 0x0000–0x0FFF backend-neutral core, 0x1000–0x1FFF Vulkan,
 * 0x2000–0x2FFF DX12 (reserved), 0x3000–0x3FFF Metal (reserved), 0xF000–0xFFFF
 * controlled-extension callouts.
 */
public enum CommandStreamOpcode {
    // Core range: state binding.
    BIND_GRAPHICS_PIPELINE(0x0001),
    BIND_COMPUTE_PIPELINE(0x0002),
    BIND_VERTEX_BUFFERS(0x0003),
    BIND_INDEX_BUFFER(0x0004),
    PUSH_CONSTANTS(0x0005),
    SET_VIEWPORT(0x0006),
    SET_SCISSOR(0x0007),
    /** Pushes a buffer's device address; the stream carries the SLOT, resolved at load. */
    PUSH_BUFFER_DEVICE_ADDRESS(0x0008),

    // Core range: draws.
    DRAW(0x0010),
    DRAW_INDEXED(0x0011),
    DRAW_INDIRECT(0x0012),
    DRAW_INDEXED_INDIRECT(0x0013),
    DRAW_INDEXED_INDIRECT_COUNT(0x0014),

    // Core range: compute.
    DISPATCH(0x0020),
    DISPATCH_INDIRECT(0x0021),

    // Core range: rendering scope and barriers.
    BEGIN_RENDERING(0x0030),
    END_RENDERING(0x0031),
    EXECUTE_BARRIER_BATCH(0x0032),

    // Core range: transfer operations.
    COPY_BUFFER(0x0040),
    COPY_IMAGE(0x0041),
    BLIT_IMAGE(0x0042),
    CLEAR_COLOR_IMAGE(0x0043),
    CLEAR_DEPTH_STENCIL_IMAGE(0x0044),
    RESOLVE_IMAGE(0x0045),
    COPY_BUFFER_TO_IMAGE(0x0046),
    COPY_IMAGE_TO_BUFFER(0x0047),

    // Vulkan range.
    PUSH_DESCRIPTOR_SET(0x1000),
    DEBUG_LABEL_BEGIN(0x1001),
    DEBUG_LABEL_END(0x1002),

    // Controlled-extension range (§8.3 opaque nodes).
    INVOKE_OPAQUE_EXTENSION_NODE(0xF000);

    private final int m_rawOpcodeValue;

    CommandStreamOpcode(int rawOpcodeValue) {
        this.m_rawOpcodeValue = rawOpcodeValue;
    }

    /**
     * @note ThreadSafety: Thread-safe (immutable value read).
     * The raw 16-bit opcode value as written on the wire.
     *
     * @return int The opcode value in [0x0000, 0xFFFF]
     */
    public int rawOpcodeValue() {
        return m_rawOpcodeValue;
    }
}
