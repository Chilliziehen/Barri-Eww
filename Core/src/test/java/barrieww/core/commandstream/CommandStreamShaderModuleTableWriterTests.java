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
 * Byte-exact tests of CommandStreamShaderModuleTableWriter against the schema v0.1
 * layout (mirrored by the native validator), a composition test over the committed
 * SPIR-V fixtures that the native GPU tests consume (cross-language tie without a
 * separate golden), and the fail-fast authoring rules.
 */
class CommandStreamShaderModuleTableWriterTests {

    /** A minimal fake SPIR-V blob: magic, version, generator, bound, schema (+ extras). */
    private static byte[] makeFakeSpirvBlob(int totalByteCount) {
        ByteBuffer blobBytes =
                ByteBuffer.allocate(totalByteCount).order(ByteOrder.LITTLE_ENDIAN);
        blobBytes.putInt(0x07230203);
        while (blobBytes.remaining() > 0) {
            blobBytes.putInt(0);
        }
        return blobBytes.array();
    }

    @Test
    void tableWriterProducesTheExactWireLayout() {
        CommandStreamShaderModuleTableWriter tableWriter =
                new CommandStreamShaderModuleTableWriter();
        assertEquals(0, tableWriter.addShaderBlob(
                MemorySegment.ofArray(makeFakeSpirvBlob(20))));
        assertEquals(1, tableWriter.addShaderBlob(
                MemorySegment.ofArray(makeFakeSpirvBlob(24))));
        // Directory ends at 40; blob 0 at 40 (20 bytes, padded to 64); blob 1 at 64.
        assertEquals(88, tableWriter.requiredByteSize());

        ByteBuffer expectedBytes = ByteBuffer.allocate(88).order(ByteOrder.LITTLE_ENDIAN);
        expectedBytes.putInt(2).putInt(0);
        expectedBytes.putLong(40).putLong(20);
        expectedBytes.putLong(64).putLong(24);
        expectedBytes.put(makeFakeSpirvBlob(20));
        expectedBytes.putInt(0); // alignment padding to 64
        expectedBytes.put(makeFakeSpirvBlob(24));

        try (Arena testArena = Arena.ofConfined()) {
            MemorySegment tableSegment = testArena.allocate(88, 8);
            assertEquals(88, tableWriter.writeTo(tableSegment));
            assertArrayEquals(expectedBytes.array(),
                    tableSegment.toArray(ValueLayout.JAVA_BYTE));
        }
    }

    @Test
    void tableWriterLaysOutTheCommittedFixturesLikeTheNativeConsumer() throws Exception {
        // The same two fixtures the native GPU tests build tables from by hand;
        // identical add order gives the identical layout the recorder executed.
        Path shaderDirectoryPath =
                Path.of(System.getProperty("barrieww.testDataDirectory"), "Shaders");
        byte[] argumentBlobBytes = Files.readAllBytes(
                shaderDirectoryPath.resolve("WriteDispatchArguments.comp.spv"));
        byte[] patternBlobBytes = Files.readAllBytes(
                shaderDirectoryPath.resolve("WritePatternByDeviceAddress.comp.spv"));

        CommandStreamShaderModuleTableWriter tableWriter =
                new CommandStreamShaderModuleTableWriter();
        tableWriter.addShaderBlob(MemorySegment.ofArray(argumentBlobBytes));
        tableWriter.addShaderBlob(MemorySegment.ofArray(patternBlobBytes));

        try (Arena testArena = Arena.ofConfined()) {
            long tableByteSize = tableWriter.requiredByteSize();
            MemorySegment tableSegment = testArena.allocate(tableByteSize, 8);
            assertEquals(tableByteSize, tableWriter.writeTo(tableSegment));

            ByteBuffer writtenBytes = ByteBuffer.wrap(
                            tableSegment.toArray(ValueLayout.JAVA_BYTE))
                    .order(ByteOrder.LITTLE_ENDIAN);
            assertEquals(2, writtenBytes.getInt(0));
            long firstBlobOffset = writtenBytes.getLong(8);
            assertEquals(40, firstBlobOffset);
            assertEquals(argumentBlobBytes.length, writtenBytes.getLong(16));
            assertEquals(patternBlobBytes.length, writtenBytes.getLong(32));
            // Both embedded blobs still start with the SPIR-V magic word.
            assertEquals(0x07230203, writtenBytes.getInt((int) firstBlobOffset));
            assertEquals(0x07230203, writtenBytes.getInt((int) writtenBytes.getLong(24)));
        }
    }

    @Test
    void tableWriterRejectsMalformedBlobs() {
        CommandStreamShaderModuleTableWriter tableWriter =
                new CommandStreamShaderModuleTableWriter();

        // Below the SPIR-V minimum size.
        assertThrows(IllegalArgumentException.class,
                () -> tableWriter.addShaderBlob(MemorySegment.ofArray(new byte[16])));
        // Not a multiple of 4.
        assertThrows(IllegalArgumentException.class,
                () -> tableWriter.addShaderBlob(MemorySegment.ofArray(new byte[22])));
        // Correct size but wrong magic word.
        assertThrows(IllegalArgumentException.class,
                () -> tableWriter.addShaderBlob(MemorySegment.ofArray(new byte[20])));
    }
}
