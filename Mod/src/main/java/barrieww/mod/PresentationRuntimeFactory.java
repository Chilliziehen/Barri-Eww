package barrieww.mod;

import barrieww.core.interoperability.NativeLibraryLoadingException;
import barrieww.core.interoperability.NativePresentationRuntimeException;
import barrieww.core.interoperability.HostImagePresentationBinding;
import barrieww.core.interoperability.PresentationBootstrapHandles;

/**
 * @note ThreadSafety: Implementations are invoked serially on the render thread.
 * Creates one host-neutral presentation runtime for each ready surface generation.
 * @warning MemoryOwnership: The returned runtime exclusively owns Native presentation resources;
 * bootstrap handles remain borrowed from the caller until that runtime is closed.
 */
public interface PresentationRuntimeFactory {
    /**
     * @note ThreadSafety: Called serially on the render thread that will own the returned runtime.
     * Creates one runtime from validated bootstrap handles and a positive initial extent.
     *
     * @param PresentationBootstrapHandles bootstrapHandles Borrowed validated Vulkan handles
     * @param int framebufferWidth Positive framebuffer width in pixels
     * @param int framebufferHeight Positive framebuffer height in pixels
     * @param int framesInFlightCount Positive number of in-flight frame slots
     * @param HostImagePresentationBinding hostImageBinding Borrowed exact-extent host image binding
     * @return PresentationRuntime Exclusively owned open presentation runtime
     * @throws NativeLibraryLoadingException When the explicit Native library or symbols fail to load
     * @throws NativePresentationRuntimeException When Native runtime creation fails
     * @warning MemoryOwnership: The returned runtime owns Native resources and borrows all handles.
     */
    PresentationRuntime create(
        PresentationBootstrapHandles bootstrapHandles,
        int framebufferWidth,
        int framebufferHeight,
        int framesInFlightCount,
        HostImagePresentationBinding hostImageBinding)
        throws NativeLibraryLoadingException, NativePresentationRuntimeException;
}
