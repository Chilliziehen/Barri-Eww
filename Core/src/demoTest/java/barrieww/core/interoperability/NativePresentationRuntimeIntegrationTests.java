package barrieww.core.interoperability;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

import java.nio.file.Path;
import org.junit.jupiter.api.Tag;
import org.junit.jupiter.api.Test;

/**
 * @note ThreadSafety: JUnit creates a fresh instance per test method; no shared state.
 * Drives the real presentation runtime FFM downcalls with rejected arguments, exercising
 * symbol resolution and POD marshalling without a live window or surface.
 */
@Tag("nativeIntegration")
class NativePresentationRuntimeIntegrationTests {

    @Test
    void createRejectsMissingBootstrapHandlesThroughTheRealDowncall() {
        Path nativeLibraryPath = Path.of(
                System.getProperty("barrieww.nativeLibraryPath"));
        PresentationBootstrapHandles missingHandles = new PresentationBootstrapHandles(
                0L, 0L, 0L, 0L, 0L, 0L, 0, 0);

        NativePresentationRuntimeException runtimeException = assertThrows(
                NativePresentationRuntimeException.class,
                () -> NativePresentationRuntime.create(nativeLibraryPath, missingHandles,
                        1280, 720, 2));
        // 1 == NativePresentationRuntimeOperationResult::InvalidArgument.
        assertEquals(1, runtimeException.operationResultCode());
        assertEquals(NativePresentationRuntime.s_createSymbolName,
                runtimeException.nativeSymbolName());
    }
}
