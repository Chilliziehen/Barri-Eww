package barrieww.core.commandstream;

import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;

/**
 * @note ThreadSafety: Not thread-safe. One writer assembles one table on the single
 *       bake thread; never share an instance across threads.
 * Writes one BufferHandleTable section (schema v0.1): an 8-byte header (entryCount,
 * zero reserved flags) followed by fixed 24-byte entries. The returned slot numbers are
 * the indices streams use to reference buffers (ADR-0002 D2 slot indirection). Two
 * entry shapes exist: CREATED entries fully describe a buffer the native loader
 * creates; IMPORTED entries name a host-bound buffer by import identifier, with memory
 * kind NONE because the provider owns the memory (§6.3). Authoring mistakes fail fast
 * here instead of surfacing as native load-time validation errors.
 *
 * @warning MemoryOwnership: writeTo() borrows the caller's target segment (Arena-owned,
 *          §6.3); the writer never allocates or frees native memory.
 */
public final class CommandStreamBufferHandleTableWriter {

    private static final ValueLayout.OfInt s_unalignedLittleEndianIntegerLayout =
            ValueLayout.JAVA_INT_UNALIGNED.withOrder(ByteOrder.LITTLE_ENDIAN);
    private static final ValueLayout.OfLong s_unalignedLittleEndianLongLayout =
            ValueLayout.JAVA_LONG_UNALIGNED.withOrder(ByteOrder.LITTLE_ENDIAN);

    /** One collected entry (either created or imported shape). */
    private record PendingBufferEntry(
            long byteSize,
            int usageFlags,
            CommandStreamBufferMemoryKind memoryKind,
            int importIdentifier) {
    }

    private final List<PendingBufferEntry> m_pendingEntries = new ArrayList<>();

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Registers one CREATED buffer description and returns its slot number.
     *
     * @param long byteSize The buffer size in bytes; must be positive
     * @param int usageFlags OR-mask of CommandStreamBufferUsage bits; must be non-zero
     *        and contain only assigned bits
     * @param CommandStreamBufferMemoryKind memoryKind The memory placement; must not be
     *        NONE (NONE is reserved for imported entries)
     * @return int The slot number streams use to reference this buffer
     * @throws IllegalArgumentException When any argument violates the schema rules
     */
    public int addCreatedBuffer(long byteSize, int usageFlags,
                                CommandStreamBufferMemoryKind memoryKind) {
        if (byteSize <= 0) {
            throw new IllegalArgumentException(
                    "CommandStreamBufferHandleTableWriter: created buffer needs a positive"
                            + " byteSize, got " + byteSize);
        }
        if (usageFlags == 0
                || (usageFlags & ~CommandStreamBufferUsage.s_allKnownUsageFlags) != 0) {
            throw new IllegalArgumentException(
                    "CommandStreamBufferHandleTableWriter: usage mask 0x"
                            + Integer.toHexString(usageFlags)
                            + " is empty or contains unassigned bits");
        }
        if (memoryKind == CommandStreamBufferMemoryKind.NONE) {
            throw new IllegalArgumentException(
                    "CommandStreamBufferHandleTableWriter: created buffers need a concrete"
                            + " memory kind; NONE is reserved for imported entries");
        }
        m_pendingEntries.add(new PendingBufferEntry(byteSize, usageFlags, memoryKind, 0));
        return m_pendingEntries.size() - 1;
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Registers one IMPORTED buffer reference and returns its slot number. The native
     * loader binds the actual handle under this identifier at load time; the provider
     * keeps ownership of handle and memory (§6.3).
     *
     * @param int importIdentifier The host-side binding identifier; must be non-zero
     *        (zero marks created entries on the wire)
     * @return int The slot number streams use to reference this buffer
     * @throws IllegalArgumentException When importIdentifier is zero
     */
    public int addImportedBuffer(int importIdentifier) {
        if (importIdentifier == 0) {
            throw new IllegalArgumentException(
                    "CommandStreamBufferHandleTableWriter: importIdentifier 0 is reserved"
                            + " for created entries");
        }
        m_pendingEntries.add(new PendingBufferEntry(
                0, 0, CommandStreamBufferMemoryKind.NONE, importIdentifier));
        return m_pendingEntries.size() - 1;
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * The exact byte size the next writeTo() will emit.
     *
     * @return long Header plus 24 bytes per entry; always a multiple of 8
     */
    public long requiredByteSize() {
        return 8L + 24L * m_pendingEntries.size();
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Emits the complete table into targetSegment. The writer keeps no emission state,
     * so writeTo may be called again.
     *
     * @param MemorySegment targetSegment Destination; must be at least
     *        requiredByteSize() bytes
     * @return long The emitted byte size (equal to requiredByteSize())
     * @throws IllegalArgumentException When targetSegment is smaller than
     *         requiredByteSize()
     * @warning MemoryOwnership: targetSegment is borrowed; see the class note.
     */
    public long writeTo(MemorySegment targetSegment) {
        long totalByteSize = requiredByteSize();
        if (targetSegment.byteSize() < totalByteSize) {
            throw new IllegalArgumentException(
                    "CommandStreamBufferHandleTableWriter: target segment holds "
                            + targetSegment.byteSize() + " bytes but the table needs "
                            + totalByteSize);
        }

        targetSegment.set(s_unalignedLittleEndianIntegerLayout, 0, m_pendingEntries.size());
        targetSegment.set(s_unalignedLittleEndianIntegerLayout, 4, 0);

        long entryOffset = 8;
        for (PendingBufferEntry pendingEntry : m_pendingEntries) {
            targetSegment.set(s_unalignedLittleEndianLongLayout, entryOffset,
                    pendingEntry.byteSize());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset + 8,
                    pendingEntry.usageFlags());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset + 12,
                    pendingEntry.memoryKind().rawMemoryKindValue());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset + 16,
                    pendingEntry.importIdentifier());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset + 20, 0);
            entryOffset += 24;
        }
        return totalByteSize;
    }
}
