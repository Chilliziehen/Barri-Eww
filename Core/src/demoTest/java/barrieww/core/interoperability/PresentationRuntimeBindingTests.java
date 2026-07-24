package barrieww.core.interoperability;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

import java.nio.file.Path;
import org.junit.jupiter.api.Test;

/**
 * @note ThreadSafety: JUnit creates a fresh instance per test method; no shared state.
 * Covers presentation runtime binding argument validation that needs no live library.
 */
class PresentationRuntimeBindingTests {

    @Test
    void createRejectsARelativeLibraryPath() {
        PresentationBootstrapHandles handles = new PresentationBootstrapHandles(
                1L, 2L, 3L, 4L, 5L, 5L, 0, 0);
        NativeLibraryLoadingException loadingException = assertThrows(
                NativeLibraryLoadingException.class,
                () -> NativePresentationRuntime.create(Path.of("BarriEwwNativeFfm"), handles,
                        1280, 720, 2));
        assertEquals(NativePresentationRuntime.s_createSymbolName,
                loadingException.nativeSymbolName());
    }
}
