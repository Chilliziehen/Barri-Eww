package barrieww.mod;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.lang.reflect.Field;
import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

/**
 * @note ThreadSafety: Tests serialize access to the render-thread-confined static tracker.
 * Verifies listener identity, resize veto propagation and monotonic main-target publication.
 * @warning MemoryOwnership: Test listeners remain test-owned and are unregistered after each test.
 */
final class MainRenderTargetGenerationTrackerTests {
    private MainRenderTargetResizeListener m_registeredListener;

    /**
     * @note ThreadSafety: Runs serially before each tracker test on the test thread.
     * Restores the production initial generation so each test observes an isolated lifecycle.
     *
     * @throws ReflectiveOperationException When the private tracker generation cannot be restored
     * @warning MemoryOwnership: Reflection borrows the static field and retains no tracker state.
     */
    @BeforeEach
    void resetGeneration() throws ReflectiveOperationException {
        Field generationField = MainRenderTargetGenerationTracker.class.getDeclaredField(
            "s_currentGeneration");
        generationField.setAccessible(true);
        generationField.setLong(null, 1L);
    }

    /**
     * @note ThreadSafety: Runs serially on the same test thread as tracker access.
     * Removes the exact listener retained by a completed test.
     * @warning MemoryOwnership: Successful unregister releases the static borrowed reference.
     */
    @AfterEach
    void unregisterListener() {
        if (m_registeredListener != null) {
            MainRenderTargetGenerationTracker.unregisterResizeListener(m_registeredListener);
        }
    }

    /** Verifies duplicate registration is rejected without replacing the active listener. */
    @Test
    void duplicateRegistrationPreservesFirstListener() {
        m_registeredListener = () -> false;
        MainRenderTargetGenerationTracker.registerResizeListener(m_registeredListener);

        assertThrows(IllegalStateException.class, () ->
            MainRenderTargetGenerationTracker.registerResizeListener(() -> true));
        assertFalse(MainRenderTargetGenerationTracker.beforeMainRenderTargetResize());
    }

    /** Verifies a successful before callback permits resize and a failed callback vetoes it. */
    @Test
    void beforeResizeDelegatesExactBooleanOutcome() {
        m_registeredListener = () -> true;
        MainRenderTargetGenerationTracker.registerResizeListener(m_registeredListener);
        assertTrue(MainRenderTargetGenerationTracker.beforeMainRenderTargetResize());
        MainRenderTargetGenerationTracker.unregisterResizeListener(m_registeredListener);

        m_registeredListener = () -> false;
        MainRenderTargetGenerationTracker.registerResizeListener(m_registeredListener);
        assertFalse(MainRenderTargetGenerationTracker.beforeMainRenderTargetResize());
    }

    /** Verifies only a successful resize of the current main target publishes one generation. */
    @Test
    void successfulCurrentTargetTailPublishesMonotonically() {
        Object currentMainRenderTarget = new Object();
        long initialGeneration = MainRenderTargetGenerationTracker.currentGeneration();

        MainRenderTargetGenerationTracker.mainRenderTargetResizeSucceeded(
            new Object(), currentMainRenderTarget);
        assertEquals(initialGeneration, MainRenderTargetGenerationTracker.currentGeneration());

        MainRenderTargetGenerationTracker.mainRenderTargetResizeSucceeded(
            currentMainRenderTarget, currentMainRenderTarget);
        assertEquals(initialGeneration + 1,
            MainRenderTargetGenerationTracker.currentGeneration());
    }

    /** Verifies listener registration preserves a generation already advanced by successful resize. */
    @Test
    void listenerRegistrationDoesNotResetLaterGeneration() {
        Object currentMainRenderTarget = new Object();
        MainRenderTargetGenerationTracker.mainRenderTargetResizeSucceeded(
            currentMainRenderTarget,
            currentMainRenderTarget);
        long generationAfterResize = MainRenderTargetGenerationTracker.currentGeneration();
        m_registeredListener = () -> true;

        MainRenderTargetGenerationTracker.registerResizeListener(m_registeredListener);

        assertEquals(2L, generationAfterResize);
        assertEquals(
            generationAfterResize,
            MainRenderTargetGenerationTracker.currentGeneration());
    }

    /** Verifies failed or cancelled resize paths leave the generation unchanged. */
    @Test
    void failedResizeDoesNotPublishGeneration() {
        long initialGeneration = MainRenderTargetGenerationTracker.currentGeneration();
        m_registeredListener = () -> false;
        MainRenderTargetGenerationTracker.registerResizeListener(m_registeredListener);

        assertFalse(MainRenderTargetGenerationTracker.beforeMainRenderTargetResize());
        assertEquals(initialGeneration, MainRenderTargetGenerationTracker.currentGeneration());
    }

    /** Verifies null unregister reports absent when no listener is active. */
    @Test
    void unregisterNullWithoutActiveListenerReturnsFalse() {
        assertFalse(MainRenderTargetGenerationTracker.unregisterResizeListener(null));
        assertTrue(MainRenderTargetGenerationTracker.beforeMainRenderTargetResize());
    }

    /** Verifies null unregister cannot remove an active listener. */
    @Test
    void unregisterNullPreservesActiveListener() {
        m_registeredListener = () -> false;
        MainRenderTargetGenerationTracker.registerResizeListener(m_registeredListener);

        assertFalse(MainRenderTargetGenerationTracker.unregisterResizeListener(null));
        assertFalse(MainRenderTargetGenerationTracker.beforeMainRenderTargetResize());
    }

    /** Verifies a wrong listener identity cannot remove an active listener. */
    @Test
    void unregisterWrongIdentityPreservesActiveListener() {
        m_registeredListener = () -> false;
        MainRenderTargetGenerationTracker.registerResizeListener(m_registeredListener);

        assertFalse(MainRenderTargetGenerationTracker.unregisterResizeListener(() -> false));
        assertFalse(MainRenderTargetGenerationTracker.beforeMainRenderTargetResize());
    }

    /** Verifies the exact registered identity releases listener retention. */
    @Test
    void unregisterExactIdentityReleasesRetention() {
        m_registeredListener = () -> false;
        MainRenderTargetGenerationTracker.registerResizeListener(m_registeredListener);

        assertTrue(MainRenderTargetGenerationTracker.unregisterResizeListener(m_registeredListener));
        m_registeredListener = null;
        assertTrue(MainRenderTargetGenerationTracker.beforeMainRenderTargetResize());

        MainRenderTargetResizeListener replacementListener = () -> true;
        MainRenderTargetGenerationTracker.registerResizeListener(replacementListener);
        m_registeredListener = replacementListener;
        assertTrue(MainRenderTargetGenerationTracker.beforeMainRenderTargetResize());
    }
}
