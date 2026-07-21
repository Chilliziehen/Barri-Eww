package barrieww.core.commandstream;

/**
 * @note ThreadSafety: Immutable constants only; fully thread-safe.
 * Shared constants of the BECS module container wire format (ADR-0002 D2). These mirror
 * the C++ side (Native/include/BarriEww/CommandStream/CommandStreamModuleHeader.hpp and
 * CommandStreamModuleSectionEntry.hpp); the committed golden files under
 * TestData/CommandStream are the arbiter that both sides agree. The version pair is
 * shared with lane streams (one format family, single source in CommandStreamFormat).
 */
public final class CommandStreamModuleFormat {

    /** Magic bytes "BECM" identifying a module container. */
    public static final byte[] s_expectedMagicBytes = {0x42, 0x45, 0x43, 0x4D};

    /** Byte size of the module header (ADR-0002 D2). */
    public static final int s_moduleHeaderByteSize = 32;

    /** Byte size of one section directory entry. */
    public static final int s_sectionEntryByteSize = 24;

    private CommandStreamModuleFormat() {
        // Constants holder; never instantiated.
    }
}
