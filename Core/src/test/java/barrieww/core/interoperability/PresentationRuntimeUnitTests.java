package barrieww.core.interoperability;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import org.junit.jupiter.api.Test;

/**
 * @note ThreadSafety: JUnit creates a fresh instance per test method; no shared state.
 * Covers presentation frame-status decoding and metrics record decoding of the pure value
 * types without loading the Native library or the demo binding.
 */
class PresentationRuntimeUnitTests {

    @Test
    void everyFrameStatusRoundTripsItsStableCode() {
        for (PresentationFrameStatus status : PresentationFrameStatus.values()) {
            assertEquals(status, PresentationFrameStatus.fromCode(status.code()));
        }
        assertThrows(IllegalArgumentException.class,
                () -> PresentationFrameStatus.fromCode(4));
        assertThrows(IllegalArgumentException.class,
                () -> PresentationFrameStatus.fromCode(-1));
    }

    @Test
    void frameMetricsLayoutMatchesTheNativeRecordSize() {
        assertEquals(112L, PresentationFrameMetrics.s_byteSize);
    }

    @Test
    void frameMetricsDecodeReadsEveryFixedField() {
        try (Arena testArena = Arena.ofConfined()) {
            MemorySegment metricsSegment =
                    testArena.allocate(PresentationFrameMetrics.s_byteSize, 8);
            metricsSegment.set(ValueLayout.JAVA_LONG, 0, 42L);   // frameSequence
            metricsSegment.set(ValueLayout.JAVA_LONG, 8, 7L);    // swapchainGeneration
            metricsSegment.set(ValueLayout.JAVA_INT, 16, 1);     // frameSlotIndex
            metricsSegment.set(ValueLayout.JAVA_INT, 20, 2);     // imageIndex
            metricsSegment.set(ValueLayout.JAVA_INT, 24,
                    PresentationFrameMetrics.s_cpuMetricsValidFlag);
            metricsSegment.set(ValueLayout.JAVA_INT, 28, 0);     // presentResultValue
            metricsSegment.set(ValueLayout.JAVA_LONG, 32, 100L); // fenceWaitNanoseconds
            metricsSegment.set(ValueLayout.JAVA_LONG, 40, 200L); // acquireNanoseconds
            metricsSegment.set(ValueLayout.JAVA_LONG, 48, 300L); // nativeSubmitCallNanoseconds
            metricsSegment.set(ValueLayout.JAVA_LONG, 56, 400L); // presentCallNanoseconds
            metricsSegment.set(ValueLayout.JAVA_LONG, 64, 500L); // totalCpuFrameNanoseconds
            metricsSegment.set(ValueLayout.JAVA_LONG, 72, 0L);
            metricsSegment.set(ValueLayout.JAVA_LONG, 80, 0L);
            metricsSegment.set(ValueLayout.JAVA_LONG, 88, 0L);
            metricsSegment.set(ValueLayout.JAVA_LONG, 96, 0L);
            metricsSegment.set(ValueLayout.JAVA_INT, 104, 1);    // presentModeValue (MAILBOX)
            metricsSegment.set(ValueLayout.JAVA_INT, 108, 0);    // sharingModeValue

            PresentationFrameMetrics metrics =
                    PresentationFrameMetrics.decode(metricsSegment);
            assertEquals(42L, metrics.frameSequence());
            assertEquals(7L, metrics.swapchainGeneration());
            assertEquals(1, metrics.frameSlotIndex());
            assertEquals(2, metrics.imageIndex());
            assertEquals(100L, metrics.fenceWaitNanoseconds());
            assertEquals(200L, metrics.acquireNanoseconds());
            assertEquals(300L, metrics.nativeSubmitCallNanoseconds());
            assertEquals(400L, metrics.presentCallNanoseconds());
            assertEquals(500L, metrics.totalCpuFrameNanoseconds());
            assertEquals(1, metrics.presentModeValue());
            assertTrue(metrics.cpuMetricsValid());
            assertFalse(metrics.gpuMetricsValid());
        }
    }
}
