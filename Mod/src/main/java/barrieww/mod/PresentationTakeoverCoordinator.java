package barrieww.mod;

import barrieww.core.interoperability.NativeLibraryLoadingException;
import barrieww.core.interoperability.NativePresentationRuntimeException;
import barrieww.core.interoperability.PresentationFrameStatus;
import java.util.Objects;
import org.slf4j.Logger;

/**
 * @note ThreadSafety: Render-thread-confined. Configure, frame and close calls must be serialized
 * on one surface's render thread.
 * Owns the presentation runtime and generation transition policy without host implementation types.
 * @warning MemoryOwnership: The coordinator exclusively owns its current PresentationRuntime and
 * closes it before replacement, fallback or coordinator teardown.
 */
public final class PresentationTakeoverCoordinator implements AutoCloseable {
    private static final float s_clearRed = 0.08f;
    private static final float s_clearGreen = 0.72f;
    private static final float s_clearBlue = 0.93f;
    private static final int s_framesInFlightCount = 2;

    private final PresentationRuntimeFactory m_runtimeFactory;
    private final Logger m_logger;
    private PresentationRuntime m_runtime;
    private int m_framebufferWidth;
    private int m_framebufferHeight;
    private boolean m_isTakenOver;
    private boolean m_isFrameOpen;
    private boolean m_requiresReconfiguration;
    private boolean m_isTakeoverPermanentlyDisabled;
    private boolean m_isGenerationFatal;
    private boolean m_isTerminallyIntercepting;
    private boolean m_areHostImagePresentationResourcesAttached;
    private NativePresentationRuntimeException m_primaryFrameFailure;
    private NativePresentationRuntimeException m_hostResourceDetachFailure;

    /**
     * @note ThreadSafety: Construction and all subsequent use occur on one render thread.
     * Creates one generation coordinator with injected host-neutral creation and failure reporting.
     *
     * @param PresentationRuntimeFactory runtimeFactory Factory for each ready generation
     * @param Logger logger Slow-path checked-failure logger
     * @warning MemoryOwnership: The factory and logger remain caller-owned; created runtimes transfer
     * ownership to this coordinator.
     */
    public PresentationTakeoverCoordinator(
        PresentationRuntimeFactory runtimeFactory,
        Logger logger) {
        m_runtimeFactory = Objects.requireNonNull(runtimeFactory, "runtimeFactory");
        m_logger = Objects.requireNonNull(logger, "logger");
    }

