package barrieww.core.commandstream;

/**
 * @note ThreadSafety: Immutable constants only; fully thread-safe.
 * Shared constants of the BECS wire format (ADR-0002 D1/D3). These values mirror the
 * C++ side (Native/include/BarriEww/CommandStream/CommandStreamHeader.hpp) and the
 * committed golden files under TestData/CommandStream are the arbiter that both sides
 * agree.
 */
public final class CommandStreamFormat {

    /** Magic bytes "BECS" identifying a lane stream. */
    public static final byte[] s_expectedMagicBytes = {0x42, 0x45, 0x43, 0x53};

    /** Current format major version; replayers accept exactly this major (ADR-0002 D6). */
    public static final short s_currentVersionMajor = 0;

    /** Current format minor version; minors are additive (ADR-0002 D6). */
    public static final short s_currentVersionMinor = 1;

    /** Byte size of the lane-stream header (ADR-0002 D3). */
    public static final int s_streamHeaderByteSize = 32;

    /** Byte size of every command header (opcode, reserved flags, byteSize). */
    public static final int s_commandHeaderByteSize = 8;

    /** Required alignment of every command and of the stream base (ADR-0002 D1). */
    public static final int s_commandAlignment = 8;

    private CommandStreamFormat() {
        // Constants holder; never instantiated.
    }
}
