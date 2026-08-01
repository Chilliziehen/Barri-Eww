package barrieww.mod;

import static org.junit.jupiter.api.Assertions.assertDoesNotThrow;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertSame;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.mockStatic;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import barrieww.core.interoperability.NativeLibraryLoadingException;
import barrieww.core.interoperability.NativePresentationRuntime;
import barrieww.core.interoperability.NativePresentationRuntimeException;
import barrieww.core.interoperability.PresentationBootstrapHandles;
import barrieww.core.interoperability.PresentationFrameStatus;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import org.junit.jupiter.api.Test;
import org.mockito.MockedStatic;
import org.slf4j.Logger;

/**
 * @note ThreadSafety: Tests use independent thread-confined fakes and static mocking serially.
 * Verifies the host-neutral presentation generation policy and its thin Core binding adapters.
 * @warning MemoryOwnership: Fake runtimes model exclusive Native runtime ownership and record close.
 */
final class PresentationTakeoverCoordinatorTests {
    private static final PresentationBootstrapHandles s_bootstrapHandles =
        new PresentationBootstrapHandles(1L, 2L, 3L, 4L, 5L, 5L, 6, 6);

    /** Verifies every first invalid or unready input permanently commits the surface to vanilla. */
    @Test
    void invalidOrUnreadyInitialConfigurePermanentlyCommitsVanilla() {
        PresentationGenerationInputs[] unavailableInputs = {
            null,
            new PresentationGenerationInputs(null, 800, 600, true, true),
            new PresentationGenerationInputs(s_bootstrapHandles, 0, 600, true, true),
            new PresentationGenerationInputs(s_bootstrapHandles, 800, -1, true, true),
            new PresentationGenerationInputs(s_bootstrapHandles, 800, 600, false, true),
            new PresentationGenerationInputs(s_bootstrapHandles, 800, 600, true, false)
        };
        for (PresentationGenerationInputs unavailableInput : unavailableInputs) {
            RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
            runtimeFactory.m_nextRuntime =
                new RecordingRuntime("late", runtimeFactory.m_events);
            PresentationTakeoverCoordinator coordinator = createCoordinator(runtimeFactory);

            assertFalse(coordinator.configure(unavailableInput));
            assertFalse(coordinator.configure(readyInputs(800, 600)));

            assertEquals(0, runtimeFactory.m_createCount);
            assertFalse(coordinator.isTakenOver());
        }
    }

    /** Verifies configure primes one frame before committing the first ready generation. */
    @Test
    void readyInitialGenerationCreatesRuntimeOnce() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        RecordingRuntime runtime = new RecordingRuntime("first", runtimeFactory.m_events);
        runtimeFactory.m_nextRuntime = runtime;
        PresentationTakeoverCoordinator coordinator = createCoordinator(runtimeFactory);

        assertTrue(coordinator.configure(readyInputs(800, 600)));

