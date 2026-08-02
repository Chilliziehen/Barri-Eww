package barrieww.core.interoperability;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.Linker;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.SymbolLookup;
import java.lang.foreign.ValueLayout;
import java.lang.invoke.MethodHandle;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
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
    void createHostImagePresentationRejectsMissingBootstrapHandles() {
        Path nativeLibraryPath = Path.of(
                System.getProperty("barrieww.nativeLibraryPath"));
        PresentationBootstrapHandles missingHandles = new PresentationBootstrapHandles(
                0L, 0L, 0L, 0L, 0L, 0L, 0, 0);
        HostImagePresentationBinding hostImageBinding = HostImagePresentationBinding.create(
                1L, PresentationImageFormat.R8G8B8A8_UNORM,
                PresentationImageFormat.B8G8R8A8_UNORM, 1280, 720, 1280, 720);

        NativePresentationRuntimeException runtimeException = assertThrows(
                NativePresentationRuntimeException.class,
                () -> NativePresentationRuntime.createHostImagePresentation(
                        nativeLibraryPath, missingHandles, 1280, 720, 2, hostImageBinding));

        assertEquals(1, runtimeException.operationResultCode());
        assertEquals(0, runtimeException.vulkanResult());
        assertEquals(NativePresentationRuntime.s_createHostImageSymbolName,
                runtimeException.nativeSymbolName());
    }

    @Test
    void rejectedHostImageCreateClosesLookupArenaAndUnloadsCopiedLibrary() throws Exception {
        Path originalLibraryPath = Path.of(
                System.getProperty("barrieww.nativeLibraryPath"));
        Path temporaryDirectory = Files.createTempDirectory("barrieww-host-image-arena-");
        Path copiedLibraryPath = temporaryDirectory.resolve(originalLibraryPath.getFileName());
        try {
            Files.copy(originalLibraryPath, copiedLibraryPath,
                    StandardCopyOption.REPLACE_EXISTING);
            Arena libraryArena = Arena.ofConfined();
            PresentationBootstrapHandles missingHandles = new PresentationBootstrapHandles(
                    0L, 0L, 0L, 0L, 0L, 0L, 0, 0);
            HostImagePresentationBinding hostImageBinding = HostImagePresentationBinding.create(
                    1L, PresentationImageFormat.R8G8B8A8_UNORM,
                    PresentationImageFormat.B8G8R8A8_UNORM, 1280, 720, 1280, 720);
            try {
                assertThrows(NativePresentationRuntimeException.class,
                        () -> NativePresentationRuntime
                                .createHostImagePresentationWithLibraryArena(
                                        copiedLibraryPath, missingHandles, 1280, 720, 2,
                                        hostImageBinding, libraryArena));
                assertFalse(libraryArena.scope().isAlive());

                if (System.getProperty("os.name").startsWith("Windows")) {
                    Files.delete(copiedLibraryPath);
                    assertFalse(Files.exists(copiedLibraryPath));
                    Files.copy(originalLibraryPath, copiedLibraryPath,
                            StandardCopyOption.REPLACE_EXISTING);
                    assertTrue(Files.isRegularFile(copiedLibraryPath));
                }
            } finally {
                if (libraryArena.scope().isAlive()) {
                    libraryArena.close();
                }
            }
        } finally {
            Files.deleteIfExists(copiedLibraryPath);
            Files.deleteIfExists(temporaryDirectory);
        }
    }

    @Test
    void detachHostImagePresentationRejectsMissingRuntimeAndClearsOutput() throws Throwable {
        assertRejectedRuntimeClearsOutput(
                NativePresentationRuntime.s_detachHostImageResourcesSymbolName,
                NativePresentationRuntime.s_detachHostImageResourcesDescriptor);
    }

    @Test
    void submitAndPresentHostImageFrameRejectsMissingRuntimeAndClearsOutput() throws Throwable {
        assertRejectedRuntimeClearsOutput(
                NativePresentationRuntime.s_submitAndPresentHostImageFrameSymbolName,
                NativePresentationRuntime.s_submitAndPresentHostImageFrameDescriptor);
    }

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

    /**
     * @note ThreadSafety: Test-confined; one invocation uses one confined Arena.
     * Invokes a runtime-address/output-record boundary with a zero runtime and verifies stable
     * rejection plus boundary-first output clearing.
     *
     * @param String symbolName Exact Version 1 symbol name
     * @param FunctionDescriptor descriptor Exact non-critical downcall descriptor
     * @throws Throwable When symbol resolution or downcall invocation fails unexpectedly
     * @warning MemoryOwnership: This method owns and closes the lookup Arena and output segment;
     *          Native writes synchronously and retains no address.
     */
    private static void assertRejectedRuntimeClearsOutput(
            String symbolName, FunctionDescriptor descriptor) throws Throwable {
        Path nativeLibraryPath = Path.of(
                System.getProperty("barrieww.nativeLibraryPath"));
        try (Arena libraryArena = Arena.ofConfined()) {
            SymbolLookup symbolLookup = SymbolLookup.libraryLookup(
                    nativeLibraryPath, libraryArena);
            MemorySegment symbol = symbolLookup.find(symbolName).orElseThrow();
            MethodHandle handle = Linker.nativeLinker().downcallHandle(symbol, descriptor);
            MemorySegment output = libraryArena.allocate(8, 4);
            output.set(ValueLayout.JAVA_LONG, 0, -1L);

            int operationResult = (int) handle.invokeExact(0L, output);

            assertEquals(1, operationResult);
            assertEquals(0L, output.get(ValueLayout.JAVA_LONG, 0));
        }
    }
}
