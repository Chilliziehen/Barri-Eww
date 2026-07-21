package barrieww.core.commandstream;

import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.nio.ByteOrder;

/**
 * @note ThreadSafety: Not thread-safe. One writer owns one lane's segment during
 *       recording (the THREADED_RECORDING model gives every lane its own thread and its
 *       own segment, ADR-0001); never share an instance across threads.
 * Writes one BECS lane stream (ADR-0002 D3) into a preallocated MemorySegment using
 * explicit little-endian layouts (ADR-0002 D1). This is the slow-path reference writer
 * used by the bake pipeline and by tests; the JIT-generated recordLaneN() bodies will
 * later emit equivalent constant-offset writes directly.
 * Boundary: appends fail with IndexOutOfBoundsException when the segment is too small,
 * and IllegalStateException after finish(); finish() may only be called once.
 *
 * @warning MemoryOwnership: The writer BORROWS targetSegment; the caller's Arena owns
 *          it and must keep it alive until the written bytes were handed to the native
 *          side (§6.3). The writer never allocates or frees native memory.
 */
public final class CommandStreamWriter {

    private static final ValueLayout.OfShort s_littleEndianShortLayout =
            ValueLayout.JAVA_SHORT.withOrder(ByteOrder.LITTLE_ENDIAN);
    private static final ValueLayout.OfInt s_littleEndianIntegerLayout =
            ValueLayout.JAVA_INT.withOrder(ByteOrder.LITTLE_ENDIAN);
    private static final ValueLayout.OfLong s_littleEndianLongLayout =
            ValueLayout.JAVA_LONG.withOrder(ByteOrder.LITTLE_ENDIAN);
    private static final ValueLayout.OfFloat s_littleEndianFloatLayout =
            ValueLayout.JAVA_FLOAT.withOrder(ByteOrder.LITTLE_ENDIAN);

