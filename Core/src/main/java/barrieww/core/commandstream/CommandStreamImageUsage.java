package barrieww.core.commandstream;

/**
 * @note ThreadSafety: Immutable constants only; fully thread-safe.
 * Backend-neutral image usage bits of the ImageHandleTable schema (v0.1), mirroring
 * the C++ side (CommandStreamImageUsage.hpp). Entries carry an OR-mask of these; the
 * native loader maps them onto the concrete API's usage flags.
 */
public final class CommandStreamImageUsage {

    public static final int s_transferSource = 0x1;
    public static final int s_transferDestination = 0x2;
    public static final int s_sampled = 0x4;
    public static final int s_storage = 0x8;
    public static final int s_colorAttachment = 0x10;
    public static final int s_depthStencilAttachment = 0x20;

    /** OR-mask of every image usage bit assigned in the v0.1 schema. */
    public static final int s_allKnownUsageFlags = 0x3F;

    private CommandStreamImageUsage() {
        // Constants holder; never instantiated.
    }
}
