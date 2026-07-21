package barrieww.core.commandstream;

/**
 * @note ThreadSafety: Immutable constants only; fully thread-safe.
 * Backend-neutral image aspect bits (v0.1), mirroring the C++ side
 * (CommandStreamImageAspect.hpp). Used by image barrier descriptions and image command
 * payloads; the native recorder maps them onto concrete API aspects.
 */
public final class CommandStreamImageAspect {

    public static final int s_color = 0x1;
    public static final int s_depth = 0x2;
    public static final int s_stencil = 0x4;

    /** OR-mask of every image aspect bit assigned in the v0.1 schema. */
    public static final int s_allKnownAspectFlags = 0x7;

    private CommandStreamImageAspect() {
        // Constants holder; never instantiated.
    }
}
