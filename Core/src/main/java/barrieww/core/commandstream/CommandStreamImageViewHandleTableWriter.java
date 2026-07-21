package barrieww.core.commandstream;

import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;

/**
 * @note ThreadSafety: Not thread-safe. One writer assembles one table on the single
 *       bake thread; never share an instance across threads.
 * Writes one ImageViewHandleTable section (schema v0.1, mirrored by the native
 * CommandStreamImageViewHandleTableValidator): an 8-byte header (entryCount, zero
 * reserved flags) followed by fixed 32-byte entries. Every entry describes one static
 * view of an ImageHandleTable source slot. The native loader performs source-table,
 * Vulkan compatibility and materialization checks; this writer validates only the
 * fields that can be checked without the source table. v0.1 supports only 1D, 2D and
 * 3D non-array views, so arrayLayerCount is exactly one.
 *
 * @warning MemoryOwnership: writeTo() borrows the caller's target segment
 *          (Arena-owned, §6.3); the writer never allocates or frees native memory.
 */
public final class CommandStreamImageViewHandleTableWriter {

    private static final ValueLayout.OfInt s_unalignedLittleEndianIntegerLayout =
            ValueLayout.JAVA_INT_UNALIGNED.withOrder(ByteOrder.LITTLE_ENDIAN);

    /** One collected image-view description. */
    private record PendingImageViewEntry(
            int imageSlot,
            CommandStreamImageViewKind imageViewKind,
            CommandStreamImageFormat imageFormat,
            int aspectMaskValue,
            int baseMipLevel,
            int mipLevelCount,
            int baseArrayLayer,
            int arrayLayerCount) {
    }

    private final List<PendingImageViewEntry> m_pendingEntries = new ArrayList<>();

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Registers one static non-array image view and returns its slot number. Source
     * image existence, binding, format/kind compatibility and subresource bounds are
     * verified when native materialization joins this table with ImageHandleTable.
     *
     * @param int imageSlot Source ImageHandleTable slot; must be non-negative
     * @param CommandStreamImageViewKind imageViewKind View dimensionality; must not be
     *        null
     * @param CommandStreamImageFormat imageFormat View format; must not be null
     * @param int aspectMaskValue Non-empty mask of CommandStreamImageAspect bits only
     * @param int baseMipLevel First source mip level; must be non-negative
     * @param int mipLevelCount Explicit mip level count; must be positive
     * @param int baseArrayLayer First source array layer; must be non-negative and zero
     *        for a 3D view
     * @param int arrayLayerCount v0.1 requires exactly one non-array view layer
     * @return int The assigned ImageViewHandleTable slot
     * @throws IllegalArgumentException When any authoring-local argument violates the
     *         v0.1 schema
     */
    public int addImageView(int imageSlot, CommandStreamImageViewKind imageViewKind,
                            CommandStreamImageFormat imageFormat, int aspectMaskValue,
                            int baseMipLevel, int mipLevelCount, int baseArrayLayer,
                            int arrayLayerCount) {
        if (imageSlot < 0) {
            throw new IllegalArgumentException(
                    "CommandStreamImageViewHandleTableWriter: negative imageSlot " + imageSlot);
        }
        if (imageViewKind == null) {
            throw new IllegalArgumentException(
                    "CommandStreamImageViewHandleTableWriter: imageViewKind must not be null");
        }
        if (imageFormat == null) {
            throw new IllegalArgumentException(
                    "CommandStreamImageViewHandleTableWriter: imageFormat must not be null");
        }
        if (aspectMaskValue == 0
                || (aspectMaskValue & ~CommandStreamImageAspect.s_allKnownAspectFlags) != 0) {
            throw new IllegalArgumentException(
                    "CommandStreamImageViewHandleTableWriter: aspectMaskValue 0x"
                            + Integer.toHexString(aspectMaskValue)
                            + " must be non-empty and contain only assigned bits");
        }
        if (baseMipLevel < 0 || mipLevelCount <= 0 || baseArrayLayer < 0) {
            throw new IllegalArgumentException(
                    "CommandStreamImageViewHandleTableWriter: subresource bases must be"
                            + " non-negative and mipLevelCount must be positive");
        }
        if (imageViewKind == CommandStreamImageViewKind.THREE_DIMENSIONAL
                && baseArrayLayer != 0) {
            throw new IllegalArgumentException(
                    "CommandStreamImageViewHandleTableWriter: 3D views must begin at"
                            + " array layer zero");
        }
        if (arrayLayerCount != 1) {
            throw new IllegalArgumentException(
                    "CommandStreamImageViewHandleTableWriter: v0.1 supports only"
                            + " non-array views with exactly one array layer");
        }

        m_pendingEntries.add(new PendingImageViewEntry(imageSlot, imageViewKind, imageFormat,
                aspectMaskValue, baseMipLevel, mipLevelCount, baseArrayLayer,
                arrayLayerCount));
        return m_pendingEntries.size() - 1;
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * The exact byte size the next writeTo() will emit.
     *
     * @return long Header plus 32 bytes per entry; always a multiple of 8
     */
    public long requiredByteSize() {
        return 8L + 32L * m_pendingEntries.size();
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
                    "CommandStreamImageViewHandleTableWriter: target segment holds "
                            + targetSegment.byteSize() + " bytes but the table needs "
                            + totalByteSize);
        }

        targetSegment.set(s_unalignedLittleEndianIntegerLayout, 0, m_pendingEntries.size());
        targetSegment.set(s_unalignedLittleEndianIntegerLayout, 4, 0);

        long entryOffset = 8;
        for (PendingImageViewEntry pendingEntry : m_pendingEntries) {
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset,
                    pendingEntry.imageSlot());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset + 4,
                    pendingEntry.imageViewKind().rawImageViewKindValue());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset + 8,
                    pendingEntry.imageFormat().rawImageFormatValue());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset + 12,
                    pendingEntry.aspectMaskValue());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset + 16,
                    pendingEntry.baseMipLevel());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset + 20,
                    pendingEntry.mipLevelCount());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset + 24,
                    pendingEntry.baseArrayLayer());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset + 28,
                    pendingEntry.arrayLayerCount());
            entryOffset += 32;
        }
        return totalByteSize;
    }
}
