package barrieww.core.commandstream;

import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * @note ThreadSafety: Not thread-safe. One writer assembles one module on the single
 *       bake thread; never share an instance across threads.
 * Assembles one BECS module container (ADR-0002 D2): collects section contents, then
 * writeTo() lays them out deterministically — header, directory, then the section byte
 * ranges in add order, each at the next 8-byte-aligned offset, with zero-filled padding
 * and an 8-aligned total. Deterministic bytes for identical inputs keep the artifact
 * addressable by its graphHash (§8). This is the slow-path bake writer; per-frame
 * dynamic lanes use standalone streams, not module containers (ADR-0001).
 * Boundary: authoring mistakes fail fast with context — duplicate section identities
 * are rejected by addSection; LANE_STREAM contents are shallowly checked (magic,
 * laneIndex, graphHash, totalByteSize) on add; lane index contiguity (0..N-1) is
 * checked by writeTo. Full structural validation remains the native loader's job
 * (ADR-0002 D5).
 *
 * @warning MemoryOwnership: The writer stores REFERENCES to the added content segments
 *          (no copy until writeTo); the caller must keep every added segment alive and
 *          unmodified until writeTo returns. The target segment is borrowed from the
 *          caller's Arena (§6.3); the writer never allocates or frees native memory.
 */
public final class CommandStreamModuleWriter {

    // Unaligned little-endian layouts: the module writer accepts heap or native
    // segments for contents and targets (slow path), so access must not assume the
    // 8-byte base alignment that the wire format itself guarantees for its offsets.
    private static final ValueLayout.OfShort s_unalignedLittleEndianShortLayout =
            ValueLayout.JAVA_SHORT_UNALIGNED.withOrder(ByteOrder.LITTLE_ENDIAN);
    private static final ValueLayout.OfInt s_unalignedLittleEndianIntegerLayout =
            ValueLayout.JAVA_INT_UNALIGNED.withOrder(ByteOrder.LITTLE_ENDIAN);
    private static final ValueLayout.OfLong s_unalignedLittleEndianLongLayout =
            ValueLayout.JAVA_LONG_UNALIGNED.withOrder(ByteOrder.LITTLE_ENDIAN);

    /** One collected section: identity plus the borrowed content bytes. */
    private record PendingSection(
            CommandStreamModuleSectionType sectionType,
            int sectionIndex,
            MemorySegment contentSegment) {
    }

