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
 * Tests of CommandStreamRenderingTemplateTableWriter: byte-exact agreement with the
 * committed cross-language golden (which the C++ validator verifies from its side) and
 * the fail-fast authoring rules mirroring the native validator.
 */
class CommandStreamRenderingTemplateTableWriterTests {

    /** Builds the golden scenario: one 8x8 color template, Clear/Store, clear 0.2/0.4/0.6/1. */
    private static byte[] writeGoldenTable(Arena testArena) {
        CommandStreamRenderingTemplateTableWriter tableWriter =
                new CommandStreamRenderingTemplateTableWriter();
        tableWriter.addTemplate(
                List.of(new CommandStreamRenderingTemplateTableWriter.AttachmentDescription(
                        0, CommandStreamImageLayout.COLOR_ATTACHMENT,
                        CommandStreamAttachmentLoadOp.CLEAR,
                        CommandStreamAttachmentStoreOp.STORE, 0.2f, 0.4f, 0.6f, 1.0f)),
                Optional.empty(), 0, 0, 8, 8, 1, 0);
        assertEquals(80, tableWriter.requiredByteSize());
        MemorySegment tableSegment = testArena.allocate(80, 8);
        assertEquals(80, tableWriter.writeTo(tableSegment));
        return tableSegment.toArray(ValueLayout.JAVA_BYTE);
    }

    @Test
    void tableWriterOutputMatchesTheCommittedCrossLanguageGolden() throws Exception {
        Path goldenFilePath = Path.of(System.getProperty("barrieww.testDataDirectory"),
                "CommandStream", "RenderingTemplateTable.becs");
        byte[] goldenBytes = Files.readAllBytes(goldenFilePath);

        try (Arena testArena = Arena.ofConfined()) {
            assertArrayEquals(goldenBytes, writeGoldenTable(testArena));
        }
    }

    @Test
    void tableWriterEmitsColorPlusDepthInOrder() {
        CommandStreamRenderingTemplateTableWriter tableWriter =
                new CommandStreamRenderingTemplateTableWriter();
        int templateSlot = tableWriter.addTemplate(
                List.of(new CommandStreamRenderingTemplateTableWriter.AttachmentDescription(
                        0, CommandStreamImageLayout.COLOR_ATTACHMENT,
                        CommandStreamAttachmentLoadOp.CLEAR,
                        CommandStreamAttachmentStoreOp.STORE, 0f, 0f, 0f, 1f)),
                Optional.of(new CommandStreamRenderingTemplateTableWriter.AttachmentDescription(
                        1, CommandStreamImageLayout.DEPTH_STENCIL_ATTACHMENT,
                        CommandStreamAttachmentLoadOp.CLEAR,
                        CommandStreamAttachmentStoreOp.DONT_CARE, 1f, 0f, 0f, 0f)),
                0, 0, 16, 16, 1, 0);
        assertEquals(0, templateSlot);
        // Header 8 + directory 40 + two attachment records 64 = 112.
        assertEquals(112, tableWriter.requiredByteSize());
    }

    @Test
    void tableWriterRejectsInvalidTemplates() {
        CommandStreamRenderingTemplateTableWriter tableWriter =
                new CommandStreamRenderingTemplateTableWriter();

        // No attachments at all.
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addTemplate(
                List.of(), Optional.empty(), 0, 0, 8, 8, 1, 0));
        // Zero render area.
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addTemplate(
                List.of(new CommandStreamRenderingTemplateTableWriter.AttachmentDescription(
                        0, CommandStreamImageLayout.COLOR_ATTACHMENT,
                        CommandStreamAttachmentLoadOp.CLEAR,
                        CommandStreamAttachmentStoreOp.STORE, 0f, 0f, 0f, 1f)),
                Optional.empty(), 0, 0, 0, 8, 1, 0));
        // Zero layer count.
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addTemplate(
                List.of(new CommandStreamRenderingTemplateTableWriter.AttachmentDescription(
                        0, CommandStreamImageLayout.COLOR_ATTACHMENT,
                        CommandStreamAttachmentLoadOp.CLEAR,
                        CommandStreamAttachmentStoreOp.STORE, 0f, 0f, 0f, 1f)),
                Optional.empty(), 0, 0, 8, 8, 0, 0));
    }
}
