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
 * Byte-exact tests of CommandStreamWriter against the ADR-0002 D3 layout, including the
 * committed cross-language golden file that the C++ validator verifies from its side.
 */
class CommandStreamWriterTests {

    private static final long s_goldenGraphHash = 0x0102030405060708L;

    /** Writes the golden scenario (Draw + Dispatch, lane 0) and returns its bytes. */
    private static byte[] writeTwoCommandStream() {
        try (Arena testArena = Arena.ofConfined()) {
            MemorySegment streamSegment = testArena.allocate(80, 8);
            CommandStreamWriter streamWriter =
                    new CommandStreamWriter(streamSegment, 0, s_goldenGraphHash);
            streamWriter.appendDraw(3, 1, 0, 0);
            streamWriter.appendDispatch(1, 2, 3);
            long totalByteSize = streamWriter.finish();
            assertEquals(80, totalByteSize);
            return streamSegment.toArray(ValueLayout.JAVA_BYTE);
        }
    }

    @Test
    void writerProducesTheExactWireLayout() {
        ByteBuffer expectedBytes = ByteBuffer.allocate(80).order(ByteOrder.LITTLE_ENDIAN);
        // Stream header (ADR-0002 D3).
        expectedBytes.put(new byte[] {0x42, 0x45, 0x43, 0x53});
        expectedBytes.putShort(CommandStreamFormat.s_currentVersionMajor);
        expectedBytes.putShort(CommandStreamFormat.s_currentVersionMinor);
        expectedBytes.putInt(0);
        expectedBytes.putInt(2);
        expectedBytes.putLong(80);
        expectedBytes.putLong(s_goldenGraphHash);
        // Draw command.
        expectedBytes.putShort((short) 0x0010).putShort((short) 0).putInt(24);
        expectedBytes.putInt(3).putInt(1).putInt(0).putInt(0);
        // Dispatch command.
        expectedBytes.putShort((short) 0x0020).putShort((short) 0).putInt(24);
        expectedBytes.putInt(1).putInt(2).putInt(3).putInt(0);

        assertArrayEquals(expectedBytes.array(), writeTwoCommandStream());
    }

    @Test
    void writerOutputMatchesTheCommittedCrossLanguageGolden() throws Exception {
        Path goldenFilePath = Path.of(System.getProperty("barrieww.testDataDirectory"),
                "CommandStream", "TwoCommandLaneStream.becs");
        byte[] goldenBytes = Files.readAllBytes(goldenFilePath);

        assertArrayEquals(goldenBytes, writeTwoCommandStream());
    }

    @Test
    void copyBufferCommandUsesThePinnedPayloadLayout() {
        ByteBuffer expectedBytes = ByteBuffer.allocate(72).order(ByteOrder.LITTLE_ENDIAN);
        expectedBytes.put(new byte[] {0x42, 0x45, 0x43, 0x53});
        expectedBytes.putShort(CommandStreamFormat.s_currentVersionMajor);
        expectedBytes.putShort(CommandStreamFormat.s_currentVersionMinor);
        expectedBytes.putInt(0);
        expectedBytes.putInt(1);
        expectedBytes.putLong(72);
        expectedBytes.putLong(s_goldenGraphHash);
        expectedBytes.putShort((short) 0x0040).putShort((short) 0).putInt(40);
        expectedBytes.putInt(0).putInt(1);
        expectedBytes.putLong(16).putLong(32).putLong(64);

        try (Arena testArena = Arena.ofConfined()) {
            MemorySegment streamSegment = testArena.allocate(72, 8);
            CommandStreamWriter streamWriter =
                    new CommandStreamWriter(streamSegment, 0, s_goldenGraphHash);
            streamWriter.appendCopyBuffer(0, 1, 16, 32, 64);
            assertEquals(72, streamWriter.finish());
            assertArrayEquals(expectedBytes.array(),
                    streamSegment.toArray(ValueLayout.JAVA_BYTE));
        }
    }

    @Test
    void computePipelineCommandsUseThePinnedPayloadLayouts() {
        ByteBuffer expectedBytes = ByteBuffer.allocate(64).order(ByteOrder.LITTLE_ENDIAN);
        expectedBytes.put(new byte[] {0x42, 0x45, 0x43, 0x53});
        expectedBytes.putShort(CommandStreamFormat.s_currentVersionMajor);
        expectedBytes.putShort(CommandStreamFormat.s_currentVersionMinor);
        expectedBytes.putInt(0);
        expectedBytes.putInt(2);
        expectedBytes.putLong(64);
        expectedBytes.putLong(s_goldenGraphHash);
        expectedBytes.putShort((short) 0x0002).putShort((short) 0).putInt(16);
        expectedBytes.putInt(5).putInt(0);
        expectedBytes.putShort((short) 0x0008).putShort((short) 0).putInt(16);
        expectedBytes.putInt(3).putInt(8);

        try (Arena testArena = Arena.ofConfined()) {
            MemorySegment streamSegment = testArena.allocate(64, 8);
            CommandStreamWriter streamWriter =
                    new CommandStreamWriter(streamSegment, 0, s_goldenGraphHash);
            streamWriter.appendBindComputePipeline(5);
            streamWriter.appendPushBufferDeviceAddress(3, 8);
            assertEquals(64, streamWriter.finish());
            assertArrayEquals(expectedBytes.array(),
                    streamSegment.toArray(ValueLayout.JAVA_BYTE));
        }
    }

