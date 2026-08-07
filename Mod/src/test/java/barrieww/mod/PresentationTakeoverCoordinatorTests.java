package barrieww.mod;

import static org.junit.jupiter.api.Assertions.assertDoesNotThrow;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertSame;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.mockStatic;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import barrieww.core.interoperability.NativeLibraryLoadingException;
import barrieww.core.interoperability.NativePresentationRuntime;
import barrieww.core.interoperability.NativePresentationRuntimeException;
import barrieww.core.interoperability.HostImagePresentationBinding;
import barrieww.core.interoperability.PresentationBootstrapHandles;
import barrieww.core.interoperability.PresentationFrameStatus;
import barrieww.core.interoperability.PresentationImageFormat;
import java.lang.reflect.Method;
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
    /** Verifies host generation commit state excludes unready and terminal configure outcomes. */
    @Test
    void committedHostGenerationQueryRequiresPublishedRuntime() throws Exception {
        PresentationRuntimeFactory runtimeFactory = mock(PresentationRuntimeFactory.class);
        PresentationRuntime candidateRuntime = mock(PresentationRuntime.class);
        PresentationTakeoverCoordinator coordinator = new PresentationTakeoverCoordinator(
            runtimeFactory,
            mock(Logger.class));
        Method committedGenerationMethod = PresentationTakeoverCoordinator.class.getMethod(
            "hasCommittedHostImagePresentationGeneration");

        assertFalse((boolean) committedGenerationMethod.invoke(coordinator));
        when(runtimeFactory.create(
            any(), anyInt(), anyInt(), anyInt(), any())).thenReturn(candidateRuntime);
        when(candidateRuntime.beginFrameStatus(anyInt(), anyInt()))
            .thenReturn(PresentationFrameStatus.SUCCESS);
        when(candidateRuntime.submitAndPresentClearFrame(anyFloat(), anyFloat(), anyFloat()))
            .thenReturn(PresentationFrameStatus.SUCCESS);

        assertTrue(coordinator.configure(readyInputs(800, 600)));
        assertTrue((boolean) committedGenerationMethod.invoke(coordinator));
        coordinator.close();
        assertFalse((boolean) committedGenerationMethod.invoke(coordinator));
    }
    private static final PresentationBootstrapHandles s_bootstrapHandles =
        new PresentationBootstrapHandles(1L, 2L, 3L, 4L, 5L, 5L, 6, 6);
    private static final HostImagePresentationBinding s_hostImageBinding =
        new HostImagePresentationBinding(
            7L,
            PresentationImageFormat.B8G8R8A8_UNORM,
            PresentationImageFormat.B8G8R8A8_UNORM,
            800,
            600);

    /** Verifies readiness requires exact valid binding extent and a positive captured generation. */
    @Test
    void generationInputsRequireExactHostBindingAndValidGeneration() {
        assertTrue(readyInputs(800, 600, 1L).isReady());
        assertFalse(new PresentationGenerationInputs(
            s_bootstrapHandles, 800, 600, true, s_hostImageBinding, 0L).isReady());
        assertFalse(new PresentationGenerationInputs(
            s_bootstrapHandles,
            1024,
            768,
            true,
            s_hostImageBinding,
            1L).isReady());
        assertFalse(new PresentationGenerationInputs(
            s_bootstrapHandles,
            800,
            600,
            true,
            new HostImagePresentationBinding(
                0L,
                PresentationImageFormat.B8G8R8A8_UNORM,
                PresentationImageFormat.B8G8R8A8_UNORM,
                800,
                600),
            1L).isReady());
    }

    /** Verifies generation inputs expose only the canonical non-legacy readiness shape. */
    @Test
    void generationInputsHaveOneCanonicalConstructorAndNoBooleanHostReadinessAccessor() {
        assertEquals(1, PresentationGenerationInputs.class.getDeclaredConstructors().length);
        assertEquals(6,
            PresentationGenerationInputs.class.getDeclaredConstructors()[0].getParameterCount());
        assertThrows(NoSuchMethodException.class, () ->
            PresentationGenerationInputs.class.getDeclaredMethod("isHostColorTextureReady"));
    }

    /** Verifies replacement drains before preparation and commits only after a clear prime. */
    @Test
    void configureOrdersClosePreparationCreateBeginPrimeAndCommit() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        runtimeFactory.m_nextRuntime = new RecordingRuntime("first", runtimeFactory.m_events);
        PresentationTakeoverCoordinator coordinator = createCoordinator(runtimeFactory);
        assertTrue(coordinator.configure(() -> readyInputs(800, 600, 1L)));
        runtimeFactory.m_nextRuntime = new RecordingRuntime("second", runtimeFactory.m_events);

        assertTrue(coordinator.configure(() -> {
            runtimeFactory.m_events.add("prepare:second");
            return readyInputs(800, 600, 2L);
        }));

        assertEquals(List.of(
            "create:first", "begin:first:800x600", "clear:first", "close:first",
            "prepare:second", "create:second", "begin:second:800x600", "clear:second"),
            runtimeFactory.m_events);
        assertSame(s_hostImageBinding, runtimeFactory.m_hostImageBinding);
        assertTrue(coordinator.isTakenOver());
    }

    /** Verifies preparation is never invoked when old runtime destruction is uncertain. */
    @Test
    void closeFailurePreventsPreparation() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        RecordingRuntime runtime = new RecordingRuntime("first", runtimeFactory.m_events);
        runtimeFactory.m_nextRuntime = runtime;
        PresentationTakeoverCoordinator coordinator = createCoordinator(runtimeFactory);
        assertTrue(coordinator.configure(() -> readyInputs(800, 600, 1L)));
        runtime.m_closeFailure = runtimeFailure("close");
        int[] preparationCount = {0};

        assertTrue(coordinator.configure(() -> {
            preparationCount[0]++;
            return readyInputs(800, 600, 2L);
        }));

        assertEquals(0, preparationCount[0]);
        assertEquals(1, runtime.m_closeCount);
        assertTrue(coordinator.isTakenOver());
    }

    /** Verifies null, unready and throwing preparation permanently select vanilla presentation. */
    @Test
    void preparationFailuresPermanentlySelectVanilla() {
        List<PresentationGenerationPreparation> preparations = List.of(
            () -> null,
            () -> new PresentationGenerationInputs(
                s_bootstrapHandles, 800, 600, true, null, 1L),
            () -> { throw runtimeFailure("prepare"); });
        for (PresentationGenerationPreparation preparation : preparations) {
            RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
            PresentationTakeoverCoordinator coordinator = createCoordinator(runtimeFactory);

            assertFalse(coordinator.configure(preparation));
            assertFalse(coordinator.configure(() -> readyInputs(800, 600, 1L)));
            assertEquals(0, runtimeFactory.m_createCount);
            assertFalse(coordinator.isTakenOver());
        }
    }

    /** Verifies unchecked preparation failure is contained and logged once before vanilla fallback. */
    @Test
    void uncheckedPreparationFailureSelectsVanillaWithoutFactoryCall() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        Logger logger = mock(Logger.class);
        PresentationTakeoverCoordinator coordinator =
            new PresentationTakeoverCoordinator(runtimeFactory, logger);
        IllegalStateException preparationFailure = new IllegalStateException("preparation");

        assertFalse(coordinator.configure(() -> { throw preparationFailure; }));

        assertEquals(0, runtimeFactory.m_createCount);
        assertFalse(coordinator.isTakenOver());
        verify(logger).error(any(String.class),
            org.mockito.ArgumentMatchers.same(preparationFailure));
    }

    /** Verifies a null candidate never creates runtime-null takeover and permanently selects vanilla. */
    @Test
    void nullFactoryCandidateSelectsVanillaWithoutTakeover() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        runtimeFactory.m_shouldReturnNull = true;
        PresentationTakeoverCoordinator coordinator = createCoordinator(runtimeFactory);

        assertFalse(coordinator.configure(() -> readyInputs(800, 600, 1L)));

        assertEquals(1, runtimeFactory.m_createCount);
        assertFalse(coordinator.isTakenOver());
        assertFalse(coordinator.configure(() -> readyInputs(800, 600, 2L)));
        assertEquals(1, runtimeFactory.m_createCount);
    }

    /** Verifies detach before acquisition switches the next complete frame to clear output. */
    @Test
    void detachBeforeBeginUsesClearForNextFrame() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        RecordingRuntime runtime = new RecordingRuntime("first", runtimeFactory.m_events);
        runtimeFactory.m_nextRuntime = runtime;
        PresentationTakeoverCoordinator coordinator = createCoordinator(runtimeFactory);
        assertTrue(coordinator.configure(() -> readyInputs(800, 600, 1L)));

        assertTrue(coordinator.detachHostImagePresentationResources());
        coordinator.beginFrame();
        coordinator.presentFrame();

        assertEquals(0, runtime.m_hostPresentCount);
        assertEquals(2, runtime.m_clearPresentCount);
        assertEquals(1, runtime.m_detachCount);
    }

    /** Verifies attached frames use host submit while a successful detach switches to clear output. */
    @Test
    void successfulDetachSwitchesOpenAndLaterFramesToClearOutput() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        RecordingRuntime runtime = new RecordingRuntime("first", runtimeFactory.m_events);
        runtimeFactory.m_nextRuntime = runtime;
        PresentationTakeoverCoordinator coordinator = createCoordinator(runtimeFactory);
        assertTrue(coordinator.configure(() -> readyInputs(800, 600, 1L)));

        coordinator.beginFrame();
        coordinator.presentFrame();
        coordinator.beginFrame();
        assertTrue(coordinator.detachHostImagePresentationResources());
        assertTrue(coordinator.detachHostImagePresentationResources());
        coordinator.presentFrame();
        coordinator.beginFrame();
        coordinator.presentFrame();

        assertEquals(1, runtime.m_hostPresentCount);
        assertEquals(3, runtime.m_clearPresentCount);
        assertEquals(1, runtime.m_detachCount);
        assertTrue(coordinator.isTakenOver());
        assertTrue(coordinator.requiresReconfiguration());
    }

    /** Verifies detach failure vetoes resize but preserves host output until delayed fallback. */
    @Test
    void detachFailureContinuesHostOutputThenFallsBackAtConfigure() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        RecordingRuntime runtime = new RecordingRuntime("first", runtimeFactory.m_events);
        runtime.m_detachFailure = runtimeFailure("detach");
        runtimeFactory.m_nextRuntime = runtime;
        Logger logger = mock(Logger.class);
        PresentationTakeoverCoordinator coordinator =
            new PresentationTakeoverCoordinator(runtimeFactory, logger);
        assertTrue(coordinator.configure(() -> readyInputs(800, 600, 1L)));

        assertFalse(coordinator.detachHostImagePresentationResources());
        assertFalse(coordinator.detachHostImagePresentationResources());
        coordinator.beginFrame();
        coordinator.presentFrame();

        assertEquals(1, runtime.m_detachCount);
        assertEquals(1, runtime.m_hostPresentCount);
        assertTrue(coordinator.isTakenOver());
        assertTrue(coordinator.requiresReconfiguration());
        verify(logger, times(1)).error(any(String.class), any(Throwable.class));

        assertFalse(coordinator.configure(() -> readyInputs(800, 600, 2L)));
        assertEquals(1, runtime.m_closeCount);
        assertFalse(coordinator.isTakenOver());
    }

    /** Verifies detach and close failures enter catastrophic terminal interception once. */
    @Test
    void detachAndCloseFailuresEnterCatastrophicTerminalInterception() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        RecordingRuntime runtime = new RecordingRuntime("first", runtimeFactory.m_events);
        NativePresentationRuntimeException detachFailure = runtimeFailure("detach");
        NativePresentationRuntimeException closeFailure = runtimeFailure("close");
        runtime.m_detachFailure = detachFailure;
        runtime.m_closeFailure = closeFailure;
        runtimeFactory.m_nextRuntime = runtime;
        PresentationTakeoverCoordinator coordinator = createCoordinator(runtimeFactory);
        assertTrue(coordinator.configure(() -> readyInputs(800, 600, 1L)));
        assertFalse(coordinator.detachHostImagePresentationResources());
        int[] preparationCount = {0};

        assertTrue(coordinator.configure(() -> {
            preparationCount[0]++;
            return readyInputs(800, 600, 2L);
        }));

        assertEquals(0, preparationCount[0]);
        assertEquals(1, runtime.m_closeCount);
        assertSame(detachFailure, closeFailure.getSuppressed()[0]);
        assertTrue(coordinator.isTakenOver());
        assertFalse(coordinator.requiresReconfiguration());
        assertFalse(coordinator.detachHostImagePresentationResources());
    }

    /**
     * Verifies configure close retains fatal frame context before terminal interception.
     * A fatal generation vetoes detach without a Native call, so the frame failure is the
     * only context close can aggregate.
     */
    @Test
    void frameAndCloseFailuresAggregateInDeterministicOrder() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        RecordingRuntime runtime = new RecordingRuntime("first", runtimeFactory.m_events);
        NativePresentationRuntimeException frameFailure = runtimeFailure("frame");
        NativePresentationRuntimeException closeFailure = runtimeFailure("close");
        runtimeFactory.m_nextRuntime = runtime;
        Logger logger = mock(Logger.class);
        PresentationTakeoverCoordinator coordinator =
            new PresentationTakeoverCoordinator(runtimeFactory, logger);
        assertTrue(coordinator.configure(() -> readyInputs(800, 600, 1L)));
        runtime.m_beginFailure = frameFailure;
        runtime.m_closeFailure = closeFailure;
        coordinator.beginFrame();

        assertFalse(coordinator.detachHostImagePresentationResources());
        assertEquals(0, runtime.m_detachCount);
        int[] preparationCount = {0};
        assertTrue(coordinator.configure(() -> {
            preparationCount[0]++;
            return readyInputs(800, 600, 2L);
        }));

        assertEquals(0, preparationCount[0]);
        assertEquals(1, runtime.m_closeCount);
        assertEquals(1, closeFailure.getSuppressed().length);
        assertSame(frameFailure, closeFailure.getSuppressed()[0]);
        assertEquals(0, frameFailure.getSuppressed().length);
        verify(logger).error(any(String.class),
            org.mockito.ArgumentMatchers.same(frameFailure));
        verify(logger).error(any(String.class),
            org.mockito.ArgumentMatchers.same(closeFailure));
        verify(logger, times(2)).error(any(String.class), any(Throwable.class));
        assertTrue(coordinator.isTakenOver());
        assertFalse(coordinator.requiresReconfiguration());
        assertFalse(coordinator.detachHostImagePresentationResources());
    }

    /** Verifies every first invalid or unready input permanently commits the surface to vanilla. */
    @Test
    void invalidOrUnreadyInitialConfigurePermanentlyCommitsVanilla() {
        PresentationGenerationInputs[] unavailableInputs = {
            null,
            new PresentationGenerationInputs(null, 800, 600, true, s_hostImageBinding, 1L),
            new PresentationGenerationInputs(
                s_bootstrapHandles, 0, 600, true, s_hostImageBinding, 1L),
            new PresentationGenerationInputs(
                s_bootstrapHandles, 800, -1, true, s_hostImageBinding, 1L),
            new PresentationGenerationInputs(
                s_bootstrapHandles, 800, 600, false, s_hostImageBinding, 1L),
            new PresentationGenerationInputs(
                s_bootstrapHandles, 800, 600, true, null, 1L)
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
        assertEquals(List.of("create:first", "begin:first:800x600", "clear:first"),
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
                "clear:candidate", "close:candidate"), runtimeFactory.m_events);
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

    /** Verifies failed candidate close enters catastrophic terminal interception with suppression. */
    @Test
    void candidatePrimeAndCloseFailuresEnterCatastrophicTerminalInterception() {
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
        assertFalse(coordinator.detachHostImagePresentationResources());
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

        assertEquals(List.of("create:first", "begin:first:800x600", "clear:first",
            "close:first", "create:second", "begin:second:1024x768", "clear:second"),
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

        assertEquals(List.of("create:first", "begin:first:800x600", "clear:first",
            "close:first", "create:second", "begin:second:1024x768", "clear:second",
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

        assertEquals(List.of("create:first", "begin:first:800x600", "clear:first",
            "close:first", "create:failure"),
            runtimeFactory.m_events);
        assertFalse(coordinator.isTakenOver());
        assertDoesNotThrow(coordinator::beginFrame);
        assertDoesNotThrow(coordinator::presentFrame);
        assertFalse(coordinator.configure(readyInputs(1280, 720)));
        assertEquals(2, runtimeFactory.m_createCount);
    }

    /** Verifies clear priming precedes exact attached host-image frame submission. */
    @Test
    void successfulBeginThenPresentUsesClearPrimeAndHostFrame() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        RecordingRuntime runtime = new RecordingRuntime("first", runtimeFactory.m_events);
        runtimeFactory.m_nextRuntime = runtime;
        PresentationTakeoverCoordinator coordinator = createCoordinator(runtimeFactory);
        assertTrue(coordinator.configure(readyInputs(800, 600)));

        coordinator.beginFrame();
        coordinator.presentFrame();

        assertEquals(List.of("create:first", "begin:first:800x600", "clear:first",
            "begin:first:800x600", "host:first"),
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
    void replacementCloseFailureEntersCatastrophicTerminalInterception() {
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
        assertEquals(List.of("create:first", "begin:first:800x600", "clear:first",
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
        when(nativeRuntime.submitAndPresentHostImageFrame())
            .thenReturn(PresentationFrameStatus.SUCCESS);
        CorePresentationRuntime runtime = new CorePresentationRuntime(nativeRuntime);

        assertSame(PresentationFrameStatus.SUBOPTIMAL, runtime.beginFrameStatus(800, 600));
        assertSame(PresentationFrameStatus.RECREATE_REQUIRED,
            runtime.submitAndPresentClearFrame(0.08f, 0.72f, 0.93f));
        runtime.detachHostImagePresentationResources();
        assertSame(PresentationFrameStatus.SUCCESS, runtime.submitAndPresentHostImageFrame());
        runtime.close();

        verify(nativeRuntime).detachHostImagePresentationResources();
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
            nativeRuntimeCreation.when(() -> NativePresentationRuntime.createHostImagePresentation(
                expectedLibraryPath, s_bootstrapHandles, 800, 600, 2, s_hostImageBinding))
                .thenReturn(nativeRuntime);
            CorePresentationRuntimeFactory runtimeFactory =
                new CorePresentationRuntimeFactory(relativeLibraryPath);

            PresentationRuntime runtime = runtimeFactory.create(
                s_bootstrapHandles, 800, 600, 2, s_hostImageBinding);

            assertTrue(runtime instanceof CorePresentationRuntime);
            nativeRuntimeCreation.verify(() -> NativePresentationRuntime.createHostImagePresentation(
                expectedLibraryPath, s_bootstrapHandles, 800, 600, 2, s_hostImageBinding));
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
        return readyInputs(framebufferWidth, framebufferHeight, 1L);
    }

    /** Verifies fatal frame state vetoes direct resize detach without another Native operation. */
    @Test
    void fatalFrameFailureVetoesDirectResizeDetachWithoutNativeCall() {
        RecordingRuntimeFactory runtimeFactory = new RecordingRuntimeFactory();
        RecordingRuntime runtime = new RecordingRuntime("first", runtimeFactory.m_events);
        runtimeFactory.m_nextRuntime = runtime;
        PresentationTakeoverCoordinator coordinator = createCoordinator(runtimeFactory);
        assertTrue(coordinator.configure(readyInputs(800, 600)));
        runtime.m_beginFailure = runtimeFailure("begin");
        coordinator.beginFrame();

        assertFalse(coordinator.detachHostImagePresentationResources());

        assertEquals(0, runtime.m_detachCount);
        assertTrue(coordinator.isTakenOver());
        assertTrue(coordinator.requiresReconfiguration());
    }

    /**
     * @note ThreadSafety: Concurrency-safe immutable test-value creation.
     * Creates ready inputs with an explicit captured host-target generation.
     *
     * @param int framebufferWidth Positive test framebuffer width
     * @param int framebufferHeight Positive test framebuffer height
     * @param long hostTargetGeneration Positive captured host-target generation
     * @return PresentationGenerationInputs Ready immutable test inputs
     * @warning MemoryOwnership: Returned inputs borrow shared test handle values and binding data.
     */
    private static PresentationGenerationInputs readyInputs(
        int framebufferWidth,
        int framebufferHeight,
        long hostTargetGeneration) {
        HostImagePresentationBinding hostImageBinding = framebufferWidth == 800
            && framebufferHeight == 600
            ? s_hostImageBinding
            : new HostImagePresentationBinding(
                7L,
                PresentationImageFormat.B8G8R8A8_UNORM,
                PresentationImageFormat.B8G8R8A8_UNORM,
                framebufferWidth,
                framebufferHeight);
        return new PresentationGenerationInputs(
            s_bootstrapHandles,
            framebufferWidth,
            framebufferHeight,
            true,
            hostImageBinding,
            hostTargetGeneration);
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
        private boolean m_shouldReturnNull;
        private PresentationBootstrapHandles m_bootstrapHandles;
        private HostImagePresentationBinding m_hostImageBinding;
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
         * @param HostImagePresentationBinding hostImageBinding Borrowed exact host image binding
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
            int framesInFlightCount,
            HostImagePresentationBinding hostImageBinding)
            throws NativeLibraryLoadingException, NativePresentationRuntimeException {
            m_createCount++;
            m_bootstrapHandles = bootstrapHandles;
            m_hostImageBinding = hostImageBinding;
            m_framesInFlightCount = framesInFlightCount;
            if (m_loadingFailure != null) {
                m_events.add("create:failure");
                throw m_loadingFailure;
            }
            if (m_creationFailure != null) {
                m_events.add("create:failure");
                throw m_creationFailure;
            }
            if (m_shouldReturnNull) {
                m_events.add("create:null");
                return null;
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
        private NativePresentationRuntimeException m_detachFailure;
        private NativePresentationRuntimeException m_closeFailure;
        private int m_beginCount;
        private int m_presentCount;
        private int m_clearPresentCount;
        private int m_hostPresentCount;
        private int m_detachCount;
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
            m_events.add("clear:" + m_name);
            m_presentCount++;
            m_clearPresentCount++;
            m_clearRed = clearRed;
            m_clearGreen = clearGreen;
            m_clearBlue = clearBlue;
            if (m_submitFailure != null) {
                throw m_submitFailure;
            }
            return m_submitStatus;
        }

        /**
         * @note ThreadSafety: Confined to the invoking test thread.
         * Records one host-resource detach attempt and optionally fails.
         *
         * @throws NativePresentationRuntimeException Configured checked detach failure
         * @warning MemoryOwnership: Models ending the runtime borrow without owning a host image.
         */
        @Override
        public void detachHostImagePresentationResources()
            throws NativePresentationRuntimeException {
            m_events.add("detach:" + m_name);
            m_detachCount++;
            if (m_detachFailure != null) {
                throw m_detachFailure;
            }
        }

        /**
         * @note ThreadSafety: Confined to the invoking test thread.
         * Records one host-image submission and returns the configured submit status.
         *
         * @return PresentationFrameStatus Configured semantic host submission status
         * @throws NativePresentationRuntimeException Configured checked submission failure
         * @warning MemoryOwnership: Observes fake borrowed host-image state without ownership transfer.
         */
        @Override
        public PresentationFrameStatus submitAndPresentHostImageFrame()
            throws NativePresentationRuntimeException {
            m_events.add("host:" + m_name);
            m_presentCount++;
            m_hostPresentCount++;
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
