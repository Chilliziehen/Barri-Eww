package barrieww.core.interoperability;

import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;

/**
 * @note ThreadSafety: Immutable value; safe to share across threads.
 * Completed-frame CPU/GPU timings and identity decoded from the Version 1 FrameMetrics ABI
 * (§6.7.3). GPU nanosecond fields are valid only when the GPU-metrics bit is set in
 * validFlags; the Java-measured parameter-write duration is not part of the native record.
 */
public record PresentationFrameMetrics(
        long frameSequence,
        long swapchainGeneration,
        int frameSlotIndex,
        int imageIndex,
        int validFlags,
        int presentResultValue,
        long fenceWaitNanoseconds,
        long acquireNanoseconds,
        long nativeSubmitCallNanoseconds,
        long presentCallNanoseconds,
        long totalCpuFrameNanoseconds,
        long computeGpuNanoseconds,
        long graphicsGpuNanoseconds,
        long finalTransferGpuNanoseconds,
        long totalSubmittedGpuNanoseconds,
        int presentModeValue,
        int sharingModeValue) {

    /** The validFlags bit set once a completed frame recorded CPU timings. */
    public static final int s_cpuMetricsValidFlag = 0x1;

    /** The validFlags bit set once a completed frame recorded GPU timestamp timings. */
    public static final int s_gpuMetricsValidFlag = 0x2;

    /** The fixed native FrameMetricsVersion1 record layout (112 bytes). */
    public static final MemoryLayout s_layout = MemoryLayout.structLayout(
            ValueLayout.JAVA_LONG.withName("frameSequence"),
            ValueLayout.JAVA_LONG.withName("swapchainGeneration"),
            ValueLayout.JAVA_INT.withName("frameSlotIndex"),
            ValueLayout.JAVA_INT.withName("imageIndex"),
            ValueLayout.JAVA_INT.withName("validFlags"),
            ValueLayout.JAVA_INT.withName("presentResultValue"),
            ValueLayout.JAVA_LONG.withName("fenceWaitNanoseconds"),
            ValueLayout.JAVA_LONG.withName("acquireNanoseconds"),
            ValueLayout.JAVA_LONG.withName("nativeSubmitCallNanoseconds"),
            ValueLayout.JAVA_LONG.withName("presentCallNanoseconds"),
            ValueLayout.JAVA_LONG.withName("totalCpuFrameNanoseconds"),
            ValueLayout.JAVA_LONG.withName("computeGpuNanoseconds"),
            ValueLayout.JAVA_LONG.withName("graphicsGpuNanoseconds"),
            ValueLayout.JAVA_LONG.withName("finalTransferGpuNanoseconds"),
            ValueLayout.JAVA_LONG.withName("totalSubmittedGpuNanoseconds"),
            ValueLayout.JAVA_INT.withName("presentModeValue"),
            ValueLayout.JAVA_INT.withName("sharingModeValue"));

    /** The native record byte size (112). */
    public static final long s_byteSize = s_layout.byteSize();

    /** Whether the completed frame's CPU segment timings are valid. */
    public boolean cpuMetricsValid() {
        return (validFlags & s_cpuMetricsValidFlag) != 0;
    }

    /** Whether the completed frame's GPU timestamp timings are valid. */
    public boolean gpuMetricsValid() {
        return (validFlags & s_gpuMetricsValidFlag) != 0;
    }

    /**
     * Decodes one native FrameMetricsVersion1 record from a 112-byte segment.
     *
     * @param MemorySegment metricsSegment Live segment holding the fixed-layout record
     * @return PresentationFrameMetrics The decoded immutable metrics value
     */
    public static PresentationFrameMetrics decode(MemorySegment metricsSegment) {
        return new PresentationFrameMetrics(
                metricsSegment.get(ValueLayout.JAVA_LONG, 0),
                metricsSegment.get(ValueLayout.JAVA_LONG, 8),
                metricsSegment.get(ValueLayout.JAVA_INT, 16),
                metricsSegment.get(ValueLayout.JAVA_INT, 20),
                metricsSegment.get(ValueLayout.JAVA_INT, 24),
                metricsSegment.get(ValueLayout.JAVA_INT, 28),
                metricsSegment.get(ValueLayout.JAVA_LONG, 32),
                metricsSegment.get(ValueLayout.JAVA_LONG, 40),
                metricsSegment.get(ValueLayout.JAVA_LONG, 48),
                metricsSegment.get(ValueLayout.JAVA_LONG, 56),
                metricsSegment.get(ValueLayout.JAVA_LONG, 64),
                metricsSegment.get(ValueLayout.JAVA_LONG, 72),
                metricsSegment.get(ValueLayout.JAVA_LONG, 80),
                metricsSegment.get(ValueLayout.JAVA_LONG, 88),
                metricsSegment.get(ValueLayout.JAVA_LONG, 96),
                metricsSegment.get(ValueLayout.JAVA_INT, 104),
                metricsSegment.get(ValueLayout.JAVA_INT, 108));
    }
}
