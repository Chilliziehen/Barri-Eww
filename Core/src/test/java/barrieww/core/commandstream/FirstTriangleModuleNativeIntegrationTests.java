package barrieww.core.commandstream;

import static org.junit.jupiter.api.Assertions.assertArrayEquals;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

import barrieww.core.interoperability.CommandStreamModuleValidationError;
import barrieww.core.interoperability.CommandStreamModuleValidationException;
import barrieww.core.interoperability.CommandStreamValidationError;
import barrieww.core.interoperability.NativeCommandStreamModuleValidator;
import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.nio.ByteOrder;
import java.nio.file.Files;
import java.nio.file.Path;
import org.junit.jupiter.api.Tag;
import org.junit.jupiter.api.Test;

/**
 * @note ThreadSafety: JUnit creates a fresh instance per test method; no shared state.
 * Exercises the real Java writer to Panama to Native Version 1 validation chain.
 */
@Tag("nativeIntegration")
class FirstTriangleModuleNativeIntegrationTests {

    @Test
    void javaOwnedFirstTriangleModuleValidatesThroughNativeFfm() throws Exception {
        Path goldenFilePath = Path.of(System.getProperty("barrieww.testDataDirectory"),
                "CommandStream", "FirstTriangleModule.becs");
        byte[] goldenBytes = Files.readAllBytes(goldenFilePath);
        Path nativeLibraryPath = Path.of(
                System.getProperty("barrieww.nativeLibraryPath"));

        try (Arena moduleArena = Arena.ofConfined()) {
            MemorySegment moduleSegment =
                    FirstTriangleModuleGoldenTests.writeGoldenModule(moduleArena);
            assertArrayEquals(goldenBytes, moduleSegment.toArray(ValueLayout.JAVA_BYTE));
            try (NativeCommandStreamModuleValidator validator =
                         NativeCommandStreamModuleValidator.open(nativeLibraryPath)) {
                validator.validate(moduleSegment);
            }
        }
    }

    @Test
    void nestedLaneValidationFailurePreservesBothOffsetDomains() throws Exception {
        Path nativeLibraryPath = Path.of(
                System.getProperty("barrieww.nativeLibraryPath"));
        try (Arena moduleArena = Arena.ofConfined()) {
            MemorySegment moduleSegment =
                    FirstTriangleModuleGoldenTests.writeGoldenModule(moduleArena);
            long laneStreamByteOffset = findLaneStreamByteOffset(moduleSegment);
            moduleSegment.set(ValueLayout.JAVA_BYTE, laneStreamByteOffset, (byte) 'X');
            try (NativeCommandStreamModuleValidator validator =
                         NativeCommandStreamModuleValidator.open(nativeLibraryPath)) {
                CommandStreamModuleValidationException validationException = assertThrows(
                        CommandStreamModuleValidationException.class,
                        () -> validator.validate(moduleSegment));
                assertEquals(CommandStreamModuleValidationError.LANE_STREAM_INVALID,
                        validationException.moduleValidationError());
                assertEquals(laneStreamByteOffset,
                        validationException.moduleByteOffset());
                assertEquals(CommandStreamValidationError.INVALID_MAGIC,
                        validationException.laneStreamValidationError().orElseThrow());
                assertEquals(0, validationException.laneStreamByteOffset().orElseThrow());
            }
        }
    }

    /** Returns the first lane section's module-relative byte offset. */
    private static long findLaneStreamByteOffset(MemorySegment moduleSegment) {
        ValueLayout.OfInt littleEndianInt = ValueLayout.JAVA_INT.withOrder(ByteOrder.LITTLE_ENDIAN);
        ValueLayout.OfShort littleEndianShort =
                ValueLayout.JAVA_SHORT.withOrder(ByteOrder.LITTLE_ENDIAN);
        ValueLayout.OfLong littleEndianLong =
                ValueLayout.JAVA_LONG.withOrder(ByteOrder.LITTLE_ENDIAN);
        int sectionCount = moduleSegment.get(littleEndianInt, 8);
        for (int sectionEntryIndex = 0;
             sectionEntryIndex < sectionCount;
             ++sectionEntryIndex) {
            long sectionEntryByteOffset = CommandStreamModuleFormat.s_moduleHeaderByteSize
                    + (long) sectionEntryIndex * CommandStreamModuleFormat.s_sectionEntryByteSize;
            int sectionTypeValue = Short.toUnsignedInt(
                    moduleSegment.get(littleEndianShort, sectionEntryByteOffset));
            if (sectionTypeValue
                    == CommandStreamModuleSectionType.LANE_STREAM.rawSectionTypeValue()) {
                return moduleSegment.get(littleEndianLong,
                        sectionEntryByteOffset + 8);
            }
        }
        throw new IllegalStateException("First-triangle module has no lane stream");
    }
}
