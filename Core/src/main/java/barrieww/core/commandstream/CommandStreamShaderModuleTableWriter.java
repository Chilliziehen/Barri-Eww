package barrieww.core.commandstream;

import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;

/**
 * @note ThreadSafety: Not thread-safe. One writer assembles one table on the single
 *       bake thread; never share an instance across threads.
 * Writes one ShaderModuleTable section (schema v0.1, mirrored by the native
 * CommandStreamShaderModuleTableValidator): an 8-byte header (entryCount, zero
 * reserved flags), entryCount 16-byte directory entries {blobByteOffset u64,
 * blobByteSize u64}, then the SPIR-V blob regions in add order, each at the next
 * 8-byte-aligned offset with zero-filled padding and an 8-aligned total
 * (deterministic bytes, §8 graphHash addressing). Blobs are shallowly checked on add
 * (SPIR-V magic, minimum size, 4-multiple size) so authoring mistakes fail fast here
 * instead of surfacing as native load-time validation errors.
 *
 * @warning MemoryOwnership: The writer stores REFERENCES to the added blob segments
 *          (no copy until writeTo); the caller must keep every added segment alive and
 *          unmodified until writeTo returns. The target segment is borrowed from the
 *          caller's Arena (§6.3); the writer never allocates or frees native memory.
 */
public final class CommandStreamShaderModuleTableWriter {

    private static final ValueLayout.OfInt s_unalignedLittleEndianIntegerLayout =
            ValueLayout.JAVA_INT_UNALIGNED.withOrder(ByteOrder.LITTLE_ENDIAN);
    private static final ValueLayout.OfLong s_unalignedLittleEndianLongLayout =
            ValueLayout.JAVA_LONG_UNALIGNED.withOrder(ByteOrder.LITTLE_ENDIAN);

    /** The SPIR-V magic word every blob must start with. */
    private static final int s_spirvMagicWord = 0x07230203;

    /** Magic, version, generator, bound and schema words form the minimal SPIR-V header. */
    private static final long s_minimumSpirvByteSize = 20;

    private final List<MemorySegment> m_pendingBlobs = new ArrayList<>();

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Registers one SPIR-V blob and returns its shader module slot. The blob is
     * shallowly verified immediately (fail fast): SPIR-V magic word, minimum size and
     * 4-multiple size.
     *
     * @param MemorySegment shaderBlob The SPIR-V bytes (heap or native segment)
     * @return int The slot number pipeline entries use to reference this module
     * @throws IllegalArgumentException When the blob fails the shallow SPIR-V check
     * @warning MemoryOwnership: shaderBlob is borrowed by reference; see the class
     *          note for the required lifetime.
     */
    public int addShaderBlob(MemorySegment shaderBlob) {
        if (shaderBlob.byteSize() < s_minimumSpirvByteSize
                || shaderBlob.byteSize() % 4 != 0) {
            throw new IllegalArgumentException(
                    "CommandStreamShaderModuleTableWriter: blob of " + shaderBlob.byteSize()
                            + " bytes is below the SPIR-V minimum or not a multiple of 4");
        }
        int firstBlobWord = shaderBlob.get(s_unalignedLittleEndianIntegerLayout, 0);
        if (firstBlobWord != s_spirvMagicWord) {
            throw new IllegalArgumentException(
                    "CommandStreamShaderModuleTableWriter: blob does not start with the"
                            + " SPIR-V magic word (got 0x"
                            + Integer.toHexString(firstBlobWord) + ")");
        }
        m_pendingBlobs.add(shaderBlob);
        return m_pendingBlobs.size() - 1;
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * The exact byte size the next writeTo() will emit (header, directory, aligned
     * blob regions and trailing padding).
     *
     * @return long The total table byte size; always a multiple of 8
     */
    public long requiredByteSize() {
        long layoutCursor = directoryEndOffset();
        for (MemorySegment pendingBlob : m_pendingBlobs) {
            layoutCursor = alignUpToCommandAlignment(layoutCursor + pendingBlob.byteSize());
        }
        return layoutCursor;
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Emits the complete table into targetSegment: zero-fills the emitted range, then
     * writes header, directory and blob regions. The writer keeps no emission state,
     * so writeTo may be called again (for example after adding more blobs).
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
                    "CommandStreamShaderModuleTableWriter: target segment holds "
                            + targetSegment.byteSize() + " bytes but the table needs "
                            + totalByteSize);
        }

        targetSegment.asSlice(0, totalByteSize).fill((byte) 0);
        targetSegment.set(s_unalignedLittleEndianIntegerLayout, 0, m_pendingBlobs.size());
        targetSegment.set(s_unalignedLittleEndianIntegerLayout, 4, 0);

        long layoutCursor = directoryEndOffset();
        for (int slotIndex = 0; slotIndex < m_pendingBlobs.size(); ++slotIndex) {
            MemorySegment pendingBlob = m_pendingBlobs.get(slotIndex);
            long entryOffset = 8L + 16L * slotIndex;
            targetSegment.set(s_unalignedLittleEndianLongLayout, entryOffset, layoutCursor);
            targetSegment.set(s_unalignedLittleEndianLongLayout, entryOffset + 8,
                    pendingBlob.byteSize());
            MemorySegment.copy(pendingBlob, 0, targetSegment, layoutCursor,
                    pendingBlob.byteSize());
            layoutCursor = alignUpToCommandAlignment(layoutCursor + pendingBlob.byteSize());
        }
        return totalByteSize;
    }

    /** The first byte after the directory: header plus one entry per pending blob. */
    private long directoryEndOffset() {
        return 8L + 16L * m_pendingBlobs.size();
    }

    /** Rounds byteOffset up to the next multiple of the 8-byte command alignment. */
    private static long alignUpToCommandAlignment(long byteOffset) {
        long alignment = CommandStreamFormat.s_commandAlignment;
        return (byteOffset + alignment - 1) & -alignment;
    }
}
