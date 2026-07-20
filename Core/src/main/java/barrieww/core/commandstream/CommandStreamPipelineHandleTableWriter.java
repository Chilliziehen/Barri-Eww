package barrieww.core.commandstream;

import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;

/**
 * @note ThreadSafety: Not thread-safe. One writer assembles one table on the single
 *       bake thread; never share an instance across threads.
 * Writes one PipelineHandleTable section (schema v0.1, mirrored by the native
 * CommandStreamPipelineHandleTableValidator): an 8-byte header (entryCount, zero
 * reserved flags) followed by fixed 16-byte entries {pipelineKindValue u32,
 * shaderModuleSlot u32, pushConstantByteSize u32, reservedFlags u32}. The v0.1
 * pipeline layout is push constants only (resources travel as buffer device
 * addresses, PushBufferDeviceAddress); the entry point is the fixed convention
 * "main". Whether shaderModuleSlot actually exists in the shader table is checked at
 * native materialization, where both tables first meet (ADR-0002 D5 layering).
 *
 * @warning MemoryOwnership: writeTo() borrows the caller's target segment
 *          (Arena-owned, §6.3); the writer never allocates or frees native memory.
 */
public final class CommandStreamPipelineHandleTableWriter {

    private static final ValueLayout.OfInt s_unalignedLittleEndianIntegerLayout =
            ValueLayout.JAVA_INT_UNALIGNED.withOrder(ByteOrder.LITTLE_ENDIAN);

    /** Vulkan's guaranteed minimum for maxPushConstantsSize bounds the schema. */
    private static final int s_maximumPushConstantByteSize = 128;

    /** One collected compute pipeline description. */
    private record PendingPipelineEntry(int shaderModuleSlot, int pushConstantByteSize) {
    }

    private final List<PendingPipelineEntry> m_pendingEntries = new ArrayList<>();

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Registers one COMPUTE pipeline description and returns its slot number.
     *
     * @param int shaderModuleSlot Slot into the module's ShaderModuleTable; must be
     *        non-negative (existence is verified at native materialization)
     * @param int pushConstantByteSize Size of the push constant range at offset 0;
     *        must be a non-negative multiple of 4, at most 128 (0 means no range)
     * @return int The slot number streams use in BindComputePipeline
     * @throws IllegalArgumentException When any argument violates the schema rules
     */
    public int addComputePipeline(int shaderModuleSlot, int pushConstantByteSize) {
        if (shaderModuleSlot < 0) {
            throw new IllegalArgumentException(
                    "CommandStreamPipelineHandleTableWriter: negative shaderModuleSlot "
                            + shaderModuleSlot);
        }
        if (pushConstantByteSize < 0 || pushConstantByteSize % 4 != 0
                || pushConstantByteSize > s_maximumPushConstantByteSize) {
            throw new IllegalArgumentException(
                    "CommandStreamPipelineHandleTableWriter: pushConstantByteSize "
                            + pushConstantByteSize
                            + " must be a multiple of 4 in [0, 128]");
        }
        m_pendingEntries.add(new PendingPipelineEntry(shaderModuleSlot, pushConstantByteSize));
        return m_pendingEntries.size() - 1;
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * The exact byte size the next writeTo() will emit.
     *
     * @return long Header plus 16 bytes per entry; always a multiple of 8
     */
    public long requiredByteSize() {
        return 8L + 16L * m_pendingEntries.size();
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
                    "CommandStreamPipelineHandleTableWriter: target segment holds "
                            + targetSegment.byteSize() + " bytes but the table needs "
                            + totalByteSize);
        }

        targetSegment.set(s_unalignedLittleEndianIntegerLayout, 0, m_pendingEntries.size());
        targetSegment.set(s_unalignedLittleEndianIntegerLayout, 4, 0);

        long entryOffset = 8;
        for (PendingPipelineEntry pendingEntry : m_pendingEntries) {
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset,
                    CommandStreamPipelineKind.COMPUTE.rawPipelineKindValue());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset + 4,
                    pendingEntry.shaderModuleSlot());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset + 8,
                    pendingEntry.pushConstantByteSize());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset + 12, 0);
            entryOffset += 16;
        }
        return totalByteSize;
    }
}