    /**
     * @note ThreadSafety: Render-thread-confined and non-reentrant.
     * Drains the preceding runtime, then creates and primes a candidate with one presented color
     * frame before publishing takeover. A false return commits the current configure call to
     * vanilla presentation. If runtime destruction fails, true preserves terminal interception
     * because creating a second swapchain while Native ownership may remain active is unsafe.
     *
     * @param PresentationGenerationPreparation generationPreparation Post-retirement input callback,
     * or null when unavailable
     * @return boolean True after successful replacement or while terminal interception must prevent
     * a second swapchain; false when the current configure must proceed with vanilla presentation
     * @warning MemoryOwnership: Closes any preceding runtime before candidate creation. Candidate
     * ownership transfers only after priming; rejection closes it before vanilla presentation.
     */
    public boolean configure(PresentationGenerationPreparation generationPreparation) {
        m_isFrameOpen = false;
        if (m_isTerminallyIntercepting) {
            m_requiresReconfiguration = false;
            return true;
        }

        PresentationRuntime oldRuntime = m_runtime;
        if (oldRuntime != null) {
            try {
                oldRuntime.close();
            } catch (NativePresentationRuntimeException closeFailure) {
                addRetainedGenerationFailures(
                    closeFailure,
                    m_primaryFrameFailure,
                    m_hostResourceDetachFailure);
                m_runtime = null;
                m_isTakeoverPermanentlyDisabled = true;
                m_isTakenOver = true;
                m_requiresReconfiguration = false;
                m_isTerminallyIntercepting = true;
                logRuntimeFailure("Presentation runtime generation drain failed", closeFailure);
                return true;
            }
            m_runtime = null;
        }

        m_isTakenOver = false;
        m_requiresReconfiguration = false;
        m_isGenerationFatal = false;
        m_primaryFrameFailure = null;
        m_hostResourceDetachFailure = null;
        m_areHostImagePresentationResourcesAttached = false;

        if (m_isTakeoverPermanentlyDisabled) {
            return false;
        }
        PresentationGenerationInputs inputs;
        try {
            inputs = generationPreparation == null ? null : generationPreparation.prepare();
        } catch (NativePresentationRuntimeException preparationFailure) {
            m_isTakeoverPermanentlyDisabled = true;
            logRuntimeFailure("Presentation generation preparation failed", preparationFailure);
            return false;
        } catch (RuntimeException preparationFailure) {
            m_isTakeoverPermanentlyDisabled = true;
            m_logger.error("Presentation generation preparation failed", preparationFailure);
            return false;
        }
        if (inputs == null || !inputs.isReady()) {
            m_isTakeoverPermanentlyDisabled = true;
            return false;
        }

        PresentationRuntime candidateRuntime;
        try {
            candidateRuntime = m_runtimeFactory.create(
                inputs.bootstrapHandles(),
                inputs.framebufferWidth(),
                inputs.framebufferHeight(),
                s_framesInFlightCount,
                inputs.hostImageBinding());
        } catch (NativeLibraryLoadingException loadingFailure) {
            m_isTakeoverPermanentlyDisabled = true;
            logLoadingFailure("Presentation runtime creation failed", loadingFailure);
            return false;
        } catch (NativePresentationRuntimeException creationFailure) {
            m_isTakeoverPermanentlyDisabled = true;
            logRuntimeFailure("Presentation runtime creation failed", creationFailure);
            return false;
        }
        if (candidateRuntime == null) {
            m_isTakeoverPermanentlyDisabled = true;
            m_logger.error("Presentation runtime factory returned null");
            return false;
        }

        boolean candidateRequiresReconfiguration = false;
        PresentationFrameStatus beginStatus;
        try {
            beginStatus = candidateRuntime.beginFrameStatus(
                inputs.framebufferWidth(), inputs.framebufferHeight());
        } catch (NativePresentationRuntimeException beginFailure) {
            return rejectCandidateRuntime(
                candidateRuntime, "Presentation candidate acquisition failed", beginFailure);
        }
        if (beginStatus == PresentationFrameStatus.SURFACE_UNAVAILABLE
            || beginStatus == PresentationFrameStatus.RECREATE_REQUIRED) {
            return rejectCandidateRuntime(candidateRuntime, null, null);
        }
        if (beginStatus == PresentationFrameStatus.SUBOPTIMAL) {
            candidateRequiresReconfiguration = true;
        }

        PresentationFrameStatus submitStatus;
        try {
            submitStatus = candidateRuntime.submitAndPresentClearFrame(
                s_clearRed, s_clearGreen, s_clearBlue);
        } catch (NativePresentationRuntimeException submitFailure) {
            return rejectCandidateRuntime(
                candidateRuntime, "Presentation candidate submission failed", submitFailure);
        }
        if (submitStatus == PresentationFrameStatus.SURFACE_UNAVAILABLE
            || submitStatus == PresentationFrameStatus.RECREATE_REQUIRED) {
            return rejectCandidateRuntime(candidateRuntime, null, null);
        }
        if (submitStatus == PresentationFrameStatus.SUBOPTIMAL) {
            candidateRequiresReconfiguration = true;
        }

        m_runtime = candidateRuntime;
        m_framebufferWidth = inputs.framebufferWidth();
        m_framebufferHeight = inputs.framebufferHeight();
        m_isTakenOver = true;
        m_areHostImagePresentationResourcesAttached = true;
        m_requiresReconfiguration = candidateRequiresReconfiguration;
        return true;
    }

    /**
     * @note ThreadSafety: Render-thread-confined hot path; call once before backend frame work.
     * Acquires a frame only for an active healthy takeover and contains checked Core failures.
     * A fatal generation retains its last presented image without making another Native call.
     * @warning MemoryOwnership: Uses reusable runtime-owned storage and transfers no ownership.
     */
    public void beginFrame() {
        if (!m_isTakenOver || m_isGenerationFatal || m_runtime == null) {
            return;
        }

        m_isFrameOpen = false;
        try {
            PresentationFrameStatus frameStatus =
                m_runtime.beginFrameStatus(m_framebufferWidth, m_framebufferHeight);
            switch (frameStatus) {
                case SUCCESS -> m_isFrameOpen = true;
                case SUBOPTIMAL -> {
                    m_isFrameOpen = true;
                    m_requiresReconfiguration = true;
                }
                case SURFACE_UNAVAILABLE, RECREATE_REQUIRED ->
                    m_requiresReconfiguration = true;
            }
        } catch (NativePresentationRuntimeException frameFailure) {
            handleFrameFailure("Presentation frame acquisition failed", frameFailure);
        }
    }

