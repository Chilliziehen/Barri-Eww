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
 * Writes one RenderingTemplateTable section (schema v0.1, mirrored by the native
 * CommandStreamRenderingTemplateTableValidator): an 8-byte header (templateCount, zero
 * reserved flags), a 40-byte directory record per template, then each template's
 * attachment records (color records first, then one depth record when present) packed
 * in add order. Attachment regions start right after the directory, which is naturally
 * 8-byte aligned (header 8 + 40 per template, records 32 bytes each). A template with
 * neither a color nor a depth attachment, a zero render area or a zero layer count
 * fails fast here, mirroring the native validator (authoring errors shifted left).
 *
 * @warning MemoryOwnership: writeTo() borrows the caller's target segment (Arena-owned,
 *          §6.3); the writer never allocates or frees native memory.
 */
public final class CommandStreamRenderingTemplateTableWriter {

    private static final ValueLayout.OfInt s_unalignedLittleEndianIntegerLayout =
            ValueLayout.JAVA_INT_UNALIGNED.withOrder(ByteOrder.LITTLE_ENDIAN);
    private static final ValueLayout.OfLong s_unalignedLittleEndianLongLayout =
            ValueLayout.JAVA_LONG_UNALIGNED.withOrder(ByteOrder.LITTLE_ENDIAN);
    private static final ValueLayout.OfFloat s_unalignedLittleEndianFloatLayout =
            ValueLayout.JAVA_FLOAT_UNALIGNED.withOrder(ByteOrder.LITTLE_ENDIAN);

    /**
     * @note ThreadSafety: Immutable record; thread-safe.
     * One rendering attachment: the source image-view slot, the layout it is in during
     * rendering, its load/store operations and a clear value. For a depth/stencil
     * attachment clearRed carries the depth clear and clearGreen's bits carry the u32
     * stencil clear (Float.intBitsToFloat).
     */
    public record AttachmentDescription(
            int imageViewSlot,
            CommandStreamImageLayout imageLayout,
            CommandStreamAttachmentLoadOp loadOp,
            CommandStreamAttachmentStoreOp storeOp,
            float clearRed,
            float clearGreen,
            float clearBlue,
            float clearAlpha) {
    }

    /** One collected template: its geometry, layers and attachment records in order. */
    private record PendingTemplate(
            List<AttachmentDescription> colorAttachments,
            Optional<AttachmentDescription> depthAttachment,
            int renderAreaOffsetX,
            int renderAreaOffsetY,
            int renderAreaWidth,
            int renderAreaHeight,
            int layerCount,
            int viewMask) {
    }

