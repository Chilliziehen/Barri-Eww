package barrieww.core.commandstream;

import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;

/**
 * @note ThreadSafety: Not thread-safe. One writer assembles one table on the single
 *       bake thread; never share an instance across threads.
 * Writes one GraphicsPipelineTable section (schema v0.1, §9.14, mirrored by the native
 * CommandStreamGraphicsPipelineTableValidator): an 8-byte header (pipelineCount, zero
 * reserved flags) then one fixed 80-byte record per pipeline — color attachment formats
 * are inlined (up to 8; unused indices zero), so the emitted size is exactly
 * 8 + pipelineCount * 80. Authoring errors (bad push range, missing attachments,
 * inconsistent depth state) fail fast here, mirroring the native validator (errors
 * shifted left). Shader-module slot existence is the native materialization's
 * cross-table check and deliberately not verified here.
 *
 * @warning MemoryOwnership: writeTo() borrows the caller's target segment (Arena-owned,
 *          §6.3); the writer never allocates or frees native memory.
 */
public final class CommandStreamGraphicsPipelineTableWriter {

    private static final ValueLayout.OfInt s_unalignedLittleEndianIntegerLayout =
            ValueLayout.JAVA_INT_UNALIGNED.withOrder(ByteOrder.LITTLE_ENDIAN);

    private static final int s_maximumColorAttachmentCount = 8;
    private static final int s_pipelineRecordByteSize = 80;
    private static final int s_maximumPushConstantByteSize = 128;

    /**
     * @note ThreadSafety: Immutable record; thread-safe.
     * The depth-testing state of one pipeline: the depth attachment format plus the
     * test/write enables and the compare operation used while testing.
     */
    public record DepthState(
            CommandStreamImageFormat depthAttachmentFormat,
            boolean depthTestEnabled,
            boolean depthWriteEnabled,
            Optional<CommandStreamCompareOperation> depthCompareOperation) {
    }

    /** One collected pipeline: every compile-time-fixed field of its 80-byte record. */
    private record PendingPipeline(
            int vertexShaderModuleSlot,
            int fragmentShaderModuleSlot,
            int pushConstantByteSize,
            CommandStreamPrimitiveTopology topology,
            List<CommandStreamImageFormat> colorAttachmentFormats,
            Optional<DepthState> depthState,
            CommandStreamCullMode cullMode,
            CommandStreamFrontFace frontFace) {
    }

    private final List<PendingPipeline> m_pendingPipelines = new ArrayList<>();

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Registers one graphics pipeline and returns its slot number.
     *
     * @param int vertexShaderModuleSlot The vertex stage's ShaderModuleTable slot
     * @param int fragmentShaderModuleSlot The fragment stage's ShaderModuleTable slot
     * @param int pushConstantByteSize The push range visible to both stages; a multiple
     *        of 4 in [0, 128] (0 = no range)
     * @param CommandStreamPrimitiveTopology topology The input assembly topology
     * @param List<CommandStreamImageFormat> colorAttachmentFormats The color attachment
     *        formats in slot order; at most 8, each a color format
     * @param Optional<DepthState> depthState The depth attachment and testing state, or
     *        empty when the pipeline has no depth attachment
     * @param CommandStreamCullMode cullMode The rasterization cull mode
     * @param CommandStreamFrontFace frontFace The front-face winding order
     * @return int The pipeline slot BindGraphicsPipeline commands reference
     * @throws IllegalArgumentException When the pipeline violates the schema rules
     */
    public int addPipeline(int vertexShaderModuleSlot, int fragmentShaderModuleSlot,
                           int pushConstantByteSize,
                           CommandStreamPrimitiveTopology topology,
                           List<CommandStreamImageFormat> colorAttachmentFormats,
                           Optional<DepthState> depthState,
                           CommandStreamCullMode cullMode,
                           CommandStreamFrontFace frontFace) {
        if (pushConstantByteSize % 4 != 0 || pushConstantByteSize < 0
                || pushConstantByteSize > s_maximumPushConstantByteSize) {
            throw new IllegalArgumentException(
                    "CommandStreamGraphicsPipelineTableWriter: pushConstantByteSize must be"
                            + " a multiple of 4 in [0, 128] but is " + pushConstantByteSize);
        }
        if (colorAttachmentFormats.size() > s_maximumColorAttachmentCount) {
            throw new IllegalArgumentException(
                    "CommandStreamGraphicsPipelineTableWriter: at most 8 color attachments"
                            + " but " + colorAttachmentFormats.size() + " were given");
        }
        if (colorAttachmentFormats.isEmpty() && depthState.isEmpty()) {
            throw new IllegalArgumentException(
                    "CommandStreamGraphicsPipelineTableWriter: a pipeline needs at least"
                            + " one color or depth attachment");
        }
        for (CommandStreamImageFormat colorAttachmentFormat : colorAttachmentFormats) {
            if (isDepthCapableFormat(colorAttachmentFormat)) {
                throw new IllegalArgumentException(
                        "CommandStreamGraphicsPipelineTableWriter: " + colorAttachmentFormat
                                + " is not a color format");
            }
        }
        if (depthState.isPresent()) {
            DepthState presentDepthState = depthState.get();
            if (!isDepthCapableFormat(presentDepthState.depthAttachmentFormat())) {
                throw new IllegalArgumentException(
                        "CommandStreamGraphicsPipelineTableWriter: "
                                + presentDepthState.depthAttachmentFormat()
                                + " is not a depth-capable format");
            }
            if (presentDepthState.depthWriteEnabled()
                    && !presentDepthState.depthTestEnabled()) {
                throw new IllegalArgumentException(
                        "CommandStreamGraphicsPipelineTableWriter: depth writes require"
                                + " depth testing");
            }
            if (presentDepthState.depthTestEnabled()
                    != presentDepthState.depthCompareOperation().isPresent()) {
                throw new IllegalArgumentException(
                        "CommandStreamGraphicsPipelineTableWriter: the compare operation"
                                + " must be present exactly when depth testing is enabled");
            }
        }
        m_pendingPipelines.add(new PendingPipeline(vertexShaderModuleSlot,
                fragmentShaderModuleSlot, pushConstantByteSize, topology,
                List.copyOf(colorAttachmentFormats), depthState, cullMode, frontFace));
        return m_pendingPipelines.size() - 1;
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * The exact byte size the next writeTo() will emit.
     *
     * @return long Header plus one 80-byte record per pipeline; always a multiple of 8
     */
    public long requiredByteSize() {
        return 8L + (long) s_pipelineRecordByteSize * m_pendingPipelines.size();
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
                    "CommandStreamGraphicsPipelineTableWriter: target segment holds "
                            + targetSegment.byteSize() + " bytes but the table needs "
                            + totalByteSize);
        }