    /**
     * @note ThreadSafety: Render-thread-confined hot path; call once after backend frame work.
     * Submits the fixed clear frame only when a healthy takeover acquired an open frame and contains
     * checked Core failures. A fatal generation retains its last presented image without another
     * Native call.
     * @warning MemoryOwnership: Uses reusable runtime-owned storage and transfers no ownership.
     */
    public void presentFrame() {
        if (!m_isTakenOver || m_isGenerationFatal || !m_isFrameOpen || m_runtime == null) {
            return;
        }

        try {
            PresentationFrameStatus frameStatus = m_areHostImagePresentationResourcesAttached
                ? m_runtime.submitAndPresentHostImageFrame()
                : m_runtime.submitAndPresentClearFrame(s_clearRed, s_clearGreen, s_clearBlue);
            if (frameStatus != PresentationFrameStatus.SUCCESS) {
                m_requiresReconfiguration = true;
            }
        } catch (NativePresentationRuntimeException frameFailure) {
            handleFrameFailure("Presentation frame submission failed", frameFailure);
        } finally {
            m_isFrameOpen = false;
        }
    }

    /** Returns whether backend methods must remain intercepted for the current generation. */
    public boolean isTakenOver() {
        return m_isTakenOver;
    }

    /** Returns whether the host should schedule a configure transition. */
    public boolean requiresReconfiguration() {
        return m_requiresReconfiguration;
    }

    /** Returns whether one host-image candidate runtime was successfully published and retained. */
    public boolean hasCommittedHostImagePresentationGeneration() {
        return m_runtime != null
            && m_isTakenOver
            && m_areHostImagePresentationResourcesAttached;
    }

    /**
     * @note ThreadSafety: Render-thread-confined; call before complete host target resize.
     * Detaches borrowed host-image resources exactly once while preserving clear-capable runtime and
     * any open frame. A checked failure vetoes resize and preserves attached host presentation.
     *
     * @return boolean True when resources are detached or already absent; false when resize must stop
     * @warning MemoryOwnership: Success ends Native borrowing before host image destruction. Failure
     * leaves the old host image valid and borrowed until the retained runtime is closed.
     */
    public boolean detachHostImagePresentationResources() {
        if (m_isTerminallyIntercepting
            && m_areHostImagePresentationResourcesAttached) {
            return false;
        }
        if (!m_isTakenOver || m_runtime == null
            || !m_areHostImagePresentationResourcesAttached) {
            return true;
        }
        if (m_hostResourceDetachFailure != null) {
            return false;
        }

        try {
            m_runtime.detachHostImagePresentationResources();
        } catch (NativePresentationRuntimeException detachFailure) {
            m_hostResourceDetachFailure = detachFailure;
            m_requiresReconfiguration = true;
            m_isTakeoverPermanentlyDisabled = true;
            logRuntimeFailure("Presentation host-image resource detach failed", detachFailure);
            return false;
        }

        m_areHostImagePresentationResourcesAttached = false;
        m_requiresReconfiguration = true;
        return true;
    }

    /**
     * @note ThreadSafety: Render-thread-confined; call serially during surface teardown.
     * Consumes the owned runtime and clears interception state before its sole close attempt.
     *
     * @throws NativePresentationRuntimeException When Core reports a destroy failure
     * @warning MemoryOwnership: The Native destroy boundary consumes the runtime address regardless
     * of reported outcome, so this coordinator drops the runtime before close and never retries it.
     */
    @Override
    public void close() throws NativePresentationRuntimeException {
        PresentationRuntime runtime = m_runtime;
        NativePresentationRuntimeException primaryFrameFailure = m_primaryFrameFailure;
        NativePresentationRuntimeException hostResourceDetachFailure =
            m_hostResourceDetachFailure;
        m_runtime = null;
        m_isTakenOver = false;
        m_isFrameOpen = false;
        m_requiresReconfiguration = false;
        m_isGenerationFatal = false;
        m_isTerminallyIntercepting = false;
        m_areHostImagePresentationResourcesAttached = false;
        m_primaryFrameFailure = null;
        m_hostResourceDetachFailure = null;
        if (runtime != null) {
            try {
                runtime.close();
            } catch (NativePresentationRuntimeException closeFailure) {
                addRetainedGenerationFailures(
                    closeFailure,
                    primaryFrameFailure,
                    hostResourceDetachFailure);
                throw closeFailure;
            }
        }
    }

    /**
     * @note ThreadSafety: Render-thread-confined slow failure path.
     * Records the first checked frame failure, marks the generation fatal without further Native
     * calls and delays vanilla fallback until configure can drain the retained runtime.
     *
     * @param String message Failure operation context
     * @param NativePresentationRuntimeException frameFailure Primary checked Core failure
     * @warning MemoryOwnership: Retains the owned runtime until configure or explicit close.
     */
    private void handleFrameFailure(
        String message,
        NativePresentationRuntimeException frameFailure) {
        m_isFrameOpen = false;
        m_requiresReconfiguration = true;
        m_isTakeoverPermanentlyDisabled = true;
        m_isGenerationFatal = true;
        if (m_primaryFrameFailure == null) {
            m_primaryFrameFailure = frameFailure;
            logRuntimeFailure(message, frameFailure);
        }
    }

