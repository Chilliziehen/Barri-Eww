package barrieww.mod;

import barrieww.core.interoperability.NativeLibraryLoadingException;
import barrieww.core.interoperability.NativePresentationRuntime;
import barrieww.core.interoperability.NativePresentationRuntimeException;
import barrieww.core.interoperability.PresentationBootstrapHandles;
import java.nio.file.Path;
import java.util.Objects;

/**
 * @note ThreadSafety: Immutable factory; creation is confined by the caller to the render thread.
 * Owns the normalized absolute Native library path and creates thin Core runtime adapters.
 * @warning MemoryOwnership: The path is an immutable value; each returned adapter exclusively owns
 * its Core runtime while all Vulkan bootstrap handles remain borrowed.
 */
public final class CorePresentationRuntimeFactory implements PresentationRuntimeFactory {
    private final Path m_nativeLibraryPath;

    /**
     * @note ThreadSafety: Construction publishes immutable normalized path state.
     * Captures an owned normalized absolute representation of the Native library path.
     *
     * @param Path nativeLibraryPath Native presentation shared-library path
     * @warning MemoryOwnership: Path is immutable; the normalized absolute value is retained.
     */
    public CorePresentationRuntimeFactory(Path nativeLibraryPath) {
        m_nativeLibraryPath = Objects.requireNonNull(
            nativeLibraryPath, "nativeLibraryPath").toAbsolutePath().normalize();
    }

    /**
     * @note ThreadSafety: Called serially on the render thread that owns the new runtime.
     * Invokes Core creation with the owned absolute path and wraps the resulting runtime.
     *
     * @param PresentationBootstrapHandles bootstrapHandles Borrowed validated Vulkan handles
     * @param int framebufferWidth Positive framebuffer width in pixels
     * @param int framebufferHeight Positive framebuffer height in pixels
     * @param int framesInFlightCount Positive number of in-flight frame slots
     * @return PresentationRuntime Exclusively owned Core adapter
     * @throws NativeLibraryLoadingException Exact checked Core loading failure
     * @throws NativePresentationRuntimeException Exact checked Core runtime creation failure
     * @warning MemoryOwnership: The adapter owns Core and Native resources; handles remain borrowed.
     */
    @Override
    public PresentationRuntime create(
        PresentationBootstrapHandles bootstrapHandles,
        int framebufferWidth,
        int framebufferHeight,
        int framesInFlightCount)
        throws NativeLibraryLoadingException, NativePresentationRuntimeException {
        return new CorePresentationRuntime(NativePresentationRuntime.create(
            m_nativeLibraryPath,
            bootstrapHandles,
            framebufferWidth,
            framebufferHeight,
            framesInFlightCount));
    }
}
