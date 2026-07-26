package barrieww.core.interoperability;

import java.util.Optional;

/**
 * @note ThreadSafety: Immutable value; safe to share across threads.
 * The result of rendering one clear frame: the begin-frame status/identity, the submit-and-
 * present status, and the completed metrics of the prior frame that reused this slot.
 */
public record PresentationClearFrame(
        PresentationFrameStatus beginStatus,
        PresentationFrameStatus submitStatus,
        int frameSlotIndex,
        int imageIndex,
        long frameSequence,
        long swapchainGeneration,
        Optional<PresentationFrameMetrics> priorMetrics) {
}
