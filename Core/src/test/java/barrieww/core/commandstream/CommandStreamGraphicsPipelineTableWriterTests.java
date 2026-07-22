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
import java.util.Optional;
import org.junit.jupiter.api.Test;

/**
 * @note ThreadSafety: JUnit creates a fresh instance per test method; no shared state.
 * Tests of CommandStreamGraphicsPipelineTableWriter: byte-exact agreement with the
 * committed cross-language golden (which the C++ validator verifies from its side) and
 * the fail-fast authoring rules mirroring the native validator.
 */
class CommandStreamGraphicsPipelineTableWriterTests {

    /**
     * Builds the golden scenario: vertex slot 0 + fragment slot 1, 8-byte push range,
     * TriangleList, one R8G8B8A8Unorm color attachment, depth D32Float with
     * test+write+LessOrEqual, back-face culling, counter-clockwise front.
     */
    private static byte[] writeGoldenTable(Arena testArena) {
        CommandStreamGraphicsPipelineTableWriter tableWriter =
                new CommandStreamGraphicsPipelineTableWriter();
        tableWriter.addPipeline(0, 1, 8, CommandStreamPrimitiveTopology.TRIANGLE_LIST,
                List.of(CommandStreamImageFormat.R8G8B8A8_UNORM),
                Optional.of(new CommandStreamGraphicsPipelineTableWriter.DepthState(
                        CommandStreamImageFormat.D32_FLOAT, true, true,
                        Optional.of(CommandStreamCompareOperation.LESS_OR_EQUAL))),
                CommandStreamCullMode.BACK, CommandStreamFrontFace.COUNTER_CLOCKWISE);
        assertEquals(88, tableWriter.requiredByteSize());
        MemorySegment tableSegment = testArena.allocate(88, 8);
        assertEquals(88, tableWriter.writeTo(tableSegment));
        return tableSegment.toArray(ValueLayout.JAVA_BYTE);
    }

    @Test
    void tableWriterOutputMatchesTheCommittedCrossLanguageGolden() throws Exception {
        Path goldenFilePath = Path.of(System.getProperty("barrieww.testDataDirectory"),
                "CommandStream", "GraphicsPipelineTable.becs");
        byte[] goldenBytes = Files.readAllBytes(goldenFilePath);

        try (Arena testArena = Arena.ofConfined()) {
            assertArrayEquals(goldenBytes, writeGoldenTable(testArena));
        }
    }

    @Test
    void tableWriterZeroesUnusedColorFormatIndices() {
        CommandStreamGraphicsPipelineTableWriter tableWriter =
                new CommandStreamGraphicsPipelineTableWriter();
        int pipelineSlot = tableWriter.addPipeline(0, 1, 0,
                CommandStreamPrimitiveTopology.TRIANGLE_LIST,
                List.of(CommandStreamImageFormat.R8G8B8A8_UNORM), Optional.empty(),
                CommandStreamCullMode.NONE, CommandStreamFrontFace.COUNTER_CLOCKWISE);
        assertEquals(0, pipelineSlot);

        try (Arena testArena = Arena.ofConfined()) {
            MemorySegment tableSegment = testArena.allocate(88, 8);
            tableWriter.writeTo(tableSegment);
            assertEquals(1, tableSegment.get(ValueLayout.JAVA_INT_UNALIGNED, 56)); // used
            for (long formatIndex = 1; formatIndex < 8; ++formatIndex) {
                assertEquals(0, tableSegment.get(ValueLayout.JAVA_INT_UNALIGNED,
                        56 + 4 * formatIndex));
            }
            // Disabled depth state emits all-zero depth fields.
            assertEquals(0, tableSegment.get(ValueLayout.JAVA_INT_UNALIGNED, 28));
            assertEquals(0, tableSegment.get(ValueLayout.JAVA_INT_UNALIGNED, 32));
            assertEquals(0, tableSegment.get(ValueLayout.JAVA_INT_UNALIGNED, 40));
        }
    }

    @Test
    void tableWriterRejectsInvalidPipelines() {
        CommandStreamGraphicsPipelineTableWriter tableWriter =
                new CommandStreamGraphicsPipelineTableWriter();

        // Push range not a multiple of 4.
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addPipeline(0, 1, 6,
                CommandStreamPrimitiveTopology.TRIANGLE_LIST,
                List.of(CommandStreamImageFormat.R8G8B8A8_UNORM), Optional.empty(),
                CommandStreamCullMode.NONE, CommandStreamFrontFace.COUNTER_CLOCKWISE));

        // No attachment at all.
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addPipeline(0, 1, 0,
                CommandStreamPrimitiveTopology.TRIANGLE_LIST, List.of(), Optional.empty(),
                CommandStreamCullMode.NONE, CommandStreamFrontFace.COUNTER_CLOCKWISE));

        // Depth format in a color attachment position.
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addPipeline(0, 1, 0,
                CommandStreamPrimitiveTopology.TRIANGLE_LIST,
                List.of(CommandStreamImageFormat.D32_FLOAT), Optional.empty(),
                CommandStreamCullMode.NONE, CommandStreamFrontFace.COUNTER_CLOCKWISE));

        // Color format as the depth attachment.
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addPipeline(0, 1, 0,
                CommandStreamPrimitiveTopology.TRIANGLE_LIST,
                List.of(CommandStreamImageFormat.R8G8B8A8_UNORM),
                Optional.of(new CommandStreamGraphicsPipelineTableWriter.DepthState(
                        CommandStreamImageFormat.R8G8B8A8_UNORM, false, false,
                        Optional.empty())),
                CommandStreamCullMode.NONE, CommandStreamFrontFace.COUNTER_CLOCKWISE));

        // Depth writes without depth testing.
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addPipeline(0, 1, 0,
                CommandStreamPrimitiveTopology.TRIANGLE_LIST,
                List.of(CommandStreamImageFormat.R8G8B8A8_UNORM),
                Optional.of(new CommandStreamGraphicsPipelineTableWriter.DepthState(
                        CommandStreamImageFormat.D32_FLOAT, false, true,
                        Optional.empty())),
                CommandStreamCullMode.NONE, CommandStreamFrontFace.COUNTER_CLOCKWISE));

        // Compare operation present while testing is disabled.
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addPipeline(0, 1, 0,
                CommandStreamPrimitiveTopology.TRIANGLE_LIST,
                List.of(CommandStreamImageFormat.R8G8B8A8_UNORM),
                Optional.of(new CommandStreamGraphicsPipelineTableWriter.DepthState(
                        CommandStreamImageFormat.D32_FLOAT, false, false,
                        Optional.of(CommandStreamCompareOperation.LESS))),
                CommandStreamCullMode.NONE, CommandStreamFrontFace.COUNTER_CLOCKWISE));

        // Nothing was collected by the failed additions.
        assertEquals(8, tableWriter.requiredByteSize());
    }
}
