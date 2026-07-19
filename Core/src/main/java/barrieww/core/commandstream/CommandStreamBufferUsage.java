package barrieww.core.commandstream;

/**
 * @note ThreadSafety: Immutable constants only; fully thread-safe.
 * Backend-neutral buffer usage bits of the BufferHandleTable schema (v0.1), mirroring
 * the C++ side (CommandStreamBufferUsage.hpp). Entries carry an OR-mask of these; the
 * native loader maps them onto the concrete API's usage flags at creation time.
 */
public final class CommandStreamBufferUsage {

    public static final int s_transferSource = 0x1;
    public static final int s_transferDestination = 0x2;
    public static final int s_vertex = 0x4;
    public static final int s_index = 0x8;
    public static final int s_uniform = 0x10;
    public static final int s_storage = 0x20;
    public static final int s_indirect = 0x40;

    /** OR-mask of every usage bit assigned in the v0.1 schema. */
    public static final int s_allKnownUsageFlags = 0x7F;

    private CommandStreamBufferUsage() {
        // Constants holder; never instantiated.
    }
}
