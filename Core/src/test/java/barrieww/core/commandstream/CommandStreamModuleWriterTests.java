package barrieww.core.commandstream;

import static org.junit.jupiter.api.Assertions.assertArrayEquals;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import org.junit.jupiter.api.Test;

/**
 * @note ThreadSafety: JUnit creates a fresh instance per test method; no shared state.
 * Tests of CommandStreamModuleWriter: byte-exact agreement with the committed
 * cross-language module golden (which the C++ module validator verifies from its
 * side), deterministic layout bookkeeping, and the fail-fast authoring checks.
 */
class CommandStreamModuleWriterTests {

    private static final long s_goldenGraphHash = 0x0102030405060708L;

    /** Records one lane stream into a fresh native segment and returns the segment. */
    private static MemorySegment recordLaneStream(Arena testArena, int laneIndex,
                                                  long graphHash, boolean includeDispatch) {
        long streamByteSize = includeDispatch ? 80 : 56;
        MemorySegment streamSegment = testArena.allocate(streamByteSize, 8);
        CommandStreamWriter streamWriter =
                new CommandStreamWriter(streamSegment, laneIndex, graphHash);
        if (includeDispatch) {
            streamWriter.appendDraw(3, 1, 0, 0);
            streamWriter.appendDispatch(1, 2, 3);
        } else {
            streamWriter.appendDraw(6, 1, 0, 0);
        }
        assertEquals(streamByteSize, streamWriter.finish());
        return streamSegment;
    }

    /** The golden scenario: a schema-valid buffer table plus lanes 0 and 1. */
    private static byte[] writeGoldenModule(Arena testArena) {
        // Real composition path: table writer -> module writer (staging + device +
        // imported entries, matching the recorder's future CopyBuffer scenario).
        CommandStreamBufferHandleTableWriter tableWriter =
                new CommandStreamBufferHandleTableWriter();
        tableWriter.addCreatedBuffer(64, CommandStreamBufferUsage.s_transferSource,
                CommandStreamBufferMemoryKind.HOST_VISIBLE_PERSISTENT_MAPPED);
        tableWriter.addCreatedBuffer(64, CommandStreamBufferUsage.s_transferDestination,
                CommandStreamBufferMemoryKind.DEVICE_LOCAL);
        tableWriter.addImportedBuffer(1001);
        MemorySegment tableSegment = testArena.allocate(tableWriter.requiredByteSize(), 8);
        tableWriter.writeTo(tableSegment);

        // Barrier batch table matching the golden: one global batch, one buffer batch.
        CommandStreamBarrierBatchTableWriter barrierTableWriter =
                new CommandStreamBarrierBatchTableWriter();
        barrierTableWriter.addBatch(
                List.of(new CommandStreamBarrierBatchTableWriter.GlobalBarrierDescription(
                        CommandStreamPipelineStage.s_transfer,
                        CommandStreamMemoryAccess.s_transferWrite,
                        CommandStreamPipelineStage.s_transfer,
                        CommandStreamMemoryAccess.s_transferRead)),
                List.of());
        barrierTableWriter.addBatch(
                List.of(),
                List.of(new CommandStreamBarrierBatchTableWriter.BufferBarrierDescription(
                        CommandStreamPipelineStage.s_transfer,
                        CommandStreamMemoryAccess.s_transferWrite,
                        CommandStreamPipelineStage.s_transfer,
                        CommandStreamMemoryAccess.s_transferRead,
                        1, 0, CommandStreamBarrierBatchTableWriter.s_wholeByteCount)));
        MemorySegment barrierTableSegment =
                testArena.allocate(barrierTableWriter.requiredByteSize(), 8);
        barrierTableWriter.writeTo(barrierTableSegment);

        CommandStreamModuleWriter moduleWriter = new CommandStreamModuleWriter(s_goldenGraphHash);
        moduleWriter.addSection(CommandStreamModuleSectionType.BUFFER_HANDLE_TABLE, 0,
                tableSegment);
        moduleWriter.addSection(CommandStreamModuleSectionType.BARRIER_BATCH_TABLE, 0,
                barrierTableSegment);
        moduleWriter.addSection(CommandStreamModuleSectionType.LANE_STREAM, 0,
                recordLaneStream(testArena, 0, s_goldenGraphHash, true));
        moduleWriter.addSection(CommandStreamModuleSectionType.LANE_STREAM, 1,
                recordLaneStream(testArena, 1, s_goldenGraphHash, false));

        assertEquals(488, moduleWriter.requiredByteSize());
        MemorySegment moduleSegment = testArena.allocate(488, 8);
        assertEquals(488, moduleWriter.writeTo(moduleSegment));
        return moduleSegment.toArray(ValueLayout.JAVA_BYTE);
    }

