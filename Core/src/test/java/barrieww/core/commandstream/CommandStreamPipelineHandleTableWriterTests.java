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
 * Byte-exact tests of CommandStreamPipelineHandleTableWriter against the schema v0.1
 * layout — the expected bytes reproduce exactly the table the native GPU-driven
 * dispatch test builds by hand — plus the fail-fast authoring rules.
 */
class CommandStreamPipelineHandleTableWriterTests {

    @Test
    void tableWriterProducesTheExactWireLayout() {
        CommandStreamPipelineHandleTableWriter tableWriter =
                new CommandStreamPipelineHandleTableWriter();
        assertEquals(0, tableWriter.addComputePipeline(0, 8));
        assertEquals(1, tableWriter.addComputePipeline(1, 8));
        assertEquals(40, tableWriter.requiredByteSize());

        ByteBuffer expectedBytes = ByteBuffer.allocate(40).order(ByteOrder.LITTLE_ENDIAN);
        expectedBytes.putInt(2).putInt(0);
        expectedBytes.putInt(1).putInt(0).putInt(8).putInt(0);
        expectedBytes.putInt(1).putInt(1).putInt(8).putInt(0);

        try (Arena testArena = Arena.ofConfined()) {
            MemorySegment tableSegment = testArena.allocate(40, 8);
            assertEquals(40, tableWriter.writeTo(tableSegment));
            assertArrayEquals(expectedBytes.array(),
                    tableSegment.toArray(ValueLayout.JAVA_BYTE));
        }
    }

    @Test
    void tableWriterAllowsAnEmptyPushConstantRange() {
        CommandStreamPipelineHandleTableWriter tableWriter =
                new CommandStreamPipelineHandleTableWriter();
        assertEquals(0, tableWriter.addComputePipeline(3, 0));
        assertEquals(24, tableWriter.requiredByteSize());
    }

    @Test
    void tableWriterRejectsInvalidDescriptions() {
        CommandStreamPipelineHandleTableWriter tableWriter =
                new CommandStreamPipelineHandleTableWriter();

        assertThrows(IllegalArgumentException.class,
                () -> tableWriter.addComputePipeline(-1, 8));
        assertThrows(IllegalArgumentException.class,
                () -> tableWriter.addComputePipeline(0, 6));
        assertThrows(IllegalArgumentException.class,
                () -> tableWriter.addComputePipeline(0, 132));
    }
}