        assertEquals(1, runtimeFactory.m_createCount);
        assertEquals(2, runtimeFactory.m_framesInFlightCount);
        assertEquals(s_bootstrapHandles, runtimeFactory.m_bootstrapHandles);
        assertEquals(List.of("create:first", "begin:first:800x600", "submit:first"),
            runtimeFactory.m_events);
        assertEquals(1, runtime.m_beginCount);
        assertEquals(1, runtime.m_presentCount);
        assertEquals(0.08f, runtime.m_clearRed);
        assertEquals(0.72f, runtime.m_clearGreen);
        assertEquals(0.93f, runtime.m_clearBlue);
        assertTrue(coordinator.isTakenOver());
        assertFalse(coordinator.requiresReconfiguration());
    }

    /** Verifies unavailable prime acquisition closes the candidate without submitting. */
    @Test
    void unavailablePrimeBeginClosesCandidateAndReturnsFalse() {
        for (PresentationFrameStatus beginStatus : List.of(
            PresentationFrameStatus.SURFACE_UNAVAILABLE,
            PresentationFrameStatus.RECREATE_REQUIRED)) {
            RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
            RecordingRuntime runtime = new RecordingRuntime("candidate", runtimeFactory.m_events);
            runtime.m_beginStatus = beginStatus;
            runtimeFactory.m_nextRuntime = runtime;
            PresentationTakeoverCoordinator coordinator = createCoordinator(runtimeFactory);

            assertFalse(coordinator.configure(readyInputs(800, 600)));

            assertEquals(List.of(
                "create:candidate", "begin:candidate:800x600", "close:candidate"),
                runtimeFactory.m_events);
            assertEquals(0, runtime.m_presentCount);
            assertEquals(1, runtime.m_closeCount);
            assertFalse(coordinator.isTakenOver());

            runtimeFactory.m_nextRuntime =
                new RecordingRuntime("retry", runtimeFactory.m_events);
            assertFalse(coordinator.configure(readyInputs(1024, 768)));
            assertEquals(1, runtimeFactory.m_createCount);
        }
    }

    /** Verifies unavailable prime submission closes the candidate and returns false. */
    @Test
    void unavailablePrimeSubmitClosesCandidateAndReturnsFalse() {
        for (PresentationFrameStatus submitStatus : List.of(
            PresentationFrameStatus.SURFACE_UNAVAILABLE,
            PresentationFrameStatus.RECREATE_REQUIRED)) {
            RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
            RecordingRuntime runtime = new RecordingRuntime("candidate", runtimeFactory.m_events);
            runtime.m_submitStatus = submitStatus;
            runtimeFactory.m_nextRuntime = runtime;
            PresentationTakeoverCoordinator coordinator = createCoordinator(runtimeFactory);

            assertFalse(coordinator.configure(readyInputs(800, 600)));

            assertEquals(List.of("create:candidate", "begin:candidate:800x600",
                "submit:candidate", "close:candidate"), runtimeFactory.m_events);
            assertEquals(1, runtime.m_presentCount);
            assertEquals(1, runtime.m_closeCount);
            assertFalse(coordinator.isTakenOver());
        }
    }

    /** Verifies a checked prime begin failure closes the candidate and preserves context. */
    @Test
    void checkedPrimeBeginFailureClosesCandidateAndReturnsFalse() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        RecordingRuntime runtime = new RecordingRuntime("candidate", runtimeFactory.m_events);
        NativePresentationRuntimeException beginFailure = runtimeFailure("primeBegin");
        runtime.m_beginFailure = beginFailure;
        runtimeFactory.m_nextRuntime = runtime;
        Logger logger = mock(Logger.class);
        PresentationTakeoverCoordinator coordinator =
            new PresentationTakeoverCoordinator(runtimeFactory, logger);

        assertFalse(coordinator.configure(readyInputs(800, 600)));

        assertEquals(1, runtime.m_closeCount);
        assertEquals(0, runtime.m_presentCount);
        assertFalse(coordinator.isTakenOver());
        verify(logger).error(any(String.class), org.mockito.ArgumentMatchers.same(beginFailure));
    }

    /** Verifies a checked prime submit failure closes the candidate and preserves context. */
    @Test
    void checkedPrimeSubmitFailureClosesCandidateAndReturnsFalse() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        RecordingRuntime runtime = new RecordingRuntime("candidate", runtimeFactory.m_events);
        NativePresentationRuntimeException submitFailure = runtimeFailure("primeSubmit");
        runtime.m_submitFailure = submitFailure;
        runtimeFactory.m_nextRuntime = runtime;
        Logger logger = mock(Logger.class);
        PresentationTakeoverCoordinator coordinator =
            new PresentationTakeoverCoordinator(runtimeFactory, logger);

        assertFalse(coordinator.configure(readyInputs(800, 600)));

        assertEquals(1, runtime.m_closeCount);
        assertEquals(1, runtime.m_presentCount);
        assertFalse(coordinator.isTakenOver());
        verify(logger).error(any(String.class), org.mockito.ArgumentMatchers.same(submitFailure));
    }

    /** Verifies failed candidate close enters stable terminal interception with suppression. */
    @Test
    void checkedPrimeFailureCloseFailureEntersTerminalInterceptionWithSuppression() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        RecordingRuntime runtime = new RecordingRuntime("candidate", runtimeFactory.m_events);
        NativePresentationRuntimeException submitFailure = runtimeFailure("primeSubmit");
        NativePresentationRuntimeException closeFailure = runtimeFailure("candidateClose");
        runtime.m_submitFailure = submitFailure;
        runtime.m_closeFailure = closeFailure;
        runtimeFactory.m_nextRuntime = runtime;
        Logger logger = mock(Logger.class);
        PresentationTakeoverCoordinator coordinator =
            new PresentationTakeoverCoordinator(runtimeFactory, logger);

        assertTrue(coordinator.configure(readyInputs(800, 600)));

        assertSame(submitFailure, closeFailure.getSuppressed()[0]);
        assertTrue(coordinator.isTakenOver());
        assertFalse(coordinator.requiresReconfiguration());
        assertEquals(1, runtimeFactory.m_createCount);
        assertEquals(1, runtime.m_closeCount);
        verify(logger).error(any(String.class), org.mockito.ArgumentMatchers.same(closeFailure));

        assertTrue(coordinator.configure(readyInputs(1024, 768)));
        assertFalse(coordinator.requiresReconfiguration());
        assertEquals(1, runtimeFactory.m_createCount);
        assertEquals(1, runtime.m_closeCount);
        verify(logger, times(1)).error(any(String.class), any(Throwable.class));

        assertDoesNotThrow(coordinator::close);
        assertEquals(1, runtime.m_closeCount);
        assertFalse(coordinator.isTakenOver());
    }

    /** Verifies suboptimal prime status commits takeover while requesting reconfiguration. */
    @Test
    void suboptimalPrimeCommitsAndRequestsReconfiguration() {
        for (boolean isBeginSuboptimal : List.of(true, false)) {
            RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
            RecordingRuntime runtime = new RecordingRuntime("candidate", runtimeFactory.m_events);
            runtime.m_beginStatus = isBeginSuboptimal
                ? PresentationFrameStatus.SUBOPTIMAL : PresentationFrameStatus.SUCCESS;
            runtime.m_submitStatus = isBeginSuboptimal
                ? PresentationFrameStatus.SUCCESS : PresentationFrameStatus.SUBOPTIMAL;
            runtimeFactory.m_nextRuntime = runtime;
            PresentationTakeoverCoordinator coordinator = createCoordinator(runtimeFactory);

            assertTrue(coordinator.configure(readyInputs(800, 600)));

            assertTrue(coordinator.isTakenOver());
            assertTrue(coordinator.requiresReconfiguration());
        }
    }

    /** Verifies resize drains the old generation before replacement creation. */
    @Test
    void resizeClosesOldRuntimeBeforeCreatingReplacement() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        runtimeFactory.m_nextRuntime = new RecordingRuntime("first", runtimeFactory.m_events);
        PresentationTakeoverCoordinator coordinator = createCoordinator(runtimeFactory);
        assertTrue(coordinator.configure(readyInputs(800, 600)));
        runtimeFactory.m_nextRuntime = new RecordingRuntime("second", runtimeFactory.m_events);

        assertTrue(coordinator.configure(readyInputs(1024, 768)));

        assertEquals(List.of("create:first", "begin:first:800x600", "submit:first",
            "close:first", "create:second", "begin:second:1024x768", "submit:second"),
            runtimeFactory.m_events);
        assertTrue(coordinator.isTakenOver());
    }

    /** Verifies resize candidate rejection permits vanilla only after the old runtime closes. */
    @Test
    void resizePrimeFailureClosesOldAndCandidateBeforeReturningFalse() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        RecordingRuntime firstRuntime = new RecordingRuntime("first", runtimeFactory.m_events);
        runtimeFactory.m_nextRuntime = firstRuntime;
        PresentationTakeoverCoordinator coordinator = createCoordinator(runtimeFactory);
        assertTrue(coordinator.configure(readyInputs(800, 600)));
        RecordingRuntime secondRuntime = new RecordingRuntime("second", runtimeFactory.m_events);
        secondRuntime.m_submitStatus = PresentationFrameStatus.RECREATE_REQUIRED;
        runtimeFactory.m_nextRuntime = secondRuntime;

        assertFalse(coordinator.configure(readyInputs(1024, 768)));

        assertEquals(List.of("create:first", "begin:first:800x600", "submit:first",
            "close:first", "create:second", "begin:second:1024x768", "submit:second",
            "close:second"), runtimeFactory.m_events);
        assertEquals(1, firstRuntime.m_closeCount);
        assertEquals(1, secondRuntime.m_closeCount);
        assertFalse(coordinator.isTakenOver());

        runtimeFactory.m_nextRuntime =
            new RecordingRuntime("third", runtimeFactory.m_events);
        assertFalse(coordinator.configure(readyInputs(1280, 720)));
        assertEquals(2, runtimeFactory.m_createCount);
    }

    /** Verifies failed replacement leaves configure in the vanilla generation state. */
    @Test
    void replacementFailureReturnsFalseAfterEndingOldTakeover() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        runtimeFactory.m_nextRuntime = new RecordingRuntime("first", runtimeFactory.m_events);
        PresentationTakeoverCoordinator coordinator = createCoordinator(runtimeFactory);
        assertTrue(coordinator.configure(readyInputs(800, 600)));
        runtimeFactory.m_creationFailure = runtimeFailure("replacement");

        assertFalse(coordinator.configure(readyInputs(1024, 768)));

        assertEquals(List.of("create:first", "begin:first:800x600", "submit:first",
            "close:first", "create:failure"),
            runtimeFactory.m_events);
        assertFalse(coordinator.isTakenOver());
        assertDoesNotThrow(coordinator::beginFrame);
        assertDoesNotThrow(coordinator::presentFrame);
        assertFalse(coordinator.configure(readyInputs(1280, 720)));
        assertEquals(2, runtimeFactory.m_createCount);
    }

    /** Verifies successful frame operations occur in exact order with the fixed clear color. */
    @Test
    void successfulBeginThenPresentUsesExactFixedClearColor() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        RecordingRuntime runtime = new RecordingRuntime("first", runtimeFactory.m_events);
        runtimeFactory.m_nextRuntime = runtime;
        PresentationTakeoverCoordinator coordinator = createCoordinator(runtimeFactory);
        assertTrue(coordinator.configure(readyInputs(800, 600)));

        coordinator.beginFrame();
        coordinator.presentFrame();

        assertEquals(List.of("create:first", "begin:first:800x600", "submit:first",
            "begin:first:800x600", "submit:first"),
            runtimeFactory.m_events);
        assertEquals(0.08f, runtime.m_clearRed);
        assertEquals(0.72f, runtime.m_clearGreen);
        assertEquals(0.93f, runtime.m_clearBlue);
        assertFalse(coordinator.requiresReconfiguration());
    }

    /** Verifies presentation without a successful begin never submits to Core. */
    @Test
    void presentBeforeBeginDoesNotSubmit() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        RecordingRuntime runtime = new RecordingRuntime("first", runtimeFactory.m_events);
        runtimeFactory.m_nextRuntime = runtime;
        PresentationTakeoverCoordinator coordinator = createCoordinator(runtimeFactory);
        assertTrue(coordinator.configure(readyInputs(800, 600)));

        coordinator.presentFrame();

        assertEquals(1, runtime.m_presentCount);
    }

    /** Verifies suboptimal acquisition remains open and requests a later reconfiguration. */
    @Test
    void suboptimalBeginStillPresentsAndRequestsReconfiguration() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        RecordingRuntime runtime = new RecordingRuntime("first", runtimeFactory.m_events);
        runtime.m_beginStatus = PresentationFrameStatus.SUBOPTIMAL;
        runtimeFactory.m_nextRuntime = runtime;
        PresentationTakeoverCoordinator coordinator = createCoordinator(runtimeFactory);
        assertTrue(coordinator.configure(readyInputs(800, 600)));

        coordinator.beginFrame();
        coordinator.presentFrame();

        assertEquals(2, runtime.m_presentCount);
        assertTrue(coordinator.requiresReconfiguration());
    }

    /** Verifies unavailable and recreate acquisition statuses never submit. */
    @Test
    void unavailableOrRecreateBeginDoesNotPresent() {
        for (PresentationFrameStatus beginStatus : List.of(
            PresentationFrameStatus.SURFACE_UNAVAILABLE,
            PresentationFrameStatus.RECREATE_REQUIRED)) {
            RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
            RecordingRuntime runtime = new RecordingRuntime("first", runtimeFactory.m_events);
            runtimeFactory.m_nextRuntime = runtime;
            PresentationTakeoverCoordinator coordinator = createCoordinator(runtimeFactory);
            assertTrue(coordinator.configure(readyInputs(800, 600)));
            runtime.m_beginStatus = beginStatus;

            coordinator.beginFrame();
            coordinator.presentFrame();

            assertEquals(1, runtime.m_presentCount);
            assertTrue(coordinator.requiresReconfiguration());
            assertTrue(coordinator.isTakenOver());
        }
    }

    /** Verifies every non-success submit status requests reconfiguration. */
    @Test
    void nonSuccessSubmitStatusRequestsReconfiguration() {
        for (PresentationFrameStatus submitStatus : List.of(
            PresentationFrameStatus.SUBOPTIMAL,
            PresentationFrameStatus.RECREATE_REQUIRED,
            PresentationFrameStatus.SURFACE_UNAVAILABLE)) {
            RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
            RecordingRuntime runtime = new RecordingRuntime("first", runtimeFactory.m_events);
            runtimeFactory.m_nextRuntime = runtime;
            PresentationTakeoverCoordinator coordinator = createCoordinator(runtimeFactory);
            assertTrue(coordinator.configure(readyInputs(800, 600)));
            runtime.m_submitStatus = submitStatus;

            coordinator.beginFrame();
            coordinator.presentFrame();

            assertTrue(coordinator.requiresReconfiguration());
        }
    }

    /** Verifies fatal begin state makes no further Native calls before delayed fallback drains. */
    @Test
    void checkedBeginFailureStopsNativeCallsUntilConfigureDrainsRuntime() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        RecordingRuntime runtime = new RecordingRuntime("first", runtimeFactory.m_events);
        NativePresentationRuntimeException beginFailure = runtimeFailure("begin");
        runtimeFactory.m_nextRuntime = runtime;
        Logger logger = mock(Logger.class);
        PresentationTakeoverCoordinator coordinator =
            new PresentationTakeoverCoordinator(runtimeFactory, logger);
        assertTrue(coordinator.configure(readyInputs(800, 600)));
        runtime.m_beginFailure = beginFailure;

        assertDoesNotThrow(coordinator::beginFrame);

        assertTrue(coordinator.isTakenOver());
        assertTrue(coordinator.requiresReconfiguration());
        assertEquals(0, runtime.m_closeCount);
        assertEquals(2, runtime.m_beginCount);
        assertEquals(1, runtime.m_presentCount);

        assertDoesNotThrow(coordinator::beginFrame);
        assertDoesNotThrow(coordinator::presentFrame);
        assertDoesNotThrow(coordinator::beginFrame);
        assertDoesNotThrow(coordinator::presentFrame);

        assertEquals(2, runtime.m_beginCount);
        assertEquals(1, runtime.m_presentCount);
        verify(logger, times(1)).error(any(String.class), any(Throwable.class));
        assertTrue(coordinator.isTakenOver());
        assertFalse(coordinator.configure(readyInputs(1024, 768)));
        assertEquals(1, runtime.m_closeCount);
        assertFalse(coordinator.isTakenOver());
        assertEquals(1, runtimeFactory.m_createCount);
    }

    /** Verifies fatal submit state makes no further Native calls before delayed fallback drains. */
    @Test
    void checkedSubmitFailureStopsNativeCallsUntilConfigureDrainsRuntime() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        RecordingRuntime runtime = new RecordingRuntime("first", runtimeFactory.m_events);
        NativePresentationRuntimeException submitFailure = runtimeFailure("submit");
        runtimeFactory.m_nextRuntime = runtime;
        Logger logger = mock(Logger.class);
        PresentationTakeoverCoordinator coordinator =
            new PresentationTakeoverCoordinator(runtimeFactory, logger);
        assertTrue(coordinator.configure(readyInputs(800, 600)));
        runtime.m_submitFailure = submitFailure;
        coordinator.beginFrame();

        assertDoesNotThrow(coordinator::presentFrame);

        assertTrue(coordinator.isTakenOver());
        assertTrue(coordinator.requiresReconfiguration());
        assertEquals(2, runtime.m_presentCount);
        assertEquals(0, runtime.m_closeCount);
        assertEquals(2, runtime.m_beginCount);

        coordinator.beginFrame();
        coordinator.presentFrame();
        coordinator.beginFrame();
        coordinator.presentFrame();

        assertEquals(2, runtime.m_beginCount);
        assertEquals(2, runtime.m_presentCount);
        verify(logger, times(1)).error(any(String.class), any(Throwable.class));
        assertFalse(coordinator.configure(readyInputs(1024, 768)));
        assertEquals(1, runtime.m_closeCount);
        assertFalse(coordinator.isTakenOver());
    }

    /** Verifies delayed fallback attaches and separately reports a runtime close failure. */
    @Test
    void delayedFallbackAttachesAndReportsCloseFailure() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        RecordingRuntime runtime = new RecordingRuntime("first", runtimeFactory.m_events);
        NativePresentationRuntimeException beginFailure = runtimeFailure("begin");
        NativePresentationRuntimeException closeFailure = runtimeFailure("close");
        runtimeFactory.m_nextRuntime = runtime;
        Logger logger = mock(Logger.class);
        PresentationTakeoverCoordinator coordinator =
            new PresentationTakeoverCoordinator(runtimeFactory, logger);
        assertTrue(coordinator.configure(readyInputs(800, 600)));
        runtime.m_beginFailure = beginFailure;
        runtime.m_closeFailure = closeFailure;

        coordinator.beginFrame();
        assertTrue(coordinator.configure(readyInputs(1024, 768)));

        assertEquals(1, runtime.m_closeCount);
        assertSame(beginFailure, closeFailure.getSuppressed()[0]);
        assertEquals(0, beginFailure.getSuppressed().length);
        assertTrue(coordinator.isTakenOver());
        assertEquals(1, runtimeFactory.m_createCount);
        verify(logger).error(any(String.class),
            org.mockito.ArgumentMatchers.same(closeFailure));
        verify(logger, times(2)).error(any(String.class), any(Throwable.class));
    }

    /** Verifies identical frame and close failures never trigger Java self-suppression. */
    @Test
    void identicalFrameAndConfigureCloseFailureAvoidsSelfSuppression() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        RecordingRuntime runtime = new RecordingRuntime("first", runtimeFactory.m_events);
        NativePresentationRuntimeException sharedFailure = runtimeFailure("shared");
        runtimeFactory.m_nextRuntime = runtime;
        PresentationTakeoverCoordinator coordinator = createCoordinator(runtimeFactory);
        assertTrue(coordinator.configure(readyInputs(800, 600)));
        runtime.m_beginFailure = sharedFailure;
        runtime.m_closeFailure = sharedFailure;
        coordinator.beginFrame();

        assertDoesNotThrow(() -> assertTrue(coordinator.configure(readyInputs(1024, 768))));

        assertEquals(0, sharedFailure.getSuppressed().length);
        assertTrue(coordinator.isTakenOver());
    }

    /** Verifies resize close failure preserves interception and never creates a second swapchain. */
    @Test
    void resizeCloseFailureEntersTerminalInterceptionWithoutReplacementCreation() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        RecordingRuntime runtime = new RecordingRuntime("first", runtimeFactory.m_events);
        runtimeFactory.m_nextRuntime = runtime;
        Logger logger = mock(Logger.class);
        PresentationTakeoverCoordinator coordinator =
            new PresentationTakeoverCoordinator(runtimeFactory, logger);
        assertTrue(coordinator.configure(readyInputs(800, 600)));
        runtime.m_closeFailure = runtimeFailure("close");
        runtimeFactory.m_nextRuntime = new RecordingRuntime("second", runtimeFactory.m_events);

        assertTrue(coordinator.configure(readyInputs(1024, 768)));

        assertTrue(coordinator.isTakenOver());
        assertFalse(coordinator.requiresReconfiguration());
        assertEquals(1, runtimeFactory.m_createCount);
        assertEquals(List.of("create:first", "begin:first:800x600", "submit:first",
            "close:first"), runtimeFactory.m_events);
        verify(logger, times(1)).error(any(String.class), any(Throwable.class));

        assertTrue(coordinator.configure(readyInputs(1280, 720)));
        assertFalse(coordinator.requiresReconfiguration());
        assertEquals(1, runtimeFactory.m_createCount);
        assertEquals(1, runtime.m_closeCount);
        verify(logger, times(1)).error(any(String.class), any(Throwable.class));

        assertDoesNotThrow(coordinator::close);
        assertEquals(1, runtime.m_closeCount);
        assertFalse(coordinator.isTakenOver());
    }

    /** Verifies successful explicit close releases the runtime exactly once. */
    @Test
    void successfulCloseIsIdempotentAndClosesRuntimeExactlyOnce() throws Exception {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        RecordingRuntime runtime = new RecordingRuntime("first", runtimeFactory.m_events);
        runtimeFactory.m_nextRuntime = runtime;
        PresentationTakeoverCoordinator coordinator = createCoordinator(runtimeFactory);
        assertTrue(coordinator.configure(readyInputs(800, 600)));

        coordinator.close();
        coordinator.close();

        assertEquals(1, runtime.m_closeCount);
        assertFalse(coordinator.isTakenOver());
    }

    /** Verifies explicit close failure finalizes ownership and remains idempotent. */
    @Test
    void explicitCloseFailureFinalizesOwnershipAndRemainsIdempotent() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        RecordingRuntime runtime = new RecordingRuntime("first", runtimeFactory.m_events);
        NativePresentationRuntimeException beginFailure = runtimeFailure("begin");
        NativePresentationRuntimeException closeFailure = runtimeFailure("close");
        runtimeFactory.m_nextRuntime = runtime;
        PresentationTakeoverCoordinator coordinator = createCoordinator(runtimeFactory);
        assertTrue(coordinator.configure(readyInputs(800, 600)));
        runtime.m_beginFailure = beginFailure;
        runtime.m_closeFailure = closeFailure;
        coordinator.beginFrame();

        assertSame(closeFailure, assertThrows(
            NativePresentationRuntimeException.class, coordinator::close));
        assertEquals(1, runtime.m_closeCount);

        assertDoesNotThrow(coordinator::close);
        assertDoesNotThrow(coordinator::close);

        assertSame(beginFailure, closeFailure.getSuppressed()[0]);
        assertEquals(0, beginFailure.getSuppressed().length);
        assertEquals(1, runtime.m_closeCount);
        assertFalse(coordinator.isTakenOver());
    }

    /** Verifies the Core adapter delegates semantic statuses and close without remapping. */
    @Test
    void coreRuntimeAdapterDelegatesNativeRuntime() throws Exception {
        NativePresentationRuntime nativeRuntime = mock(NativePresentationRuntime.class);
        when(nativeRuntime.beginFrameStatus(800, 600))
            .thenReturn(PresentationFrameStatus.SUBOPTIMAL);
        when(nativeRuntime.submitAndPresentClearFrame(0.08f, 0.72f, 0.93f))
            .thenReturn(PresentationFrameStatus.RECREATE_REQUIRED);
        CorePresentationRuntime runtime = new CorePresentationRuntime(nativeRuntime);

        assertSame(PresentationFrameStatus.SUBOPTIMAL, runtime.beginFrameStatus(800, 600));
        assertSame(PresentationFrameStatus.RECREATE_REQUIRED,
            runtime.submitAndPresentClearFrame(0.08f, 0.72f, 0.93f));
        runtime.close();

        verify(nativeRuntime).close();
    }

    /** Verifies the Core factory owns a normalized absolute path and forwards exact creation data. */
    @Test
    void coreRuntimeFactoryUsesAbsoluteOwnedLibraryPath() throws Exception {
        Path relativeLibraryPath = Path.of("native", "BarriEwwNativeFfm.dll");
        Path expectedLibraryPath = relativeLibraryPath.toAbsolutePath().normalize();
        NativePresentationRuntime nativeRuntime = mock(NativePresentationRuntime.class);
        try (MockedStatic<NativePresentationRuntime> nativeRuntimeCreation =
                 mockStatic(NativePresentationRuntime.class)) {
            nativeRuntimeCreation.when(() -> NativePresentationRuntime.create(
                expectedLibraryPath, s_bootstrapHandles, 800, 600, 2)).thenReturn(nativeRuntime);
            CorePresentationRuntimeFactory runtimeFactory =
                new CorePresentationRuntimeFactory(relativeLibraryPath);

            PresentationRuntime runtime = runtimeFactory.create(
                s_bootstrapHandles, 800, 600, 2);

            assertTrue(runtime instanceof CorePresentationRuntime);
            nativeRuntimeCreation.verify(() -> NativePresentationRuntime.create(
                expectedLibraryPath, s_bootstrapHandles, 800, 600, 2));
        }
    }

    /** Verifies a checked library-loading creation failure disables future takeover attempts. */
    @Test
    void libraryLoadingFailureDisablesFutureTakeoverAttempts() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        runtimeFactory.m_loadingFailure = new NativeLibraryLoadingException(
            "loading", Path.of("missing").toAbsolutePath(), "symbol", null);
        Logger logger = mock(Logger.class);
        PresentationTakeoverCoordinator coordinator =
            new PresentationTakeoverCoordinator(runtimeFactory, logger);

        assertFalse(coordinator.configure(readyInputs(800, 600)));
        assertFalse(coordinator.configure(readyInputs(1024, 768)));

        assertEquals(1, runtimeFactory.m_createCount);
        verify(logger).error(any(String.class), any(NativeLibraryLoadingException.class));
    }

    /**
     * @note ThreadSafety: Concurrency-safe immutable test-value creation.
     * Creates ready immutable generation inputs with the shared test handles.
     *
     * @param int framebufferWidth Positive test framebuffer width
     * @param int framebufferHeight Positive test framebuffer height
     * @return PresentationGenerationInputs Ready immutable test inputs
     * @warning MemoryOwnership: Shared primitive handle values remain borrowed test data.
     */
    private static PresentationGenerationInputs readyInputs(
        int framebufferWidth,
        int framebufferHeight) {
        return new PresentationGenerationInputs(
            s_bootstrapHandles, framebufferWidth, framebufferHeight, true, true);
    }

    /**
     * @note ThreadSafety: The returned coordinator is confined to the invoking test thread.
     * Creates a coordinator with a recording factory and an isolated logger.
     *
     * @param RecordingRuntimeFactory runtimeFactory Test-owned recording factory
     * @return PresentationTakeoverCoordinator Thread-confined test coordinator
     * @warning MemoryOwnership: Runtime ownership transfers to the returned coordinator on create.
     */
    private static PresentationTakeoverCoordinator createCoordinator(
        RecordingRuntimeFactory runtimeFactory) {
        return new PresentationTakeoverCoordinator(runtimeFactory, mock(Logger.class));
    }

    /**
     * @note ThreadSafety: Concurrency-safe immutable checked-failure creation.
     * Creates a checked presentation failure with stable test context.
     *
     * @param String operationName Semantic test operation name
     * @return NativePresentationRuntimeException Checked failure carrying stable test context
     */
    private static NativePresentationRuntimeException runtimeFailure(String operationName) {
        return new NativePresentationRuntimeException(
            operationName, operationName + "Symbol", 7, -4);
    }

    /** Records factory calls and allows each test to select its next outcome. */
    private static final class RecordingRuntimeFactory implements PresentationRuntimeFactory {
        private final List<String> m_events = new ArrayList<>();
        private RecordingRuntime m_nextRuntime;
        private NativeLibraryLoadingException m_loadingFailure;
        private NativePresentationRuntimeException m_creationFailure;
        private PresentationBootstrapHandles m_bootstrapHandles;
        private int m_createCount;
        private int m_framesInFlightCount;

        /**
         * @note ThreadSafety: Confined to the invoking test thread.
         * Records exact creation data and returns or throws the configured outcome.
         *
         * @param PresentationBootstrapHandles bootstrapHandles Borrowed test handles
         * @param int framebufferWidth Test framebuffer width
         * @param int framebufferHeight Test framebuffer height
         * @param int framesInFlightCount Requested frame-slot count
         * @return PresentationRuntime Configured fake runtime
         * @throws NativeLibraryLoadingException Configured checked loading failure
         * @throws NativePresentationRuntimeException Configured checked runtime failure
         * @warning MemoryOwnership: Returned fake runtime ownership transfers to the coordinator.
         */
        @Override
        public PresentationRuntime create(
            PresentationBootstrapHandles bootstrapHandles,
            int framebufferWidth,
            int framebufferHeight,
            int framesInFlightCount)
            throws NativeLibraryLoadingException, NativePresentationRuntimeException {
            m_createCount++;
            m_bootstrapHandles = bootstrapHandles;
            m_framesInFlightCount = framesInFlightCount;
            if (m_loadingFailure != null) {
                m_events.add("create:failure");
                throw m_loadingFailure;
            }
            if (m_creationFailure != null) {
                m_events.add("create:failure");
                throw m_creationFailure;
            }
            m_events.add("create:" + m_nextRuntime.m_name);
            return m_nextRuntime;
        }
    }

    /** Records frame and close calls while exposing configurable semantic outcomes. */
    private static final class RecordingRuntime implements PresentationRuntime {
        private final String m_name;
        private final List<String> m_events;
        private PresentationFrameStatus m_beginStatus = PresentationFrameStatus.SUCCESS;
        private PresentationFrameStatus m_submitStatus = PresentationFrameStatus.SUCCESS;
        private NativePresentationRuntimeException m_beginFailure;
        private NativePresentationRuntimeException m_submitFailure;
        private NativePresentationRuntimeException m_closeFailure;
        private int m_beginCount;
        private int m_presentCount;
        private int m_closeCount;
        private float m_clearRed;
        private float m_clearGreen;
        private float m_clearBlue;

        /**
         * @note ThreadSafety: Construction and use are confined to one test thread.
         * Creates one named fake sharing the factory event sequence.
         *
         * @param String name Semantic runtime generation name
         * @param List<String> events Shared ordered event recorder
         * @warning MemoryOwnership: The fake borrows the test-owned event list.
         */
        private RecordingRuntime(String name, List<String> events) {
            m_name = name;
            m_events = events;
        }

        /**
         * @note ThreadSafety: Confined to the invoking test thread.
         * Records acquisition and returns or throws the configured outcome.
         *
         * @param int framebufferWidth Test framebuffer width
         * @param int framebufferHeight Test framebuffer height
         * @return PresentationFrameStatus Configured semantic acquisition status
         * @throws NativePresentationRuntimeException Configured checked acquisition failure
         * @warning MemoryOwnership: Records primitive values and transfers no ownership.
         */
        @Override
        public PresentationFrameStatus beginFrameStatus(
            int framebufferWidth,
            int framebufferHeight) throws NativePresentationRuntimeException {
            m_events.add("begin:" + m_name + ":" + framebufferWidth + "x" + framebufferHeight);
            m_beginCount++;
            if (m_beginFailure != null) {
                throw m_beginFailure;
            }
            return m_beginStatus;
        }

        /**
         * @note ThreadSafety: Confined to the invoking test thread.
         * Records the exact clear channels and returns or throws the configured outcome.
         *
         * @param float clearRed Clear color red channel
         * @param float clearGreen Clear color green channel
         * @param float clearBlue Clear color blue channel
         * @return PresentationFrameStatus Configured semantic submit status
         * @throws NativePresentationRuntimeException Configured checked submission failure
         * @warning MemoryOwnership: Records primitive values and transfers no ownership.
         */
        @Override
        public PresentationFrameStatus submitAndPresentClearFrame(
            float clearRed,
            float clearGreen,
            float clearBlue) throws NativePresentationRuntimeException {
            m_events.add("submit:" + m_name);
            m_presentCount++;
            m_clearRed = clearRed;
            m_clearGreen = clearGreen;
            m_clearBlue = clearBlue;
            if (m_submitFailure != null) {
                throw m_submitFailure;
            }
            return m_submitStatus;
        }

        /** Records one idempotence-sensitive close attempt and optionally fails. */
        @Override
        public void close() throws NativePresentationRuntimeException {
            m_events.add("close:" + m_name);
            m_closeCount++;
            if (m_closeFailure != null) {
                throw m_closeFailure;
            }
        }
    }
}
