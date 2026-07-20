package barrieww.core.commandstream;

import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;

/**
 * @note ThreadSafety: Not thread-safe. One writer assembles one table on the single
 *       bake thread; never share an instance across threads.
 * Writes one BarrierBatchTable section (schema v0.1): an 8-byte header (batchCount,
 * zero reserved flags), a 16-byte directory record per batch, then each batch's
 * barrier records (global records first, buffer records after) packed in add order.
 * The returned batch slots are what ExecuteBarrierBatch commands reference. Masks use
 * synchronization2 semantics (CommandStreamPipelineStage / CommandStreamMemoryAccess);
 * empty stage masks fail fast here, mirroring the native validator's ZeroStageMask
 * rule (authoring errors shifted left).
 *
 * @warning MemoryOwnership: writeTo() borrows the caller's target segment (Arena-owned,
 *          §6.3); the writer never allocates or frees native memory.
 */
public final class CommandStreamBarrierBatchTableWriter {

    /** byteCount sentinel meaning "from byteOffset to the end of the buffer". */
    public static final long s_wholeByteCount = 0xFFFFFFFFFFFFFFFFL;

    private static final ValueLayout.OfInt s_unalignedLittleEndianIntegerLayout =
            ValueLayout.JAVA_INT_UNALIGNED.withOrder(ByteOrder.LITTLE_ENDIAN);
    private static final ValueLayout.OfLong s_unalignedLittleEndianLongLayout =
            ValueLayout.JAVA_LONG_UNALIGNED.withOrder(ByteOrder.LITTLE_ENDIAN);

    /**
     * @note ThreadSafety: Immutable record; thread-safe.
     * One global memory barrier (synchronization2 masks).
     */
    public record GlobalBarrierDescription(
            long sourceStageMask,
            long sourceAccessMask,
            long destinationStageMask,
            long destinationAccessMask) {
    }

    /**
     * @note ThreadSafety: Immutable record; thread-safe.
     * One buffer memory barrier: synchronization2 masks plus the buffer slot and byte
     * range (byteCount of s_wholeByteCount mirrors Vulkan's VK_WHOLE_SIZE).
     */
    public record BufferBarrierDescription(
            long sourceStageMask,
            long sourceAccessMask,
            long destinationStageMask,
            long destinationAccessMask,
            int bufferSlot,
            long byteOffset,
            long byteCount) {
    }

    /** One collected batch: its barrier descriptions in emission order. */
    private record PendingBatch(
            List<GlobalBarrierDescription> globalBarriers,
            List<BufferBarrierDescription> bufferBarriers) {
    }

    private final List<PendingBatch> m_pendingBatches = new ArrayList<>();

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Registers one barrier batch and returns its slot number.
     *
     * @param List<GlobalBarrierDescription> globalBarriers The batch's global barriers
     * @param List<BufferBarrierDescription> bufferBarriers The batch's buffer barriers
     * @return int The batch slot ExecuteBarrierBatch commands reference
     * @throws IllegalArgumentException When any barrier has an empty source or
     *         destination stage mask, or a buffer barrier has a negative slot
     */
    public int addBatch(List<GlobalBarrierDescription> globalBarriers,
                        List<BufferBarrierDescription> bufferBarriers) {
        for (GlobalBarrierDescription globalBarrier : globalBarriers) {
            if (globalBarrier.sourceStageMask() == 0 || globalBarrier.destinationStageMask() == 0) {
                throw new IllegalArgumentException(
                        "CommandStreamBarrierBatchTableWriter: global barrier with an empty"
                                + " stage mask");
            }
        }
        for (BufferBarrierDescription bufferBarrier : bufferBarriers) {
            if (bufferBarrier.sourceStageMask() == 0 || bufferBarrier.destinationStageMask() == 0) {
                throw new IllegalArgumentException(
                        "CommandStreamBarrierBatchTableWriter: buffer barrier with an empty"
                                + " stage mask");
            }
            if (bufferBarrier.bufferSlot() < 0) {
                throw new IllegalArgumentException(
                        "CommandStreamBarrierBatchTableWriter: negative bufferSlot "
                                + bufferBarrier.bufferSlot());
            }
        }
        m_pendingBatches.add(new PendingBatch(List.copyOf(globalBarriers),
                List.copyOf(bufferBarriers)));
        return m_pendingBatches.size() - 1;
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * The exact byte size the next writeTo() will emit.
     *
     * @return long Header, directory and packed barrier records; always a multiple of 8
     */
    public long requiredByteSize() {
        long totalByteSize = 8L + 16L * m_pendingBatches.size();
        for (PendingBatch pendingBatch : m_pendingBatches) {
            totalByteSize += 32L * pendingBatch.globalBarriers().size()
                    + 56L * pendingBatch.bufferBarriers().size();
        }
        return totalByteSize;
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
                    "CommandStreamBarrierBatchTableWriter: target segment holds "
                            + targetSegment.byteSize() + " bytes but the table needs "
                            + totalByteSize);
        }

        targetSegment.set(s_unalignedLittleEndianIntegerLayout, 0, m_pendingBatches.size());
        targetSegment.set(s_unalignedLittleEndianIntegerLayout, 4, 0);

        long directoryOffset = 8;
        long recordsOffset = 8L + 16L * m_pendingBatches.size();
        for (PendingBatch pendingBatch : m_pendingBatches) {
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, directoryOffset,
                    pendingBatch.globalBarriers().size());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, directoryOffset + 4,
                    pendingBatch.bufferBarriers().size());
            targetSegment.set(s_unalignedLittleEndianLongLayout, directoryOffset + 8,
                    recordsOffset);
            directoryOffset += 16;

            for (GlobalBarrierDescription globalBarrier : pendingBatch.globalBarriers()) {
                targetSegment.set(s_unalignedLittleEndianLongLayout, recordsOffset,
                        globalBarrier.sourceStageMask());
                targetSegment.set(s_unalignedLittleEndianLongLayout, recordsOffset + 8,
                        globalBarrier.sourceAccessMask());
                targetSegment.set(s_unalignedLittleEndianLongLayout, recordsOffset + 16,
                        globalBarrier.destinationStageMask());
                targetSegment.set(s_unalignedLittleEndianLongLayout, recordsOffset + 24,
                        globalBarrier.destinationAccessMask());
                recordsOffset += 32;
            }
            for (BufferBarrierDescription bufferBarrier : pendingBatch.bufferBarriers()) {
                targetSegment.set(s_unalignedLittleEndianLongLayout, recordsOffset,
                        bufferBarrier.sourceStageMask());
                targetSegment.set(s_unalignedLittleEndianLongLayout, recordsOffset + 8,
                        bufferBarrier.sourceAccessMask());
                targetSegment.set(s_unalignedLittleEndianLongLayout, recordsOffset + 16,
                        bufferBarrier.destinationStageMask());
                targetSegment.set(s_unalignedLittleEndianLongLayout, recordsOffset + 24,
                        bufferBarrier.destinationAccessMask());
                targetSegment.set(s_unalignedLittleEndianIntegerLayout, recordsOffset + 32,
                        bufferBarrier.bufferSlot());
                targetSegment.set(s_unalignedLittleEndianIntegerLayout, recordsOffset + 36, 0);
                targetSegment.set(s_unalignedLittleEndianLongLayout, recordsOffset + 40,
                        bufferBarrier.byteOffset());
                targetSegment.set(s_unalignedLittleEndianLongLayout, recordsOffset + 48,
                        bufferBarrier.byteCount());
                recordsOffset += 56;
            }
        }
        return totalByteSize;
    }
}
