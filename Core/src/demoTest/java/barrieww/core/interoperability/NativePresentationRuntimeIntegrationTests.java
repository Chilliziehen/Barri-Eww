package barrieww.core.interoperability;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.Linker;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.SymbolLookup;
import java.lang.foreign.ValueLayout;
import java.lang.invoke.MethodHandle;
import java.nio.file.Path;
import org.junit.jupiter.api.Tag;
import org.junit.jupiter.api.Test;

/**
 * @note ThreadSafety: JUnit creates a fresh instance per test method; no shared state.
 * Drives the real presentation runtime FFM downcalls with rejected arguments, exercising
 * symbol resolution and plain-data marshalling without a live window or surface.
 */
@Tag("nativeIntegration")
class NativePresentationRuntimeIntegrationTests {

    @Test
    void submitAndPresentClearFrameRejectsMissingRuntimeAndClearsOutput() throws Throwable {
        Path nativeLibraryPath = Path.of(
                System.getProperty("barrieww.nativeLibraryPath"));
        try (Arena libraryArena = Arena.ofConfined()) {
            SymbolLookup symbolLookup = SymbolLookup.libraryLookup(
                    nativeLibraryPath, libraryArena);
            MemorySegment symbol = symbolLookup.find(
                    NativePresentationRuntime.s_submitAndPresentClearFrameSymbolName)
                    .orElseThrow();
            FunctionDescriptor descriptor = FunctionDescriptor.of(
                    ValueLayout.JAVA_INT, ValueLayout.JAVA_LONG, ValueLayout.JAVA_FLOAT,
                    ValueLayout.JAVA_FLOAT, ValueLayout.JAVA_FLOAT, ValueLayout.ADDRESS);
            MethodHandle handle = Linker.nativeLinker().downcallHandle(symbol, descriptor);
            MemorySegment submitResult = libraryArena.allocate(8, 4);
            submitResult.set(ValueLayout.JAVA_LONG, 0, -1L);

            int operationResult = (int) handle.invokeExact(
                    0L, 0.1f, 0.2f, 0.3f, submitResult);

            assertEquals(1, operationResult);
            assertEquals(0L, submitResult.get(ValueLayout.JAVA_LONG, 0));
        }
    }

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
