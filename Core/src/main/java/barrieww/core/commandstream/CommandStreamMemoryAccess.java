package barrieww.core.commandstream;

/**
 * @note ThreadSafety: Immutable constants only; fully thread-safe.
 * Synchronization2 memory access bits used in barrier records (v0.1 subset, additive
 * later). Values mirror Vulkan's VK_ACCESS_2_* bits; see CommandStreamPipelineStage
 * for the legacy-mapping rationale.
 */
public final class CommandStreamMemoryAccess {

    public static final long s_indirectCommandRead = 0x1L;
    public static final long s_shaderRead = 0x20L;
    public static final long s_shaderWrite = 0x40L;
    public static final long s_transferRead = 0x800L;
    public static final long s_transferWrite = 0x1000L;

    private CommandStreamMemoryAccess() {
        // Constants holder; never instantiated.
    }
}
