package barrieww.core.commandstream;

import static org.junit.jupiter.api.Assertions.assertArrayEquals;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Files;
import java.nio.file.Path;
import org.junit.jupiter.api.Test;

/**
 * @note ThreadSafety: JUnit creates a fresh instance per test method; no shared state.
 * Byte-exact and fail-fast tests of the ImageViewHandleTable v0.1 Java writer, including
 * shared table and module fixtures consumed by Native validation/materialization tests.
 */
class CommandStreamImageViewHandleTableWriterTests {

    private static final long s_moduleGraphHash = 0x1122334455667788L;

    /** Builds the canonical two-entry writer used by exact-layout and table golden tests. */
    private static CommandStreamImageViewHandleTableWriter createGoldenTableWriter() {
        CommandStreamImageViewHandleTableWriter tableWriter =
                new CommandStreamImageViewHandleTableWriter();
        assertEquals(0, tableWriter.addImageView(4, CommandStreamImageViewKind.TWO_DIMENSIONAL,
                CommandStreamImageFormat.R8G8B8A8_UNORM, CommandStreamImageAspect.s_color,
                1, 2, 3, 1));
        assertEquals(1, tableWriter.addImageView(8, CommandStreamImageViewKind.THREE_DIMENSIONAL,
                CommandStreamImageFormat.D32_FLOAT, CommandStreamImageAspect.s_depth,
                0, 1, 0, 1));
        return tableWriter;
    }

    /** Emits the canonical two-entry table into memory owned by testArena. */
    private static MemorySegment writeGoldenTable(Arena testArena) {
        CommandStreamImageViewHandleTableWriter tableWriter = createGoldenTableWriter();
        MemorySegment tableSegment = testArena.allocate(tableWriter.requiredByteSize(), 8);
        assertEquals(tableWriter.requiredByteSize(), tableWriter.writeTo(tableSegment));
        return tableSegment;
    }

    /** Emits a module whose Java-produced image and view tables can materialize together. */
    private static byte[] writeMaterializationGoldenModule(Arena testArena) {
        CommandStreamImageHandleTableWriter imageTableWriter =
                new CommandStreamImageHandleTableWriter();
        imageTableWriter.addCreatedImage(CommandStreamImageKind.TWO_DIMENSIONAL,
                CommandStreamImageFormat.R8G8B8A8_UNORM, 8, 8, 1, 1, 1, 1,
                CommandStreamImageUsage.s_sampled);
        MemorySegment imageTableSegment =
                testArena.allocate(imageTableWriter.requiredByteSize(), 8);
        imageTableWriter.writeTo(imageTableSegment);

        CommandStreamImageViewHandleTableWriter imageViewTableWriter =
                new CommandStreamImageViewHandleTableWriter();
        imageViewTableWriter.addImageView(0, CommandStreamImageViewKind.TWO_DIMENSIONAL,
                CommandStreamImageFormat.R8G8B8A8_UNORM, CommandStreamImageAspect.s_color,
                0, 1, 0, 1);
        MemorySegment imageViewTableSegment =
                testArena.allocate(imageViewTableWriter.requiredByteSize(), 8);
        imageViewTableWriter.writeTo(imageViewTableSegment);

        CommandStreamModuleWriter moduleWriter = new CommandStreamModuleWriter(s_moduleGraphHash);
        moduleWriter.addSection(CommandStreamModuleSectionType.IMAGE_HANDLE_TABLE, 0,
                imageTableSegment);
        moduleWriter.addSection(CommandStreamModuleSectionType.IMAGE_VIEW_HANDLE_TABLE, 0,
                imageViewTableSegment);
        assertEquals(168, moduleWriter.requiredByteSize());
        MemorySegment moduleSegment = testArena.allocate(moduleWriter.requiredByteSize(), 8);
        moduleWriter.writeTo(moduleSegment);
        return moduleSegment.toArray(ValueLayout.JAVA_BYTE);
    }

