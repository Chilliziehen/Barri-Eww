package barrieww.core.demo;

import static org.junit.jupiter.api.Assertions.assertDoesNotThrow;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertSame;
import static org.junit.jupiter.api.Assertions.assertThrows;

import barrieww.core.interoperability.NativePresentationRuntimeException;
import org.junit.jupiter.api.Test;

/**
 * @note ThreadSafety: JUnit creates a fresh instance per test method; no shared state.
 * Covers visible-demo teardown failure precedence without opening a window.
 */
class VisibleClearDemoLifecycleTests {

    @Test
    void closeFailureIsSuppressedOnEveryPrimaryThrowableCategory() {
        Throwable[] primaryFailures = {
            new IllegalStateException("Primary runtime failure"),
            new AssertionError("Primary error")
        };
        for (Throwable primaryFailure : primaryFailures) {
            NativePresentationRuntimeException closeFailure = closeFailure();

            assertDoesNotThrow(() -> VisibleClearDemo.handlePresentationRuntimeCloseFailure(
                    primaryFailure, closeFailure));

            assertEquals(1, primaryFailure.getSuppressed().length);
            assertSame(closeFailure, primaryFailure.getSuppressed()[0]);
        }
    }

    @Test
    void closeFailureWithoutPrimaryFailureIsSurfaced() {
        NativePresentationRuntimeException closeFailure = closeFailure();

        IllegalStateException surfacedFailure = assertThrows(IllegalStateException.class,
                () -> VisibleClearDemo.handlePresentationRuntimeCloseFailure(null, closeFailure));

        assertSame(closeFailure, surfacedFailure.getCause());
    }

    /**
     * Creates one deterministic checked close failure.
     *
     * @return NativePresentationRuntimeException Test close failure
     */
    private static NativePresentationRuntimeException closeFailure() {
        return new NativePresentationRuntimeException(
                "Injected close failure", "barriEwwDestroyPresentationRuntimeVersion1", 4, 0);
    }
}
