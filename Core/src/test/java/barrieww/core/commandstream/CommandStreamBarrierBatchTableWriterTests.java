package barrieww.core.commandstream;

import static org.junit.jupiter.api.Assertions.assertArrayEquals;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.List;
import org.junit.jupiter.api.Test;

/**
 * @note ThreadSafety: JUnit creates a fresh instance per test method; no shared state.
 * Byte-exact tests of CommandStreamBarrierBatchTableWriter against the schema v0.1
 * layout (matching the golden module's barrier table), plus the fail-fast rules.
 */
class CommandStreamBarrierBatchTableWriterTests {

    @Test
    void barrierTableWriterProducesTheExactWireLayout() {
        CommandStreamBarrierBatchTableWriter tableWriter =
                new CommandStreamBarrierBatchTableWriter();
        assertEquals(0, tableWriter.addBatch(
                List.of(new CommandStreamBarrierBatchTableWriter.GlobalBarrierDescription(
                        CommandStreamPipelineStage.s_transfer,
                        CommandStreamMemoryAccess.s_transferWrite,
                        CommandStreamPipelineStage.s_transfer,
                        CommandStreamMemoryAccess.s_transferRead)),
                List.of()));
        assertEquals(1, tableWriter.addBatch(
                List.of(),
                List.of(new CommandStreamBarrierBatchTableWriter.BufferBarrierDescription(
                        CommandStreamPipelineStage.s_transfer,
                        CommandStreamMemoryAccess.s_transferWrite,
                        CommandStreamPipelineStage.s_transfer,
                        CommandStreamMemoryAccess.s_transferRead,
                        1, 0, CommandStreamBarrierBatchTableWriter.s_wholeByteCount))));
        assertEquals(2, tableWriter.addBatch(List.of(), List.of(),
                List.of(new CommandStreamBarrierBatchTableWriter.ImageBarrierDescription(
                        0x1L, 0L,
                        CommandStreamPipelineStage.s_transfer,
                        CommandStreamMemoryAccess.s_transferWrite,
                        0, 0x1, 0, 6, 0, 1, 0,
                        CommandStreamBarrierBatchTableWriter.s_remainingCount))));
        // Header 8 + 3*24 directory = 80; global 32 + buffer 56 + image 64 -> 232.
        assertEquals(232, tableWriter.requiredByteSize());

        ByteBuffer expectedBytes = ByteBuffer.allocate(232).order(ByteOrder.LITTLE_ENDIAN);
        expectedBytes.putInt(3).putInt(0);
        expectedBytes.putInt(1).putInt(0).putInt(0).putInt(0).putLong(80);
        expectedBytes.putInt(0).putInt(1).putInt(0).putInt(0).putLong(112);
        expectedBytes.putInt(0).putInt(0).putInt(1).putInt(0).putLong(168);
        expectedBytes.putLong(0x1000L).putLong(0x1000L).putLong(0x1000L).putLong(0x800L);
        expectedBytes.putLong(0x1000L).putLong(0x1000L).putLong(0x1000L).putLong(0x800L);
        expectedBytes.putInt(1).putInt(0).putLong(0)
                .putLong(CommandStreamBarrierBatchTableWriter.s_wholeByteCount);
        expectedBytes.putLong(0x1L).putLong(0L).putLong(0x1000L).putLong(0x1000L);
        expectedBytes.putInt(0).putInt(0x1).putInt(0).putInt(6);
        expectedBytes.putInt(0).putInt(1).putInt(0)
                .putInt(CommandStreamBarrierBatchTableWriter.s_remainingCount);

        try (Arena testArena = Arena.ofConfined()) {
            MemorySegment tableSegment = testArena.allocate(232, 8);
            assertEquals(232, tableWriter.writeTo(tableSegment));
            assertArrayEquals(expectedBytes.array(),
                    tableSegment.toArray(ValueLayout.JAVA_BYTE));
        }
    }

    @Test
    void barrierTableWriterRejectsInvalidImageBarriers() {
        CommandStreamBarrierBatchTableWriter tableWriter =
                new CommandStreamBarrierBatchTableWriter();

        // Zero mip level count.
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addBatch(
                List.of(), List.of(),
                List.of(new CommandStreamBarrierBatchTableWriter.ImageBarrierDescription(
                        0x1L, 0L, CommandStreamPipelineStage.s_transfer,
                        CommandStreamMemoryAccess.s_transferWrite,
                        0, 0x1, 0, 6, 0, 0, 0, 1))));
        // Negative image slot.
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addBatch(
                List.of(), List.of(),
                List.of(new CommandStreamBarrierBatchTableWriter.ImageBarrierDescription(
                        0x1L, 0L, CommandStreamPipelineStage.s_transfer,
                        CommandStreamMemoryAccess.s_transferWrite,
                        -1, 0x1, 0, 6, 0, 1, 0, 1))));
    }

    @Test
    void barrierTableWriterRejectsEmptyStageMasks() {
        CommandStreamBarrierBatchTableWriter tableWriter =
                new CommandStreamBarrierBatchTableWriter();

        assertThrows(IllegalArgumentException.class, () -> tableWriter.addBatch(
                List.of(new CommandStreamBarrierBatchTableWriter.GlobalBarrierDescription(
                        0, 0, CommandStreamPipelineStage.s_transfer, 0)),
                List.of()));
        assertThrows(IllegalArgumentException.class, () -> tableWriter.addBatch(
                List.of(),
                List.of(new CommandStreamBarrierBatchTableWriter.BufferBarrierDescription(
                        CommandStreamPipelineStage.s_transfer, 0, 0, 0, 1, 0, 64))));
    }
}