    @Test
    void tableWriterProducesTheExactWireLayout() {
        ByteBuffer expectedBytes = ByteBuffer.allocate(72).order(ByteOrder.LITTLE_ENDIAN);
        expectedBytes.putInt(2).putInt(0);
        expectedBytes.putInt(4).putInt(2).putInt(1).putInt(0x1)
                .putInt(1).putInt(2).putInt(3).putInt(1);
        expectedBytes.putInt(8).putInt(3).putInt(5).putInt(0x2)
                .putInt(0).putInt(1).putInt(0).putInt(1);

        try (Arena testArena = Arena.ofConfined()) {
            MemorySegment tableSegment = writeGoldenTable(testArena);
            assertEquals(72, tableSegment.byteSize());
            assertArrayEquals(expectedBytes.array(), tableSegment.toArray(ValueLayout.JAVA_BYTE));

            CommandStreamImageViewHandleTableWriter tableWriter = createGoldenTableWriter();
            assertEquals(72, tableWriter.writeTo(tableSegment));
            assertArrayEquals(expectedBytes.array(), tableSegment.toArray(ValueLayout.JAVA_BYTE));
        }
    }

    @Test
    void tableWriterMatchesTheCommittedCrossLanguageGolden() throws Exception {
        Path goldenFilePath = Path.of(System.getProperty("barrieww.testDataDirectory"),
                "CommandStream", "ImageViewHandleTable.becs");
        byte[] goldenBytes = Files.readAllBytes(goldenFilePath);

        try (Arena testArena = Arena.ofConfined()) {
            assertArrayEquals(goldenBytes,
                    writeGoldenTable(testArena).toArray(ValueLayout.JAVA_BYTE));
        }
    }

    @Test
    void imageAndImageViewModuleMatchesTheMaterializationGolden() throws Exception {
        Path goldenFilePath = Path.of(System.getProperty("barrieww.testDataDirectory"),
                "CommandStream", "ImageAndImageViewModule.becs");
        byte[] goldenBytes = Files.readAllBytes(goldenFilePath);

        try (Arena testArena = Arena.ofConfined()) {
            assertArrayEquals(goldenBytes, writeMaterializationGoldenModule(testArena));
        }
    }

    @Test
    void tableWriterRejectsInvalidDescriptions() {
        CommandStreamImageViewHandleTableWriter tableWriter =
                new CommandStreamImageViewHandleTableWriter();

        assertThrows(IllegalArgumentException.class, () -> tableWriter.addImageView(-1,
                CommandStreamImageViewKind.TWO_DIMENSIONAL,
                CommandStreamImageFormat.R8G8B8A8_UNORM, CommandStreamImageAspect.s_color,
                0, 1, 0, 1));
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addImageView(0, null,
                CommandStreamImageFormat.R8G8B8A8_UNORM, CommandStreamImageAspect.s_color,
                0, 1, 0, 1));
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addImageView(0,
                CommandStreamImageViewKind.TWO_DIMENSIONAL, null,
                CommandStreamImageAspect.s_color, 0, 1, 0, 1));
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addImageView(0,
                CommandStreamImageViewKind.TWO_DIMENSIONAL,
                CommandStreamImageFormat.R8G8B8A8_UNORM, 0,
                0, 1, 0, 1));
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addImageView(0,
                CommandStreamImageViewKind.TWO_DIMENSIONAL,
                CommandStreamImageFormat.R8G8B8A8_UNORM, 0x80,
                0, 1, 0, 1));
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addImageView(0,
                CommandStreamImageViewKind.TWO_DIMENSIONAL,
                CommandStreamImageFormat.R8G8B8A8_UNORM, CommandStreamImageAspect.s_color,
                -1, 1, 0, 1));
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addImageView(0,
                CommandStreamImageViewKind.TWO_DIMENSIONAL,
                CommandStreamImageFormat.R8G8B8A8_UNORM, CommandStreamImageAspect.s_color,
                0, 0, 0, 1));
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addImageView(0,
                CommandStreamImageViewKind.TWO_DIMENSIONAL,
                CommandStreamImageFormat.R8G8B8A8_UNORM, CommandStreamImageAspect.s_color,
                0, 1, -1, 1));
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addImageView(0,
                CommandStreamImageViewKind.THREE_DIMENSIONAL,
                CommandStreamImageFormat.D32_FLOAT, CommandStreamImageAspect.s_depth,
                0, 1, 1, 1));
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addImageView(0,
                CommandStreamImageViewKind.TWO_DIMENSIONAL,
                CommandStreamImageFormat.R8G8B8A8_UNORM, CommandStreamImageAspect.s_color,
                0, 1, 0, 2));

        try (Arena testArena = Arena.ofConfined()) {
            MemorySegment undersizedSegment = testArena.allocate(7, 8);
            assertThrows(IllegalArgumentException.class,
                    () -> tableWriter.writeTo(undersizedSegment));
        }
    }
}