    private final MemorySegment m_targetSegment;
    private final int m_laneIndex;
    private final long m_graphHash;
    private long m_currentByteOffset;
    private int m_commandCount;
    private boolean m_isFinished;

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Creates a writer positioned after the 32-byte header, which is written by
     * finish() once the command count and total size are known.
     *
     * @param MemorySegment targetSegment Preallocated destination; must be 8-byte
     *        aligned at its base and large enough for the stream being recorded
     * @param int laneIndex The lane this stream is compiled for
     * @param long graphHash The §8 artifact hash binding the stream to its graph
     * @warning MemoryOwnership: targetSegment is borrowed; see the class note.
     */
    public CommandStreamWriter(MemorySegment targetSegment, int laneIndex, long graphHash) {
        this.m_targetSegment = targetSegment;
        this.m_laneIndex = laneIndex;
        this.m_graphHash = graphHash;
        this.m_currentByteOffset = CommandStreamFormat.s_streamHeaderByteSize;
        this.m_commandCount = 0;
        this.m_isFinished = false;
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Appends a Draw command (opcode 0x0010, ADR-0002 appendix A).
     *
     * @param int vertexCount Number of vertices to draw
     * @param int instanceCount Number of instances to draw
     * @param int firstVertex Offset of the first vertex
     * @param int firstInstance Offset of the first instance
     */
    public void appendDraw(int vertexCount, int instanceCount, int firstVertex, int firstInstance) {
        writeCommandHeader(CommandStreamOpcode.DRAW, 24);
        m_targetSegment.set(s_littleEndianIntegerLayout, m_currentByteOffset + 8, vertexCount);
        m_targetSegment.set(s_littleEndianIntegerLayout, m_currentByteOffset + 12, instanceCount);
        m_targetSegment.set(s_littleEndianIntegerLayout, m_currentByteOffset + 16, firstVertex);
        m_targetSegment.set(s_littleEndianIntegerLayout, m_currentByteOffset + 20, firstInstance);
        m_currentByteOffset += 24;
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Appends a Dispatch command (opcode 0x0020, ADR-0002 appendix A). The trailing
     * 4 padding bytes are written as zero explicitly so the output does not depend on
     * the allocator's zero-initialization.
     *
     * @param int groupCountX Workgroup count along X
     * @param int groupCountY Workgroup count along Y
     * @param int groupCountZ Workgroup count along Z
     */
    public void appendDispatch(int groupCountX, int groupCountY, int groupCountZ) {
        writeCommandHeader(CommandStreamOpcode.DISPATCH, 24);
        m_targetSegment.set(s_littleEndianIntegerLayout, m_currentByteOffset + 8, groupCountX);
        m_targetSegment.set(s_littleEndianIntegerLayout, m_currentByteOffset + 12, groupCountY);
        m_targetSegment.set(s_littleEndianIntegerLayout, m_currentByteOffset + 16, groupCountZ);
        m_targetSegment.set(s_littleEndianIntegerLayout, m_currentByteOffset + 20, 0);
        m_currentByteOffset += 24;
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Appends a BindComputePipeline command (opcode 0x0002). Pinned payload layout
     * (mirrored by the native CommandBufferRecorder): +0 pipelineSlot u32,
     * +4 reserved zero padding; command byteSize 16.
     *
     * @param int pipelineSlot Pipeline table slot to bind at the compute bind point
     */
    public void appendBindComputePipeline(int pipelineSlot) {
        writeCommandHeader(CommandStreamOpcode.BIND_COMPUTE_PIPELINE, 16);
        m_targetSegment.set(s_littleEndianIntegerLayout, m_currentByteOffset + 8,
                pipelineSlot);
        m_targetSegment.set(s_littleEndianIntegerLayout, m_currentByteOffset + 12, 0);
        m_currentByteOffset += 16;
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Appends a PushBufferDeviceAddress command (opcode 0x0008): the stream carries the
     * buffer SLOT and the native recorder resolves it to the actual device address at
     * load time (addresses only exist after materialization, so this keeps the bake
     * artifact pure data). Pinned payload layout: +0 bufferSlot u32,
     * +4 pushConstantByteOffset u32; command byteSize 16.
     *
     * @param int bufferSlot Buffer table slot whose device address to push
     * @param int pushConstantByteOffset Byte offset within the bound pipeline's push
     *        constant range receiving the 8-byte address
     */
    public void appendPushBufferDeviceAddress(int bufferSlot, int pushConstantByteOffset) {
        writeCommandHeader(CommandStreamOpcode.PUSH_BUFFER_DEVICE_ADDRESS, 16);
        m_targetSegment.set(s_littleEndianIntegerLayout, m_currentByteOffset + 8, bufferSlot);
        m_targetSegment.set(s_littleEndianIntegerLayout, m_currentByteOffset + 12,
                pushConstantByteOffset);
        m_currentByteOffset += 16;
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Appends a DispatchIndirect command (opcode 0x0021): the GPU reads the three
     * workgroup counts from the referenced buffer at execution time (the GPU-driven
     * shape — the CPU prerecords, the GPU decides the workload). Pinned payload
     * layout (mirrored by the native CommandBufferRecorder): +0 bufferSlot u32,
     * +4 reserved zero padding, +8 bufferOffset u64; command byteSize 24.
     *
     * @param int bufferSlot Buffer table slot holding the indirect arguments; the
     *        entry must carry the Indirect usage bit
     * @param long bufferOffset Byte offset of the arguments within the buffer; must be
     *        a multiple of 4
     */
    public void appendDispatchIndirect(int bufferSlot, long bufferOffset) {
        writeCommandHeader(CommandStreamOpcode.DISPATCH_INDIRECT, 24);
        m_targetSegment.set(s_littleEndianIntegerLayout, m_currentByteOffset + 8, bufferSlot);
        m_targetSegment.set(s_littleEndianIntegerLayout, m_currentByteOffset + 12, 0);
        m_targetSegment.set(s_littleEndianLongLayout, m_currentByteOffset + 16, bufferOffset);
        m_currentByteOffset += 24;
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Appends a CopyBuffer command (opcode 0x0040). Pinned payload layout (mirrored by
     * the native CommandBufferRecorder): +0 sourceBufferSlot u32,
     * +4 destinationBufferSlot u32, +8 sourceByteOffset u64,
     * +16 destinationByteOffset u64, +24 copyByteCount u64; command byteSize 40.
     *
     * @param int sourceBufferSlot Buffer table slot to copy from
     * @param int destinationBufferSlot Buffer table slot to copy into
     * @param long sourceByteOffset Byte offset within the source buffer
     * @param long destinationByteOffset Byte offset within the destination buffer
     * @param long copyByteCount Number of bytes to copy
     */
    public void appendCopyBuffer(int sourceBufferSlot, int destinationBufferSlot,
                                 long sourceByteOffset, long destinationByteOffset,
                                 long copyByteCount) {
        writeCommandHeader(CommandStreamOpcode.COPY_BUFFER, 40);
        m_targetSegment.set(s_littleEndianIntegerLayout, m_currentByteOffset + 8,
                sourceBufferSlot);
        m_targetSegment.set(s_littleEndianIntegerLayout, m_currentByteOffset + 12,
                destinationBufferSlot);
        m_targetSegment.set(s_littleEndianLongLayout, m_currentByteOffset + 16,
                sourceByteOffset);
        m_targetSegment.set(s_littleEndianLongLayout, m_currentByteOffset + 24,
                destinationByteOffset);
        m_targetSegment.set(s_littleEndianLongLayout, m_currentByteOffset + 32,
                copyByteCount);
        m_currentByteOffset += 40;
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Appends an ExecuteBarrierBatch command (opcode 0x0032). Pinned payload layout:
     * +0 barrierBatchSlot u32, +4 zero padding u32; command byteSize 16.
     *
     * @param int barrierBatchSlot The BarrierBatchTable slot to execute
     */
    public void appendExecuteBarrierBatch(int barrierBatchSlot) {
        writeCommandHeader(CommandStreamOpcode.EXECUTE_BARRIER_BATCH, 16);
        m_targetSegment.set(s_littleEndianIntegerLayout, m_currentByteOffset + 8,
                barrierBatchSlot);
        m_targetSegment.set(s_littleEndianIntegerLayout, m_currentByteOffset + 12, 0);
        m_currentByteOffset += 16;
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Appends a ClearColorImage command (opcode 0x0043). Pinned payload layout
     * (mirrored by the native CommandBufferRecorder): +0 imageSlot u32,
     * +4 imageLayoutValue u32, +8..+20 RGBA clear color f32, +24 aspectMaskValue u32,
     * +28 baseMipLevel u32, +32 mipLevelCount u32, +36 baseArrayLayer u32,
     * +40 arrayLayerCount u32, +44 reserved zero; command byteSize 56.
     *
     * @param int imageSlot Image table slot to clear
     * @param CommandStreamImageLayout imageLayout The layout the image is in when the
     *        clear executes (General or TransferDestination)
     * @param float clearRed Red clear component
     * @param float clearGreen Green clear component
     * @param float clearBlue Blue clear component
     * @param float clearAlpha Alpha clear component
     * @param int aspectMaskValue OR-mask of CommandStreamImageAspect bits
     * @param int baseMipLevel First mip level of the cleared range
     * @param int mipLevelCount Mip level count (CommandStreamBarrierBatchTableWriter
     *        .s_remainingCount for all remaining)
     * @param int baseArrayLayer First array layer of the cleared range
     * @param int arrayLayerCount Array layer count (s_remainingCount for all remaining)
     */
    public void appendClearColorImage(int imageSlot, CommandStreamImageLayout imageLayout,
                                      float clearRed, float clearGreen, float clearBlue,
                                      float clearAlpha, int aspectMaskValue,
                                      int baseMipLevel, int mipLevelCount,
                                      int baseArrayLayer, int arrayLayerCount) {
        writeCommandHeader(CommandStreamOpcode.CLEAR_COLOR_IMAGE, 56);
        long payloadOffset = m_currentByteOffset + 8;
        m_targetSegment.set(s_littleEndianIntegerLayout, payloadOffset, imageSlot);
        m_targetSegment.set(s_littleEndianIntegerLayout, payloadOffset + 4,
                imageLayout.rawImageLayoutValue());
        m_targetSegment.set(s_littleEndianFloatLayout, payloadOffset + 8, clearRed);
        m_targetSegment.set(s_littleEndianFloatLayout, payloadOffset + 12, clearGreen);
        m_targetSegment.set(s_littleEndianFloatLayout, payloadOffset + 16, clearBlue);
        m_targetSegment.set(s_littleEndianFloatLayout, payloadOffset + 20, clearAlpha);
        m_targetSegment.set(s_littleEndianIntegerLayout, payloadOffset + 24, aspectMaskValue);
        m_targetSegment.set(s_littleEndianIntegerLayout, payloadOffset + 28, baseMipLevel);
        m_targetSegment.set(s_littleEndianIntegerLayout, payloadOffset + 32, mipLevelCount);
        m_targetSegment.set(s_littleEndianIntegerLayout, payloadOffset + 36, baseArrayLayer);
        m_targetSegment.set(s_littleEndianIntegerLayout, payloadOffset + 40, arrayLayerCount);
        m_targetSegment.set(s_littleEndianIntegerLayout, payloadOffset + 44, 0);
        m_currentByteOffset += 56;
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Appends a CopyImageToBuffer command (opcode 0x0047, tightly packed region at
     * image origin). Pinned payload layout (mirrored by the native
     * CommandBufferRecorder): +0 imageSlot u32, +4 bufferSlot u32,
     * +8 imageLayoutValue u32, +12 aspectMaskValue u32, +16 mipLevel u32,
     * +20 baseArrayLayer u32, +24 arrayLayerCount u32, +28 reserved,
     * +32 bufferByteOffset u64, +40 copyWidth u32, +44 copyHeight u32,
     * +48 copyDepth u32, +52 reserved zero; command byteSize 64.
     *
     * @param int imageSlot Image table slot to copy from
     * @param int bufferSlot Buffer table slot to copy into
     * @param CommandStreamImageLayout imageLayout The layout the image is in when the
     *        copy executes (General or TransferSource)
     * @param int aspectMaskValue OR-mask of CommandStreamImageAspect bits
     * @param int mipLevel The mip level to copy
     * @param int baseArrayLayer First array layer to copy
     * @param int arrayLayerCount Array layer count; must be concrete (no sentinel)
     * @param long bufferByteOffset Byte offset within the destination buffer
     * @param int copyWidth Copied extent width in texels
     * @param int copyHeight Copied extent height in texels
     * @param int copyDepth Copied extent depth in texels
     */
    public void appendCopyImageToBuffer(int imageSlot, int bufferSlot,
                                        CommandStreamImageLayout imageLayout,
                                        int aspectMaskValue, int mipLevel,
                                        int baseArrayLayer, int arrayLayerCount,
                                        long bufferByteOffset, int copyWidth,
                                        int copyHeight, int copyDepth) {
        writeCommandHeader(CommandStreamOpcode.COPY_IMAGE_TO_BUFFER, 64);
        long payloadOffset = m_currentByteOffset + 8;
        m_targetSegment.set(s_littleEndianIntegerLayout, payloadOffset, imageSlot);
        m_targetSegment.set(s_littleEndianIntegerLayout, payloadOffset + 4, bufferSlot);
        m_targetSegment.set(s_littleEndianIntegerLayout, payloadOffset + 8,
                imageLayout.rawImageLayoutValue());
        m_targetSegment.set(s_littleEndianIntegerLayout, payloadOffset + 12, aspectMaskValue);
        m_targetSegment.set(s_littleEndianIntegerLayout, payloadOffset + 16, mipLevel);
        m_targetSegment.set(s_littleEndianIntegerLayout, payloadOffset + 20, baseArrayLayer);
        m_targetSegment.set(s_littleEndianIntegerLayout, payloadOffset + 24, arrayLayerCount);
        m_targetSegment.set(s_littleEndianIntegerLayout, payloadOffset + 28, 0);
        m_targetSegment.set(s_littleEndianLongLayout, payloadOffset + 32, bufferByteOffset);
        m_targetSegment.set(s_littleEndianIntegerLayout, payloadOffset + 40, copyWidth);
        m_targetSegment.set(s_littleEndianIntegerLayout, payloadOffset + 44, copyHeight);
        m_targetSegment.set(s_littleEndianIntegerLayout, payloadOffset + 48, copyDepth);
        m_targetSegment.set(s_littleEndianIntegerLayout, payloadOffset + 52, 0);
        m_currentByteOffset += 64;
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Writes the 32-byte stream header (magic, version, laneIndex, commandCount,
     * totalByteSize, graphHash) and seals the writer.
     *
     * @return long The total byte size of the finished stream (header included)
     * @throws IllegalStateException When finish() was already called
     */
    public long finish() {
        if (m_isFinished) {
            throw new IllegalStateException("CommandStreamWriter.finish() called twice");
        }
        m_isFinished = true;

        for (int magicByteIndex = 0; magicByteIndex < 4; ++magicByteIndex) {
            m_targetSegment.set(ValueLayout.JAVA_BYTE, magicByteIndex,
                    CommandStreamFormat.s_expectedMagicBytes[magicByteIndex]);
        }
        m_targetSegment.set(s_littleEndianShortLayout, 4, CommandStreamFormat.s_currentVersionMajor);
        m_targetSegment.set(s_littleEndianShortLayout, 6, CommandStreamFormat.s_currentVersionMinor);
        m_targetSegment.set(s_littleEndianIntegerLayout, 8, m_laneIndex);
        m_targetSegment.set(s_littleEndianIntegerLayout, 12, m_commandCount);
        m_targetSegment.set(s_littleEndianLongLayout, 16, m_currentByteOffset);
        m_targetSegment.set(s_littleEndianLongLayout, 24, m_graphHash);
        return m_currentByteOffset;
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Writes one 8-byte command header (opcode, zero reserved flags, byteSize) at the
     * current offset and counts the command; the caller then writes the payload.
     *
     * @param CommandStreamOpcode opcode The command's opcode
     * @param int commandByteSize Total command size including this header; must be a
     *        multiple of 8 (ADR-0002 D1)
     * @throws IllegalStateException When the writer is already finished
     */
    private void writeCommandHeader(CommandStreamOpcode opcode, int commandByteSize) {
        if (m_isFinished) {
            throw new IllegalStateException(
                    "CommandStreamWriter: append after finish() on lane " + m_laneIndex);
        }
        m_targetSegment.set(s_littleEndianShortLayout, m_currentByteOffset,
                (short) opcode.rawOpcodeValue());
        m_targetSegment.set(s_littleEndianShortLayout, m_currentByteOffset + 2, (short) 0);
        m_targetSegment.set(s_littleEndianIntegerLayout, m_currentByteOffset + 4, commandByteSize);
        ++m_commandCount;
    }
}