    @Test
    void moduleWriterOutputMatchesTheCommittedCrossLanguageGolden() throws Exception {
        Path goldenFilePath = Path.of(System.getProperty("barrieww.testDataDirectory"),
                "CommandStream", "BufferTableAndTwoLaneModule.becs");
        byte[] goldenBytes = Files.readAllBytes(goldenFilePath);

        try (Arena testArena = Arena.ofConfined()) {
            assertArrayEquals(goldenBytes, writeGoldenModule(testArena));
        }
    }

    @Test
    void moduleWriterRejectsDuplicateSectionIdentities() {
        CommandStreamModuleWriter moduleWriter = new CommandStreamModuleWriter(s_goldenGraphHash);
        moduleWriter.addSection(CommandStreamModuleSectionType.BUFFER_HANDLE_TABLE, 0,
                MemorySegment.ofArray(new byte[8]));

        assertThrows(IllegalArgumentException.class,
                () -> moduleWriter.addSection(CommandStreamModuleSectionType.BUFFER_HANDLE_TABLE,
                        0, MemorySegment.ofArray(new byte[8])));
    }

    @Test
    void moduleWriterRejectsNonContiguousLaneIndices() {
        try (Arena testArena = Arena.ofConfined()) {
            CommandStreamModuleWriter moduleWriter =
                    new CommandStreamModuleWriter(s_goldenGraphHash);
            moduleWriter.addSection(CommandStreamModuleSectionType.LANE_STREAM, 1,
                    recordLaneStream(testArena, 1, s_goldenGraphHash, false));
            MemorySegment moduleSegment = testArena.allocate(moduleWriter.requiredByteSize(), 8);

            assertThrows(IllegalStateException.class, () -> moduleWriter.writeTo(moduleSegment));
        }
    }

    @Test
    void moduleWriterRejectsLaneContentWithForeignGraphHash() {
        try (Arena testArena = Arena.ofConfined()) {
            CommandStreamModuleWriter moduleWriter =
                    new CommandStreamModuleWriter(s_goldenGraphHash);
            MemorySegment foreignLaneStream =
                    recordLaneStream(testArena, 0, 0x1111L, true);

            assertThrows(IllegalArgumentException.class,
                    () -> moduleWriter.addSection(CommandStreamModuleSectionType.LANE_STREAM, 0,
                            foreignLaneStream));
        }
    }

    @Test
    void moduleWriterRejectsLaneContentRecordedForAnotherLane() {
        try (Arena testArena = Arena.ofConfined()) {
            CommandStreamModuleWriter moduleWriter =
                    new CommandStreamModuleWriter(s_goldenGraphHash);
            MemorySegment laneZeroStream =
                    recordLaneStream(testArena, 0, s_goldenGraphHash, true);

            assertThrows(IllegalArgumentException.class,
                    () -> moduleWriter.addSection(CommandStreamModuleSectionType.LANE_STREAM, 1,
                            laneZeroStream));
        }
    }

    @Test
    void moduleWriterRejectsUndersizedTargetSegments() {
        try (Arena testArena = Arena.ofConfined()) {
            CommandStreamModuleWriter moduleWriter =
                    new CommandStreamModuleWriter(s_goldenGraphHash);
            moduleWriter.addSection(CommandStreamModuleSectionType.BUFFER_HANDLE_TABLE, 0,
                    MemorySegment.ofArray(new byte[16]));
            MemorySegment undersizedSegment = testArena.allocate(32, 8);

            assertThrows(IllegalArgumentException.class,
                    () -> moduleWriter.writeTo(undersizedSegment));
        }
    }
}
