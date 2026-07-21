package barrieww.core.commandstream;

import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;

/**
 * @note ThreadSafety: Not thread-safe. One writer assembles one table on the single
 *       bake thread; never share an instance across threads.
 * Writes one ImageHandleTable section (schema v0.1, mirrored by the native
 * CommandStreamImageHandleTableValidator): an 8-byte header (entryCount, zero reserved
 * flags) followed by fixed 40-byte entries. CREATED entries fully describe an image
 * the native loader creates (device-local, optimal tiling, initial layout UNDEFINED —
 * the compile-time barrier pass owns the first transition); IMPORTED entries name a
 * host-bound image (for example a Minecraft swapchain image) by import identifier.
 * Descriptive fields must be valid for both shapes (the host may verify a bound image
 * against them); only the non-empty usage rule is created-specific. Authoring mistakes
 * fail fast here instead of surfacing as native load-time validation errors.
 *
 * @warning MemoryOwnership: writeTo() borrows the caller's target segment (Arena-owned,
 *          §6.3); the writer never allocates or frees native memory.
 */
public final class CommandStreamImageHandleTableWriter {

    private static final ValueLayout.OfInt s_unalignedLittleEndianIntegerLayout =
            ValueLayout.JAVA_INT_UNALIGNED.withOrder(ByteOrder.LITTLE_ENDIAN);

    /** One collected entry (either created or imported shape). */
    private record PendingImageEntry(
            CommandStreamImageKind imageKind,
            CommandStreamImageFormat imageFormat,
            int width,
            int height,
            int depth,
            int mipLevelCount,
            int arrayLayerCount,
            int sampleCountValue,
            int usageFlags,
            int importIdentifier) {
    }

