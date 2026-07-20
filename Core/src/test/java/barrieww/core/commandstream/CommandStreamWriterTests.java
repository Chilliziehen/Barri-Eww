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
