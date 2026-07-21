package barrieww.core.commandstream;

import static org.junit.jupiter.api.Assertions.assertArrayEquals;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import org.junit.jupiter.api.Test;

/**
 * @note ThreadSafety: JUnit creates a fresh instance per test method; no shared state.
 * Byte-exact tests of CommandStreamImageHandleTableWriter against the schema v0.1
 * layout (the expected bytes reproduce the table the native image execution test
 * builds by hand), plus the fail-fast authoring rules mirroring the C++ validator.
 */
class CommandStreamImageHandleTableWriterTests {

    @Test
    void tableWriterProducesTheExactWireLayout() {
        CommandStreamImageHandleTableWriter tableWriter =
                new CommandStreamImageHandleTableWriter();
        assertEquals(0, tableWriter.addCreatedImage(
                CommandStreamImageKind.TWO_DIMENSIONAL,
                CommandStreamImageFormat.R8G8B8A8_UNORM, 8, 8, 1, 1, 1, 1,
                CommandStreamImageUsage.s_transferSource
                        | CommandStreamImageUsage.s_transferDestination));
        assertEquals(1, tableWriter.addImportedImage(2002,
                CommandStreamImageKind.TWO_DIMENSIONAL,
                CommandStreamImageFormat.B8G8R8A8_UNORM, 1920, 1080, 1, 1, 1, 1, 0));
        assertEquals(88, tableWriter.requiredByteSize());

        ByteBuffer expectedBytes = ByteBuffer.allocate(88).order(ByteOrder.LITTLE_ENDIAN);
        expectedBytes.putInt(2).putInt(0);
        expectedBytes.putInt(2).putInt(1).putInt(8).putInt(8).putInt(1)
                .putInt(1).putInt(1).putInt(1).putInt(0x3).putInt(0);
        expectedBytes.putInt(2).putInt(2).putInt(1920).putInt(1080).putInt(1)
                .putInt(1).putInt(1).putInt(1).putInt(0).putInt(2002);

        try (Arena testArena = Arena.ofConfined()) {
            MemorySegment tableSegment = testArena.allocate(88, 8);
            assertEquals(88, tableWriter.writeTo(tableSegment));
            assertArrayEquals(expectedBytes.array(),
                    tableSegment.toArray(ValueLayout.JAVA_BYTE));
        }
    }

    @Test
    void tableWriterRejectsInvalidDescriptions() {
        CommandStreamImageHandleTableWriter tableWriter =
                new CommandStreamImageHandleTableWriter();

        // Created image with empty usage.
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addCreatedImage(
                CommandStreamImageKind.TWO_DIMENSIONAL,
                CommandStreamImageFormat.R8G8B8A8_UNORM, 8, 8, 1, 1, 1, 1, 0));
        // Zero width.
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addCreatedImage(
                CommandStreamImageKind.TWO_DIMENSIONAL,
                CommandStreamImageFormat.R8G8B8A8_UNORM, 0, 8, 1, 1, 1, 1, 0x1));
        // 3D image with multiple array layers.
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addCreatedImage(
                CommandStreamImageKind.THREE_DIMENSIONAL,
                CommandStreamImageFormat.R8G8B8A8_UNORM, 8, 8, 4, 1, 2, 1, 0x1));
        // Non-power-of-two sample count.
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addCreatedImage(
                CommandStreamImageKind.TWO_DIMENSIONAL,
                CommandStreamImageFormat.R8G8B8A8_UNORM, 8, 8, 1, 1, 1, 3, 0x1));
        // Unassigned usage bit.
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addCreatedImage(
                CommandStreamImageKind.TWO_DIMENSIONAL,
                CommandStreamImageFormat.R8G8B8A8_UNORM, 8, 8, 1, 1, 1, 1, 0x8000));
        // Zero import identifier.
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addImportedImage(0,
                CommandStreamImageKind.TWO_DIMENSIONAL,
                CommandStreamImageFormat.R8G8B8A8_UNORM, 8, 8, 1, 1, 1, 1, 0));
    }
}