    private final List<PendingImageEntry> m_pendingEntries = new ArrayList<>();

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Registers one CREATED image description and returns its slot number.
     *
     * @param CommandStreamImageKind imageKind The image dimensionality
     * @param CommandStreamImageFormat imageFormat The neutral format
     * @param int width Texel width; must be positive
     * @param int height Texel height; must be positive
     * @param int depth Texel depth; must be positive (1 for 1D/2D images)
     * @param int mipLevelCount Number of mip levels; must be positive
     * @param int arrayLayerCount Number of array layers; must be positive and exactly
     *        1 for 3D images
     * @param int sampleCountValue Sample count; must be 1, 2, 4 or 8
     * @param int usageFlags OR-mask of CommandStreamImageUsage bits; must be non-zero
     *        and contain only assigned bits
     * @return int The slot number streams and barriers use to reference this image
     * @throws IllegalArgumentException When any argument violates the schema rules
     */
    public int addCreatedImage(CommandStreamImageKind imageKind,
                               CommandStreamImageFormat imageFormat, int width, int height,
                               int depth, int mipLevelCount, int arrayLayerCount,
                               int sampleCountValue, int usageFlags) {
        if (usageFlags == 0) {
            throw new IllegalArgumentException(
                    "CommandStreamImageHandleTableWriter: created images need a non-empty"
                            + " usage mask");
        }
        return addEntry(imageKind, imageFormat, width, height, depth, mipLevelCount,
                arrayLayerCount, sampleCountValue, usageFlags, 0);
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Registers one IMPORTED image reference and returns its slot number. The native
     * loader binds the actual handle under the identifier at load time; the provider
     * keeps ownership (§6.3). The descriptive fields must still be valid so the host
     * can verify the bound image against them.
     *
     * @param int importIdentifier The host-side binding identifier; must be non-zero
     * @param CommandStreamImageKind imageKind The image dimensionality
     * @param CommandStreamImageFormat imageFormat The neutral format
     * @param int width Texel width; must be positive
     * @param int height Texel height; must be positive
     * @param int depth Texel depth; must be positive
     * @param int mipLevelCount Number of mip levels; must be positive
     * @param int arrayLayerCount Number of array layers; must be positive and exactly
     *        1 for 3D images
     * @param int sampleCountValue Sample count; must be 1, 2, 4 or 8
     * @param int usageFlags OR-mask of CommandStreamImageUsage bits; may be zero
     * @return int The slot number streams and barriers use to reference this image
     * @throws IllegalArgumentException When any argument violates the schema rules
     */
    public int addImportedImage(int importIdentifier, CommandStreamImageKind imageKind,
                                CommandStreamImageFormat imageFormat, int width, int height,
                                int depth, int mipLevelCount, int arrayLayerCount,
                                int sampleCountValue, int usageFlags) {
        if (importIdentifier == 0) {
            throw new IllegalArgumentException(
                    "CommandStreamImageHandleTableWriter: importIdentifier 0 is reserved"
                            + " for created entries");
        }
        return addEntry(imageKind, imageFormat, width, height, depth, mipLevelCount,
                arrayLayerCount, sampleCountValue, usageFlags, importIdentifier);
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * The exact byte size the next writeTo() will emit.
     *
     * @return long Header plus 40 bytes per entry; always a multiple of 8
     */
    public long requiredByteSize() {
        return 8L + 40L * m_pendingEntries.size();
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
                    "CommandStreamImageHandleTableWriter: target segment holds "
                            + targetSegment.byteSize() + " bytes but the table needs "
                            + totalByteSize);
        }

        targetSegment.set(s_unalignedLittleEndianIntegerLayout, 0, m_pendingEntries.size());
        targetSegment.set(s_unalignedLittleEndianIntegerLayout, 4, 0);

        long entryOffset = 8;
        for (PendingImageEntry pendingEntry : m_pendingEntries) {
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset,
                    pendingEntry.imageKind().rawImageKindValue());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset + 4,
                    pendingEntry.imageFormat().rawImageFormatValue());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset + 8,
                    pendingEntry.width());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset + 12,
                    pendingEntry.height());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset + 16,
                    pendingEntry.depth());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset + 20,
                    pendingEntry.mipLevelCount());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset + 24,
                    pendingEntry.arrayLayerCount());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset + 28,
                    pendingEntry.sampleCountValue());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset + 32,
                    pendingEntry.usageFlags());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset + 36,
                    pendingEntry.importIdentifier());
            entryOffset += 40;
        }
        return totalByteSize;
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Shared field validation and collection for both entry shapes.
     *
     * @param CommandStreamImageKind imageKind The image dimensionality
     * @param CommandStreamImageFormat imageFormat The neutral format
     * @param int width Texel width
     * @param int height Texel height
     * @param int depth Texel depth
     * @param int mipLevelCount Number of mip levels
     * @param int arrayLayerCount Number of array layers
     * @param int sampleCountValue Sample count
     * @param int usageFlags OR-mask of CommandStreamImageUsage bits
     * @param int importIdentifier Zero for created entries, non-zero for imported
     * @return int The assigned slot number
     * @throws IllegalArgumentException When any field violates the schema rules
     */
    private int addEntry(CommandStreamImageKind imageKind,
                         CommandStreamImageFormat imageFormat, int width, int height,
                         int depth, int mipLevelCount, int arrayLayerCount,
                         int sampleCountValue, int usageFlags, int importIdentifier) {
        if (width <= 0 || height <= 0 || depth <= 0 || mipLevelCount <= 0
                || arrayLayerCount <= 0) {
            throw new IllegalArgumentException(
                    "CommandStreamImageHandleTableWriter: dimensions and counts must be"
                            + " positive");
        }
        if (imageKind == CommandStreamImageKind.THREE_DIMENSIONAL && arrayLayerCount != 1) {
            throw new IllegalArgumentException(
                    "CommandStreamImageHandleTableWriter: 3D images must have exactly one"
                            + " array layer");
        }
        boolean isPowerOfTwoSampleCount = sampleCountValue == 1 || sampleCountValue == 2
                || sampleCountValue == 4 || sampleCountValue == 8;
        if (!isPowerOfTwoSampleCount) {
            throw new IllegalArgumentException(
                    "CommandStreamImageHandleTableWriter: sample count " + sampleCountValue
                            + " must be 1, 2, 4 or 8");
        }
        if ((usageFlags & ~CommandStreamImageUsage.s_allKnownUsageFlags) != 0) {
            throw new IllegalArgumentException(
                    "CommandStreamImageHandleTableWriter: usage mask 0x"
                            + Integer.toHexString(usageFlags) + " contains unassigned bits");
        }
        m_pendingEntries.add(new PendingImageEntry(imageKind, imageFormat, width, height,
                depth, mipLevelCount, arrayLayerCount, sampleCountValue, usageFlags,
                importIdentifier));
        return m_pendingEntries.size() - 1;
    }
}