        targetSegment.set(s_unalignedLittleEndianIntegerLayout, 0, m_pendingPipelines.size());
        targetSegment.set(s_unalignedLittleEndianIntegerLayout, 4, 0);

        long recordOffset = 8;
        for (PendingPipeline pendingPipeline : m_pendingPipelines) {
            Optional<DepthState> depthState = pendingPipeline.depthState();
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, recordOffset,
                    pendingPipeline.vertexShaderModuleSlot());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, recordOffset + 4,
                    pendingPipeline.fragmentShaderModuleSlot());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, recordOffset + 8,
                    pendingPipeline.pushConstantByteSize());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, recordOffset + 12,
                    pendingPipeline.topology().rawTopologyValue());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, recordOffset + 16,
                    pendingPipeline.colorAttachmentFormats().size());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, recordOffset + 20,
                    depthState.map(state -> state.depthAttachmentFormat()
                            .rawImageFormatValue()).orElse(0));
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, recordOffset + 24,
                    depthState.map(state -> state.depthTestEnabled() ? 1 : 0).orElse(0));
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, recordOffset + 28,
                    depthState.map(state -> state.depthWriteEnabled() ? 1 : 0).orElse(0));
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, recordOffset + 32,
                    depthState.flatMap(DepthState::depthCompareOperation)
                            .map(CommandStreamCompareOperation::rawCompareOperationValue)
                            .orElse(0));
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, recordOffset + 36,
                    pendingPipeline.cullMode().rawCullModeValue());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, recordOffset + 40,
                    pendingPipeline.frontFace().rawFrontFaceValue());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, recordOffset + 44, 0);
            for (int formatIndex = 0; formatIndex < s_maximumColorAttachmentCount;
                    ++formatIndex) {
                int formatValue =
                        formatIndex < pendingPipeline.colorAttachmentFormats().size()
                                ? pendingPipeline.colorAttachmentFormats().get(formatIndex)
                                        .rawImageFormatValue()
                                : 0;
                targetSegment.set(s_unalignedLittleEndianIntegerLayout,
                        recordOffset + 48 + 4L * formatIndex, formatValue);
            }
            recordOffset += s_pipelineRecordByteSize;
        }
        return totalByteSize;
    }

    /**
     * @note ThreadSafety: Thread-safe (pure function).
     * Whether the format carries a depth aspect (mirrors the native format catalog).
     *
     * @param CommandStreamImageFormat imageFormat The format to classify
     * @return boolean True for D32_FLOAT and D24_UNORM_S8_UINT
     */
    private static boolean isDepthCapableFormat(CommandStreamImageFormat imageFormat) {
        return imageFormat == CommandStreamImageFormat.D32_FLOAT
                || imageFormat == CommandStreamImageFormat.D24_UNORM_S8_UINT;
    }
}