    private final List<PendingTemplate> m_pendingTemplates = new ArrayList<>();

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Registers one rendering template and returns its slot number.
     *
     * @param List<AttachmentDescription> colorAttachments The color attachments in order
     * @param Optional<AttachmentDescription> depthAttachment The depth/stencil attachment,
     *        or empty when the template has none
     * @param int renderAreaOffsetX The render area x offset
     * @param int renderAreaOffsetY The render area y offset
     * @param int renderAreaWidth The render area width; must be positive
     * @param int renderAreaHeight The render area height; must be positive
     * @param int layerCount The number of layers rendered; must be positive
     * @param int viewMask The multiview view mask (0 for single-view)
     * @return int The template slot BeginRendering commands reference
     * @throws IllegalArgumentException When the template violates the schema rules
     */
    public int addTemplate(List<AttachmentDescription> colorAttachments,
                           Optional<AttachmentDescription> depthAttachment,
                           int renderAreaOffsetX, int renderAreaOffsetY,
                           int renderAreaWidth, int renderAreaHeight, int layerCount,
                           int viewMask) {
        if (colorAttachments.isEmpty() && depthAttachment.isEmpty()) {
            throw new IllegalArgumentException(
                    "CommandStreamRenderingTemplateTableWriter: a template needs at least"
                            + " one color or depth attachment");
        }
        if (renderAreaWidth <= 0 || renderAreaHeight <= 0) {
            throw new IllegalArgumentException(
                    "CommandStreamRenderingTemplateTableWriter: render area must be positive");
        }
        if (layerCount <= 0) {
            throw new IllegalArgumentException(
                    "CommandStreamRenderingTemplateTableWriter: layerCount must be positive");
        }
        m_pendingTemplates.add(new PendingTemplate(List.copyOf(colorAttachments),
                depthAttachment, renderAreaOffsetX, renderAreaOffsetY, renderAreaWidth,
                renderAreaHeight, layerCount, viewMask));
        return m_pendingTemplates.size() - 1;
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * The exact byte size the next writeTo() will emit.
     *
     * @return long Header, directory and packed attachment records; always a multiple of 8
     */
    public long requiredByteSize() {
        long totalByteSize = 8L + 40L * m_pendingTemplates.size();
        for (PendingTemplate pendingTemplate : m_pendingTemplates) {
            long attachmentCount = pendingTemplate.colorAttachments().size()
                    + (pendingTemplate.depthAttachment().isPresent() ? 1L : 0L);
            totalByteSize += 32L * attachmentCount;
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
                    "CommandStreamRenderingTemplateTableWriter: target segment holds "
                            + targetSegment.byteSize() + " bytes but the table needs "
                            + totalByteSize);
        }

        targetSegment.set(s_unalignedLittleEndianIntegerLayout, 0, m_pendingTemplates.size());
        targetSegment.set(s_unalignedLittleEndianIntegerLayout, 4, 0);

        long directoryOffset = 8;
        long recordsOffset = 8L + 40L * m_pendingTemplates.size();
        for (PendingTemplate pendingTemplate : m_pendingTemplates) {
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, directoryOffset,
                    pendingTemplate.colorAttachments().size());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, directoryOffset + 4,
                    pendingTemplate.depthAttachment().isPresent() ? 1 : 0);
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, directoryOffset + 8,
                    pendingTemplate.renderAreaOffsetX());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, directoryOffset + 12,
                    pendingTemplate.renderAreaOffsetY());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, directoryOffset + 16,
                    pendingTemplate.renderAreaWidth());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, directoryOffset + 20,
                    pendingTemplate.renderAreaHeight());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, directoryOffset + 24,
                    pendingTemplate.layerCount());
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, directoryOffset + 28,
                    pendingTemplate.viewMask());
            targetSegment.set(s_unalignedLittleEndianLongLayout, directoryOffset + 32,
                    recordsOffset);
            directoryOffset += 40;

            for (AttachmentDescription colorAttachment : pendingTemplate.colorAttachments()) {
                recordsOffset = writeAttachmentRecord(targetSegment, recordsOffset,
                        colorAttachment);
            }
            if (pendingTemplate.depthAttachment().isPresent()) {
                recordsOffset = writeAttachmentRecord(targetSegment, recordsOffset,
                        pendingTemplate.depthAttachment().get());
            }
        }
        return totalByteSize;
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Writes one 32-byte attachment record and returns the offset just after it.
     *
     * @param MemorySegment targetSegment The destination segment
     * @param long recordOffset The byte offset to write the record at
     * @param AttachmentDescription attachment The attachment to emit
     * @return long The offset immediately after the written record
     */
    private long writeAttachmentRecord(MemorySegment targetSegment, long recordOffset,
                                       AttachmentDescription attachment) {
        targetSegment.set(s_unalignedLittleEndianIntegerLayout, recordOffset,
                attachment.imageViewSlot());
        targetSegment.set(s_unalignedLittleEndianIntegerLayout, recordOffset + 4,
                attachment.imageLayout().rawImageLayoutValue());
        targetSegment.set(s_unalignedLittleEndianIntegerLayout, recordOffset + 8,
                attachment.loadOp().rawLoadOpValue());
        targetSegment.set(s_unalignedLittleEndianIntegerLayout, recordOffset + 12,
                attachment.storeOp().rawStoreOpValue());
        targetSegment.set(s_unalignedLittleEndianFloatLayout, recordOffset + 16,
                attachment.clearRed());
        targetSegment.set(s_unalignedLittleEndianFloatLayout, recordOffset + 20,
                attachment.clearGreen());
        targetSegment.set(s_unalignedLittleEndianFloatLayout, recordOffset + 24,
                attachment.clearBlue());
        targetSegment.set(s_unalignedLittleEndianFloatLayout, recordOffset + 28,
                attachment.clearAlpha());
        return recordOffset + 32;
    }
}