    @Test
    void drawIndirectCommandUsesThePinnedPayloadLayout() {
        ByteBuffer expectedBytes = ByteBuffer.allocate(64).order(ByteOrder.LITTLE_ENDIAN);
        expectedBytes.put(new byte[] {0x42, 0x45, 0x43, 0x53});
        expectedBytes.putShort(CommandStreamFormat.s_currentVersionMajor);
        expectedBytes.putShort(CommandStreamFormat.s_currentVersionMinor);
        expectedBytes.putInt(0);
        expectedBytes.putInt(1);
        expectedBytes.putLong(64);
        expectedBytes.putLong(s_goldenGraphHash);
        expectedBytes.putShort((short) 0x0012).putShort((short) 0).putInt(32);
        expectedBytes.putInt(2).putInt(1).putLong(48).putInt(16).putInt(0);

        try (Arena testArena = Arena.ofConfined()) {
            MemorySegment streamSegment = testArena.allocate(64, 8);
            CommandStreamWriter streamWriter =
                    new CommandStreamWriter(streamSegment, 0, s_goldenGraphHash);
            streamWriter.appendDrawIndirect(2, 1, 48, 16);
            assertEquals(64, streamWriter.finish());
            assertArrayEquals(expectedBytes.array(),
                    streamSegment.toArray(ValueLayout.JAVA_BYTE));
        }
    }

    @Test
    void dispatchIndirectCommandUsesThePinnedPayloadLayout() {
        ByteBuffer expectedBytes = ByteBuffer.allocate(56).order(ByteOrder.LITTLE_ENDIAN);
        expectedBytes.put(new byte[] {0x42, 0x45, 0x43, 0x53});
        expectedBytes.putShort(CommandStreamFormat.s_currentVersionMajor);
        expectedBytes.putShort(CommandStreamFormat.s_currentVersionMinor);
        expectedBytes.putInt(0);
        expectedBytes.putInt(1);
        expectedBytes.putLong(56);
        expectedBytes.putLong(s_goldenGraphHash);
        expectedBytes.putShort((short) 0x0021).putShort((short) 0).putInt(24);
        expectedBytes.putInt(4).putInt(0).putLong(12);

        try (Arena testArena = Arena.ofConfined()) {
            MemorySegment streamSegment = testArena.allocate(56, 8);
            CommandStreamWriter streamWriter =
                    new CommandStreamWriter(streamSegment, 0, s_goldenGraphHash);
            streamWriter.appendDispatchIndirect(4, 12);
            assertEquals(56, streamWriter.finish());
            assertArrayEquals(expectedBytes.array(),
                    streamSegment.toArray(ValueLayout.JAVA_BYTE));
        }
    }

    @Test
    void imageCommandsUseThePinnedPayloadLayouts() {
        // ClearColorImage (56B) followed by CopyImageToBuffer (64B): 32 + 120 = 152.
        ByteBuffer expectedBytes = ByteBuffer.allocate(152).order(ByteOrder.LITTLE_ENDIAN);
        expectedBytes.put(new byte[] {0x42, 0x45, 0x43, 0x53});
        expectedBytes.putShort(CommandStreamFormat.s_currentVersionMajor);
        expectedBytes.putShort(CommandStreamFormat.s_currentVersionMinor);
        expectedBytes.putInt(0);
        expectedBytes.putInt(2);
        expectedBytes.putLong(152);
        expectedBytes.putLong(s_goldenGraphHash);
        expectedBytes.putShort((short) 0x0043).putShort((short) 0).putInt(56);
        expectedBytes.putInt(0).putInt(6);
        expectedBytes.putFloat(1.0f).putFloat(0.0f).putFloat(1.0f).putFloat(1.0f);
        expectedBytes.putInt(CommandStreamImageAspect.s_color).putInt(0)
                .putInt(CommandStreamBarrierBatchTableWriter.s_remainingCount).putInt(0)
                .putInt(CommandStreamBarrierBatchTableWriter.s_remainingCount).putInt(0);
        expectedBytes.putShort((short) 0x0047).putShort((short) 0).putInt(64);
        expectedBytes.putInt(0).putInt(1).putInt(5)
                .putInt(CommandStreamImageAspect.s_color).putInt(0).putInt(0).putInt(1)
                .putInt(0);
        expectedBytes.putLong(0);
        expectedBytes.putInt(8).putInt(8).putInt(1).putInt(0);

        try (Arena testArena = Arena.ofConfined()) {
            MemorySegment streamSegment = testArena.allocate(152, 8);
            CommandStreamWriter streamWriter =
                    new CommandStreamWriter(streamSegment, 0, s_goldenGraphHash);
            streamWriter.appendClearColorImage(0,
                    CommandStreamImageLayout.TRANSFER_DESTINATION, 1.0f, 0.0f, 1.0f, 1.0f,
                    CommandStreamImageAspect.s_color, 0,
                    CommandStreamBarrierBatchTableWriter.s_remainingCount, 0,
                    CommandStreamBarrierBatchTableWriter.s_remainingCount);
            streamWriter.appendCopyImageToBuffer(0, 1,
                    CommandStreamImageLayout.TRANSFER_SOURCE,
                    CommandStreamImageAspect.s_color, 0, 0, 1, 0, 8, 8, 1);
            assertEquals(152, streamWriter.finish());
            assertArrayEquals(expectedBytes.array(),
                    streamSegment.toArray(ValueLayout.JAVA_BYTE));
        }
    }

    @Test
    void writerRejectsUseAfterFinish() {
        try (Arena testArena = Arena.ofConfined()) {
            MemorySegment streamSegment = testArena.allocate(80, 8);
            CommandStreamWriter streamWriter =
                    new CommandStreamWriter(streamSegment, 0, s_goldenGraphHash);
            streamWriter.finish();

            assertThrows(IllegalStateException.class, () -> streamWriter.appendDraw(1, 1, 0, 0));
            assertThrows(IllegalStateException.class, streamWriter::finish);
        }
    }
}
