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
 * Byte-exact tests of CommandStreamBufferHandleTableWriter against the schema v0.1
 * layout, plus the fail-fast authoring rules (the same rules the C++ validator
 * enforces at load, shifted left).
 */
class CommandStreamBufferHandleTableWriterTests {

    @Test
    void tableWriterProducesTheExactWireLayout() {
        CommandStreamBufferHandleTableWriter tableWriter =
                new CommandStreamBufferHandleTableWriter();
        assertEquals(0, tableWriter.addCreatedBuffer(64, CommandStreamBufferUsage.s_transferSource,
                CommandStreamBufferMemoryKind.HOST_VISIBLE_PERSISTENT_MAPPED));
        assertEquals(1, tableWriter.addCreatedBuffer(64,
                CommandStreamBufferUsage.s_transferDestination,
                CommandStreamBufferMemoryKind.DEVICE_LOCAL));
        assertEquals(2, tableWriter.addImportedBuffer(1001));
        assertEquals(80, tableWriter.requiredByteSize());

        ByteBuffer expectedBytes = ByteBuffer.allocate(80).order(ByteOrder.LITTLE_ENDIAN);
        expectedBytes.putInt(3).putInt(0);
        expectedBytes.putLong(64).putInt(0x1).putInt(2).putInt(0).putInt(0);
        expectedBytes.putLong(64).putInt(0x2).putInt(1).putInt(0).putInt(0);
        expectedBytes.putLong(0).putInt(0).putInt(0).putInt(1001).putInt(0);

        try (Arena testArena = Arena.ofConfined()) {
            MemorySegment tableSegment = testArena.allocate(80, 8);
            assertEquals(80, tableWriter.writeTo(tableSegment));
            assertArrayEquals(expectedBytes.array(),
                    tableSegment.toArray(ValueLayout.JAVA_BYTE));
        }
    }

    @Test
    void tableWriterRejectsInvalidCreatedDescriptions() {
        CommandStreamBufferHandleTableWriter tableWriter =
                new CommandStreamBufferHandleTableWriter();

        assertThrows(IllegalArgumentException.class,
                () -> tableWriter.addCreatedBuffer(0, CommandStreamBufferUsage.s_uniform,
                        CommandStreamBufferMemoryKind.DEVICE_LOCAL));
        assertThrows(IllegalArgumentException.class,
                () -> tableWriter.addCreatedBuffer(64, 0,
                        CommandStreamBufferMemoryKind.DEVICE_LOCAL));
        assertThrows(IllegalArgumentException.class,
                () -> tableWriter.addCreatedBuffer(64, 0x8000,
                        CommandStreamBufferMemoryKind.DEVICE_LOCAL));
        assertThrows(IllegalArgumentException.class,
                () -> tableWriter.addCreatedBuffer(64, CommandStreamBufferUsage.s_uniform,
                        CommandStreamBufferMemoryKind.NONE));
    }

    @Test
    void tableWriterRejectsZeroImportIdentifier() {
        CommandStreamBufferHandleTableWriter tableWriter =
                new CommandStreamBufferHandleTableWriter();

        assertThrows(IllegalArgumentException.class, () -> tableWriter.addImportedBuffer(0));
    }
}
