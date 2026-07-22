package barrieww.core.commandstream;

/**
 * @note ThreadSafety: Immutable constants only; fully thread-safe.
 * Synchronization2 pipeline stage bits used in barrier records (v0.1 subset, additive
 * later). Values mirror Vulkan's VK_PIPELINE_STAGE_2_* bits; the bits below bit 32
 * intentionally equal the legacy sync1 values, which is what lets a 1.2-era native
 * recorder map baked barriers onto vkCmdPipelineBarrier (ADR-0002 semantic baseline).
 */
public final class CommandStreamPipelineStage {

    public static final long s_topOfPipe = 0x1L;
    public static final long s_drawIndirect = 0x2L;
    public static final long s_colorAttachmentOutput = 0x400L;
    public static final long s_computeShader = 0x800L;
    public static final long s_transfer = 0x1000L;
    public static final long s_allCommands = 0x10000L;

    private CommandStreamPipelineStage() {
        // Constants holder; never instantiated.
    }
}