    /**
     * @note ThreadSafety: Render-thread-confined configure slow path.
     * Closes an uncommitted candidate and either permits vanilla configure or preserves terminal
     * interception when candidate destruction is uncertain.
     *
     * @param PresentationRuntime candidateRuntime Uncommitted candidate runtime to close
     * @param String failureMessage Prime operation context, or null for an ordinary status rejection
     * @param NativePresentationRuntimeException primeFailure Checked prime failure, or null
     * @return boolean False after successful candidate close; true for terminal interception
     * @warning MemoryOwnership: Releases candidate ownership before false; a close failure leaves
     * Native ownership uncertain and therefore prevents vanilla swapchain creation.
     */
    private boolean rejectCandidateRuntime(
        PresentationRuntime candidateRuntime,
        String failureMessage,
        NativePresentationRuntimeException primeFailure) {
        m_isTakeoverPermanentlyDisabled = true;
        try {
            candidateRuntime.close();
        } catch (NativePresentationRuntimeException closeFailure) {
            addSuppressedFailure(closeFailure, primeFailure);
            m_runtime = null;
            m_isTakenOver = true;
            m_requiresReconfiguration = false;
            m_isTerminallyIntercepting = true;
            m_areHostImagePresentationResourcesAttached = true;
            logRuntimeFailure("Presentation candidate destruction failed", closeFailure);
            return true;
        }
        if (primeFailure != null) {
            logRuntimeFailure(failureMessage, primeFailure);
        }
        return false;
    }

    /**
     * Attaches distinct failure context once so repeated destroy attempts do not duplicate it.
     *
     * @param NativePresentationRuntimeException primaryFailure Failure propagated to the caller
     * @param NativePresentationRuntimeException contextualFailure Earlier related failure, or null
     */
    private static void addSuppressedFailure(
        NativePresentationRuntimeException primaryFailure,
        NativePresentationRuntimeException contextualFailure) {
        if (contextualFailure == null || contextualFailure == primaryFailure) {
            return;
        }
        for (Throwable suppressedFailure : primaryFailure.getSuppressed()) {
            if (suppressedFailure == contextualFailure) {
                return;
            }
        }
        primaryFailure.addSuppressed(contextualFailure);
    }

    /**
     * @note ThreadSafety: Caller-confined; mutates only the supplied close failure.
     * Attaches retained generation failures exactly once in deterministic frame-then-detach order.
     *
     * @param NativePresentationRuntimeException closeFailure Primary close failure to propagate or log
     * @param NativePresentationRuntimeException primaryFrameFailure Earlier frame failure, or null
     * @param NativePresentationRuntimeException hostResourceDetachFailure Earlier detach failure,
     * or null
     * @warning MemoryOwnership: Exception references are observed synchronously and neither retained
     * nor transferred beyond standard Java suppressed-exception ownership.
     */
    private static void addRetainedGenerationFailures(
        NativePresentationRuntimeException closeFailure,
        NativePresentationRuntimeException primaryFrameFailure,
        NativePresentationRuntimeException hostResourceDetachFailure) {
        addSuppressedFailure(closeFailure, primaryFrameFailure);
        addSuppressedFailure(closeFailure, hostResourceDetachFailure);
    }

    /**
     * @note ThreadSafety: Render-thread-confined slow failure path; logger synchronization is
     * delegated to the injected SLF4J implementation.
     * Reports complete Core runtime context on a slow failure path.
     *
     * @param String message Failure operation context
     * @param NativePresentationRuntimeException failure Checked Core runtime failure
     * @warning MemoryOwnership: Logging observes but does not retain or own the checked failure.
     */
    private void logRuntimeFailure(
        String message,
        NativePresentationRuntimeException failure) {
        m_logger.error(
            message + "; Native symbol='" + failure.nativeSymbolName()
                + "', operation result=" + failure.operationResultCode()
                + ", Vulkan result=" + failure.vulkanResult(),
            failure);
    }

    /**
     * @note ThreadSafety: Render-thread-confined slow failure path; logger synchronization is
     * delegated to the injected SLF4J implementation.
     * Reports complete Core library-loading context on a slow failure path.
     *
     * @param String message Failure operation context
     * @param NativeLibraryLoadingException failure Checked Core loading failure
     * @warning MemoryOwnership: Logging observes but does not retain or own the checked failure.
     */
    private void logLoadingFailure(
        String message,
        NativeLibraryLoadingException failure) {
        m_logger.error(
            message + "; Native library='" + failure.nativeLibraryPath()
                + "', Native symbol='" + failure.nativeSymbolName() + "'",
            failure);
    }
}
