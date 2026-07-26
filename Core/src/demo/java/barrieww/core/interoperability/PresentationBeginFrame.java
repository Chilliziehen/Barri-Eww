package barrieww.core.interoperability;

import java.util.Optional;

/**
 * @note ThreadSafety: Immutable value; safe to share across threads.
 * The result of beginning one presentation frame: the frame status, the acquired slot/image
 * identity, and the completed metrics of the prior frame that reused this slot (when valid).
 */
public record PresentationBeginFrame(
        PresentationFrameStatus status,
        int frameSlotIndex,
        int imageIndex,
        long frameSequence,
        long swapchainGeneration,
        Optional<PresentationFrameMetrics> priorMetrics) {
}
