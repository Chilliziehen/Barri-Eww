package barrieww.core.interoperability;

/**
 * @note ThreadSafety: Immutable value; safe to share across threads.
 * The Java-owned Vulkan bootstrap handles (encoded as their native pointer values) that a
 * Native presentation runtime borrows (ADR-0004). Java retains ownership and must keep every
 * handle valid until the runtime is closed.
 * @warning MemoryOwnership: Java owns and destroys these Vulkan objects; Native only borrows
 * them for the runtime's lifetime and destroys none of them.
 */
public record PresentationBootstrapHandles(
        long instanceHandle,
        long physicalDeviceHandle,
        long logicalDeviceHandle,
        long surfaceHandle,
        long graphicsQueueHandle,
        long presentQueueHandle,
        int graphicsQueueFamilyIndex,
        int presentQueueFamilyIndex) {
}
