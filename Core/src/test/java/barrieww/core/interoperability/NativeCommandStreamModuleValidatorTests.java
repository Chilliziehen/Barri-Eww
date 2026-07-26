package barrieww.core.interoperability;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

import java.nio.file.Path;
import org.junit.jupiter.api.Test;

/**
 * @note ThreadSafety: JUnit creates a fresh instance per test method; no shared state.
 * Covers stable error-code mirrors and library-path failures without loading Native.
 */
class NativeCommandStreamModuleValidatorTests {

    @Test
    void everyModuleValidationErrorRoundTripsItsStableCode() {
        for (CommandStreamModuleValidationError validationError
                : CommandStreamModuleValidationError.values()) {
            assertEquals(validationError,
                    CommandStreamModuleValidationError.fromCode(validationError.code()));
        }
        assertThrows(IllegalArgumentException.class,
                () -> CommandStreamModuleValidationError.fromCode(0));
        assertThrows(IllegalArgumentException.class,
                () -> CommandStreamModuleValidationError.fromCode(17));
    }

    @Test
    void everyLaneValidationErrorRoundTripsItsStableCode() {
        for (CommandStreamValidationError validationError
                : CommandStreamValidationError.values()) {
            assertEquals(validationError,
                    CommandStreamValidationError.fromCode(validationError.code()));
        }
        assertThrows(IllegalArgumentException.class,
                () -> CommandStreamValidationError.fromCode(0));
        assertThrows(IllegalArgumentException.class,
                () -> CommandStreamValidationError.fromCode(11));
    }

    @Test
    void validatorRequiresAnAbsoluteLibraryPath() {
        NativeLibraryLoadingException loadingException = assertThrows(
                NativeLibraryLoadingException.class,
                () -> NativeCommandStreamModuleValidator.open(
                        Path.of("BarriEwwNativeFfm")));
        assertEquals(NativeCommandStreamModuleValidator.s_nativeSymbolName,
                loadingException.nativeSymbolName());
        assertEquals(Path.of("BarriEwwNativeFfm").toAbsolutePath().normalize(),
                loadingException.nativeLibraryPath());
    }
}
