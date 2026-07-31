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
     * Drains the preceding runtime, then attempts takeover only when all generation inputs are ready.
     * A false return commits the current configure call to vanilla presentation.
     *
     * @param PresentationGenerationInputs inputs Immutable configure inputs, or null when unavailable
     * @return boolean True only after a replacement runtime was created successfully
     * @warning MemoryOwnership: Closes any preceding owned runtime before creating its replacement.
     */
    public boolean configure(PresentationGenerationInputs inputs) {
        m_isFrameOpen = false;
        m_isTakenOver = false;
        m_requiresReconfiguration = false;

        PresentationRuntime oldRuntime = m_runtime;
        m_runtime = null;
        if (oldRuntime != null) {
            try {
                oldRuntime.close();
            } catch (NativePresentationRuntimeException closeFailure) {
                m_isTakeoverPermanentlyDisabled = true;
                logRuntimeFailure("Presentation runtime generation drain failed", closeFailure);
                return false;
            }
        }

        if (m_isTakeoverPermanentlyDisabled || inputs == null || !inputs.isReady()) {
            return false;
        }

        try {
            PresentationRuntime replacementRuntime = m_runtimeFactory.create(
                inputs.bootstrapHandles(),
                inputs.framebufferWidth(),
                inputs.framebufferHeight(),
                s_framesInFlightCount);
            m_runtime = replacementRuntime;
            m_framebufferWidth = inputs.framebufferWidth();
            m_framebufferHeight = inputs.framebufferHeight();
            m_isTakenOver = true;
            return true;
        } catch (NativeLibraryLoadingException loadingFailure) {
            m_isTakeoverPermanentlyDisabled = true;
            logLoadingFailure("Presentation runtime creation failed", loadingFailure);
            return false;
        } catch (NativePresentationRuntimeException creationFailure) {
            m_isTakeoverPermanentlyDisabled = true;
            logRuntimeFailure("Presentation runtime creation failed", creationFailure);
            return false;
        }
    }

    /**
     * @note ThreadSafety: Render-thread-confined hot path; call once before backend frame work.
     * Acquires a frame only for an active takeover and contains checked Core failures.
     * @warning MemoryOwnership: Uses reusable runtime-owned storage and transfers no ownership.
     */
    public void beginFrame() {
        if (!m_isTakenOver || m_runtime == null) {
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
     * Submits the fixed clear frame only when takeover acquired an open frame and contains checked
     * Core failures.
     * @warning MemoryOwnership: Uses reusable runtime-owned storage and transfers no ownership.
     */
    public void presentFrame() {
        if (!m_isTakenOver || !m_isFrameOpen || m_runtime == null) {
            return;
        }

        try {
            PresentationFrameStatus frameStatus = m_runtime.submitAndPresentClearFrame(
                s_clearRed, s_clearGreen, s_clearBlue);
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

    /**
     * @note ThreadSafety: Render-thread-confined; call serially during surface teardown.
     * Clears interception state and closes the owned runtime exactly once.
     *
     * @throws NativePresentationRuntimeException When Core reports a destroy failure
     * @warning MemoryOwnership: Releases the current runtime and retains no Native ownership.
     */
    @Override
    public void close() throws NativePresentationRuntimeException {
        PresentationRuntime runtime = m_runtime;
        m_runtime = null;
        m_isTakenOver = false;
        m_isFrameOpen = false;
        m_requiresReconfiguration = false;
        if (runtime != null) {
            runtime.close();
        }
    }

    /**
     * @note ThreadSafety: Render-thread-confined slow failure path.
     * Contains a checked frame failure, drains the runtime and delays fallback until configure.
     *
     * @param String message Failure operation context
     * @param NativePresentationRuntimeException frameFailure Primary checked Core failure
     * @warning MemoryOwnership: Closes the failed owned runtime and retains no Native ownership.
     */
    private void handleFrameFailure(
        String message,
        NativePresentationRuntimeException frameFailure) {
        m_isFrameOpen = false;
        m_requiresReconfiguration = true;
        m_isTakeoverPermanentlyDisabled = true;
        PresentationRuntime failedRuntime = m_runtime;
        m_runtime = null;
        if (failedRuntime != null) {
            try {
                failedRuntime.close();
            } catch (NativePresentationRuntimeException closeFailure) {
                frameFailure.addSuppressed(closeFailure);
            }
        }
        logRuntimeFailure(message, frameFailure);
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