    private final long m_graphHash;
    private final List<PendingSection> m_pendingSections = new ArrayList<>();
    private final Set<Long> m_usedSectionIdentities = new HashSet<>();

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Creates an empty module writer bound to one graph artifact hash.
     *
     * @param long graphHash The §8 artifact hash; every embedded lane stream must carry
     *        the same hash
     */
    public CommandStreamModuleWriter(long graphHash) {
        this.m_graphHash = graphHash;
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Registers one section for the next writeTo(). Sections are laid out in add order.
     * LANE_STREAM contents are shallowly verified immediately (fail fast): magic bytes,
     * laneIndex equal to sectionIndex, graphHash equal to the module's, and header
     * totalByteSize equal to the content size.
     *
     * @param CommandStreamModuleSectionType sectionType The section's type
     * @param int sectionIndex The instance index (lane index for LANE_STREAM sections;
     *        0 for singleton sections)
     * @param MemorySegment sectionContent The section bytes (heap or native segment)
     * @throws IllegalArgumentException When (sectionType, sectionIndex) was already
     *         added, when sectionIndex is negative, or when a LANE_STREAM content fails
     *         the shallow header check
     * @warning MemoryOwnership: sectionContent is borrowed by reference; see the class
     *          note for the required lifetime.
     */
    public void addSection(CommandStreamModuleSectionType sectionType, int sectionIndex,
                           MemorySegment sectionContent) {
        if (sectionIndex < 0) {
            throw new IllegalArgumentException(
                    "CommandStreamModuleWriter: negative sectionIndex " + sectionIndex
                            + " for " + sectionType);
        }
        long sectionIdentity =
                ((long) sectionType.rawSectionTypeValue() << 32) | (long) sectionIndex;
        if (!m_usedSectionIdentities.add(sectionIdentity)) {
            throw new IllegalArgumentException(
                    "CommandStreamModuleWriter: duplicate section identity (" + sectionType
                            + ", index " + sectionIndex + ")");
        }
        if (sectionType == CommandStreamModuleSectionType.LANE_STREAM) {
            verifyLaneStreamContent(sectionIndex, sectionContent);
        }
        m_pendingSections.add(new PendingSection(sectionType, sectionIndex, sectionContent));
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * The exact byte size the next writeTo() will emit (header, directory, aligned
     * section ranges and trailing padding), so the caller can allocate the target
     * segment precisely.
     *
     * @return long The total module byte size; always a multiple of 8
     */
    public long requiredByteSize() {
        long layoutCursor = directoryEndOffset();
        for (PendingSection pendingSection : m_pendingSections) {
            layoutCursor = alignUpToCommandAlignment(
                    layoutCursor + pendingSection.contentSegment().byteSize());
        }
        return layoutCursor;
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Emits the complete module into targetSegment: zero-fills the emitted range, then
     * writes header, directory and section contents. The writer keeps no emission
     * state, so writeTo may be called again (for example after adding more sections).
     *
     * @param MemorySegment targetSegment Destination; must be at least
     *        requiredByteSize() bytes
     * @return long The emitted byte size (equal to requiredByteSize())
     * @throws IllegalArgumentException When targetSegment is smaller than
     *         requiredByteSize()
     * @throws IllegalStateException When LANE_STREAM section indices are not exactly
     *         0..N-1
     * @warning MemoryOwnership: targetSegment is borrowed; the caller's Arena owns it
     *          and must keep it alive until the bytes were handed to the native side.
     */
    public long writeTo(MemorySegment targetSegment) {
        verifyLaneIndexContiguity();

        long totalByteSize = requiredByteSize();
        if (targetSegment.byteSize() < totalByteSize) {
            throw new IllegalArgumentException(
                    "CommandStreamModuleWriter: target segment holds "
                            + targetSegment.byteSize() + " bytes but the module needs "
                            + totalByteSize);
        }

        // Zero-fill first so inter-section padding is deterministic regardless of the
        // target segment's prior contents.
        targetSegment.asSlice(0, totalByteSize).fill((byte) 0);

        int laneStreamCount = 0;
        for (PendingSection pendingSection : m_pendingSections) {
            if (pendingSection.sectionType() == CommandStreamModuleSectionType.LANE_STREAM) {
                ++laneStreamCount;
            }
        }

        // Module header (ADR-0002 D2 layout, mirrored by CommandStreamModuleHeader.hpp).
        for (int magicByteIndex = 0; magicByteIndex < 4; ++magicByteIndex) {
            targetSegment.set(ValueLayout.JAVA_BYTE, magicByteIndex,
                    CommandStreamModuleFormat.s_expectedMagicBytes[magicByteIndex]);
        }
        targetSegment.set(s_unalignedLittleEndianShortLayout, 4,
                CommandStreamFormat.s_currentVersionMajor);
        targetSegment.set(s_unalignedLittleEndianShortLayout, 6,
                CommandStreamFormat.s_currentVersionMinor);
        targetSegment.set(s_unalignedLittleEndianIntegerLayout, 8, m_pendingSections.size());
        targetSegment.set(s_unalignedLittleEndianIntegerLayout, 12, laneStreamCount);
        targetSegment.set(s_unalignedLittleEndianLongLayout, 16, totalByteSize);
        targetSegment.set(s_unalignedLittleEndianLongLayout, 24, m_graphHash);

        // Directory entries and section contents, both in add order.
        long layoutCursor = directoryEndOffset();
        for (int sectionPosition = 0; sectionPosition < m_pendingSections.size();
                ++sectionPosition) {
            PendingSection pendingSection = m_pendingSections.get(sectionPosition);
            long sectionByteSize = pendingSection.contentSegment().byteSize();
            long entryOffset = CommandStreamModuleFormat.s_moduleHeaderByteSize
                    + (long) sectionPosition * CommandStreamModuleFormat.s_sectionEntryByteSize;

            targetSegment.set(s_unalignedLittleEndianShortLayout, entryOffset,
                    (short) pendingSection.sectionType().rawSectionTypeValue());
            targetSegment.set(s_unalignedLittleEndianShortLayout, entryOffset + 2, (short) 0);
            targetSegment.set(s_unalignedLittleEndianIntegerLayout, entryOffset + 4,
                    pendingSection.sectionIndex());
            targetSegment.set(s_unalignedLittleEndianLongLayout, entryOffset + 8, layoutCursor);
            targetSegment.set(s_unalignedLittleEndianLongLayout, entryOffset + 16,
                    sectionByteSize);

            MemorySegment.copy(pendingSection.contentSegment(), 0, targetSegment,
                    layoutCursor, sectionByteSize);
            layoutCursor = alignUpToCommandAlignment(layoutCursor + sectionByteSize);
        }
        return totalByteSize;
    }

    /** The first byte after the directory: header plus one entry per pending section. */
    private long directoryEndOffset() {
        return CommandStreamModuleFormat.s_moduleHeaderByteSize
                + (long) m_pendingSections.size()
                        * CommandStreamModuleFormat.s_sectionEntryByteSize;
    }

    /** Rounds byteOffset up to the next multiple of the 8-byte command alignment. */
    private static long alignUpToCommandAlignment(long byteOffset) {
        long alignment = CommandStreamFormat.s_commandAlignment;
        return (byteOffset + alignment - 1) & -alignment;
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Shallow lane-stream header check on add (fail fast; full validation stays native,
     * ADR-0002 D5): magic, laneIndex == sectionIndex, graphHash == module graphHash,
     * header totalByteSize == content size.
     *
     * @param int sectionIndex The lane index this section claims
     * @param MemorySegment laneStreamContent The candidate lane stream bytes
     * @throws IllegalArgumentException When any of the four checks fails
     */
    private void verifyLaneStreamContent(int sectionIndex, MemorySegment laneStreamContent) {
        if (laneStreamContent.byteSize() < CommandStreamFormat.s_streamHeaderByteSize) {
            throw new IllegalArgumentException(
                    "CommandStreamModuleWriter: lane " + sectionIndex + " content holds "
                            + laneStreamContent.byteSize()
                            + " bytes, smaller than the stream header");
        }
        for (int magicByteIndex = 0; magicByteIndex < 4; ++magicByteIndex) {
            if (laneStreamContent.get(ValueLayout.JAVA_BYTE, magicByteIndex)
                    != CommandStreamFormat.s_expectedMagicBytes[magicByteIndex]) {
                throw new IllegalArgumentException(
                        "CommandStreamModuleWriter: lane " + sectionIndex
                                + " content does not start with the BECS magic");
            }
        }
        int contentLaneIndex =
                laneStreamContent.get(s_unalignedLittleEndianIntegerLayout, 8);
        if (contentLaneIndex != sectionIndex) {
            throw new IllegalArgumentException(
                    "CommandStreamModuleWriter: section index " + sectionIndex
                            + " embeds a stream recorded for lane " + contentLaneIndex);
        }
        long contentTotalByteSize =
                laneStreamContent.get(s_unalignedLittleEndianLongLayout, 16);
        if (contentTotalByteSize != laneStreamContent.byteSize()) {
            throw new IllegalArgumentException(
                    "CommandStreamModuleWriter: lane " + sectionIndex + " header claims "
                            + contentTotalByteSize + " bytes but the content holds "
                            + laneStreamContent.byteSize());
        }
        long contentGraphHash =
                laneStreamContent.get(s_unalignedLittleEndianLongLayout, 24);
        if (contentGraphHash != m_graphHash) {
            throw new IllegalArgumentException(
                    "CommandStreamModuleWriter: lane " + sectionIndex + " carries graphHash 0x"
                            + Long.toHexString(contentGraphHash) + " but the module uses 0x"
                            + Long.toHexString(m_graphHash));
        }
    }

    /**
     * @note ThreadSafety: Not thread-safe (see class note).
     * Verifies LANE_STREAM section indices form exactly 0..N-1 (the native validator's
     * LaneIndexMismatch rule, enforced early on the authoring side).
     *
     * @throws IllegalStateException When the lane indices are not contiguous from zero
     */
    private void verifyLaneIndexContiguity() {
        List<Integer> laneIndices = new ArrayList<>();
        for (PendingSection pendingSection : m_pendingSections) {
            if (pendingSection.sectionType() == CommandStreamModuleSectionType.LANE_STREAM) {
                laneIndices.add(pendingSection.sectionIndex());
            }
        }
        laneIndices.sort(null);
        for (int lanePosition = 0; lanePosition < laneIndices.size(); ++lanePosition) {
            if (laneIndices.get(lanePosition) != lanePosition) {
                throw new IllegalStateException(
                        "CommandStreamModuleWriter: lane stream indices must be exactly 0.."
                                + (laneIndices.size() - 1) + " but got " + laneIndices);
            }
        }
    }
}
