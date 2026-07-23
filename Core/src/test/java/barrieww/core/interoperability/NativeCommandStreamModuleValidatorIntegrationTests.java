package barrieww.core.interoperability;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.nio.file.Path;
import org.junit.jupiter.api.Tag;
import org.junit.jupiter.api.Test;

/**
 * @note ThreadSafety: JUnit creates a fresh instance per test method; no shared state.
 * Covers Native validator lifecycle and Java-side FFM argument ownership checks.
 */
@Tag("nativeIntegration")
class NativeCommandStreamModuleValidatorIntegrationTests {

    @Test
    void validatorRejectsHeapMemoryBeforeDowncall() throws Exception {
        try (NativeCommandStreamModuleValidator validator = openValidator()) {
            assertThrows(IllegalArgumentException.class,
                    () -> validator.validate(MemorySegment.ofArray(new byte[32])));
        }
    }

    @Test
    void validatorRejectsMisalignedNativeMemoryBeforeDowncall() throws Exception {
        try (Arena arena = Arena.ofConfined();
             NativeCommandStreamModuleValidator validator = openValidator()) {
            MemorySegment allocation = arena.allocate(40, 8);
            assertThrows(IllegalArgumentException.class,
                    () -> validator.validate(allocation.asSlice(1, 32)));
        }
    }

    @Test
    void validatorRejectsUseAfterClose() throws Exception {
        NativeCommandStreamModuleValidator validator = openValidator();
        validator.close();
        validator.close();
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment moduleSegment = arena.allocate(32, 8);
            assertThrows(IllegalStateException.class,
                    () -> validator.validate(moduleSegment));
        }
    }

    @Test
    void validationExceptionCarriesOptionalNestedContext() {
        CommandStreamModuleValidationException validationException =
                new CommandStreamModuleValidationException(
                        CommandStreamModuleValidationError.LANE_STREAM_INVALID, 224,
                        CommandStreamValidationError.INVALID_MAGIC, 0L);
        assertEquals(CommandStreamModuleValidationError.LANE_STREAM_INVALID,
                validationException.moduleValidationError());
        assertEquals(224, validationException.moduleByteOffset());
        assertEquals(CommandStreamValidationError.INVALID_MAGIC,
                validationException.laneStreamValidationError().orElseThrow());
        assertTrue(validationException.laneStreamByteOffset().isPresent());
        assertEquals(0, validationException.laneStreamByteOffset().orElseThrow());

        CommandStreamModuleValidationException moduleOnlyException =
                new CommandStreamModuleValidationException(
                        CommandStreamModuleValidationError.INVALID_MODULE_MAGIC,
                        0, null, null);
        assertFalse(moduleOnlyException.laneStreamValidationError().isPresent());
        assertFalse(moduleOnlyException.laneStreamByteOffset().isPresent());
    }

    private static NativeCommandStreamModuleValidator openValidator()
            throws NativeLibraryLoadingException {
        return NativeCommandStreamModuleValidator.open(Path.of(
                System.getProperty("barrieww.nativeLibraryPath")));
    }
}
