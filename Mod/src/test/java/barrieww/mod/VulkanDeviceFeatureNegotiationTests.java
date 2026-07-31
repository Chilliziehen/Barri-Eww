package barrieww.mod;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertNotSame;
import static org.junit.jupiter.api.Assertions.assertTrue;

import com.mojang.blaze3d.vulkan.init.VulkanFeature;
import com.mojang.blaze3d.vulkan.init.VulkanPNextStruct;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.concurrent.atomic.AtomicInteger;
import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.Test;
import org.slf4j.Logger;

/**
 * @note ThreadSafety: Tests use thread-confined feature sets, counters, and logging captures.
 * Verifies pure device-feature negotiation and exact capability publication behavior.
 */
final class VulkanDeviceFeatureNegotiationTests {
    private static final VulkanPNextStruct s_testFeatureStructure =
        new VulkanPNextStruct(51, 64);
    private static final VulkanFeature s_dynamicRenderingFeature = new VulkanFeature(
        s_testFeatureStructure,
        "dynamicRendering",
        8L);
    private static final VulkanFeature s_bufferDeviceAddressFeature = new VulkanFeature(
        s_testFeatureStructure,
        "bufferDeviceAddress",
        12L);

    /** Resets process-wide capability publication after each test. */
    @AfterEach
    void resetCapabilityState() {
        VulkanDeviceCapabilityState.resetForTests();
    }

    /** Verifies supported bufferDeviceAddress is added to a copied feature set exactly once. */
    @Test
    void supportedBufferDeviceAddressIsAddedToFeatureCopy() {
        AtomicInteger supportQueryCount = new AtomicInteger();
        Set<VulkanFeature> incomingFeatures = Set.of(s_dynamicRenderingFeature);

        VulkanDeviceFeatureNegotiation.DeviceFeatureNegotiation negotiation =
            VulkanDeviceFeatureNegotiation.negotiate(
                Set.of(s_dynamicRenderingFeature),
                incomingFeatures,
                s_bufferDeviceAddressFeature,
                () -> {
                    supportQueryCount.incrementAndGet();
                    return true;
                },
                createCapturingLogger(new ArrayList<>(), new ArrayList<>()));

        assertNotSame(incomingFeatures, negotiation.negotiatedFeatures());
        assertEquals(
            Set.of(s_dynamicRenderingFeature, s_bufferDeviceAddressFeature),
            negotiation.negotiatedFeatures());
        assertTrue(negotiation.isBufferDeviceAddressNegotiated());
        assertEquals(1, supportQueryCount.get());
    }

    /** Verifies unsupported bufferDeviceAddress leaves the copied incoming features unchanged. */
    @Test
    void unsupportedBufferDeviceAddressLeavesFeatureCopyUnchanged() {
        Set<VulkanFeature> incomingFeatures = Set.of(s_dynamicRenderingFeature);

        VulkanDeviceFeatureNegotiation.DeviceFeatureNegotiation negotiation =
            VulkanDeviceFeatureNegotiation.negotiate(
                Set.of(s_dynamicRenderingFeature),
                incomingFeatures,
                s_bufferDeviceAddressFeature,
                () -> false,
                createCapturingLogger(new ArrayList<>(), new ArrayList<>()));

        assertNotSame(incomingFeatures, negotiation.negotiatedFeatures());
        assertEquals(incomingFeatures, negotiation.negotiatedFeatures());
        assertFalse(negotiation.isBufferDeviceAddressNegotiated());
    }

    /** Verifies missing dynamicRendering contains readiness without querying device support. */
    @Test
    void missingDynamicRenderingWarnsOnceWithoutSupportQuery() {
        AtomicInteger supportQueryCount = new AtomicInteger();
        List<String> loggingMethodNames = new ArrayList<>();
        List<Object[]> loggingArguments = new ArrayList<>();
        Logger logger = createCapturingLogger(loggingMethodNames, loggingArguments);

        VulkanDeviceFeatureNegotiation.DeviceFeatureNegotiation negotiation =
            VulkanDeviceFeatureNegotiation.negotiate(
                Set.of(s_dynamicRenderingFeature),
                Set.of(),
                s_bufferDeviceAddressFeature,
                () -> {
                    supportQueryCount.incrementAndGet();
                    return true;
                },
                logger);

        assertFalse(negotiation.isBufferDeviceAddressNegotiated());
        assertTrue(negotiation.negotiatedFeatures().isEmpty());
        assertEquals(0, supportQueryCount.get());
        assertEquals(List.of("warn"), loggingMethodNames);
        assertEquals(1, loggingArguments.size());
        assertEquals(true, loggingArguments.getFirst()[1]);
        assertEquals(false, loggingArguments.getFirst()[2]);
    }

    /** Verifies publication records the exact logical-device address and logs one result. */
    @Test
    void capabilityPublicationRecordsExactLogicalDevice() {
        List<String> loggingMethodNames = new ArrayList<>();
        List<Object[]> loggingArguments = new ArrayList<>();
        Logger logger = createCapturingLogger(loggingMethodNames, loggingArguments);

        VulkanDeviceFeatureNegotiation.publishCapability(41L, true, logger);

        assertTrue(VulkanDeviceCapabilityState.isBufferDeviceAddressEnabled(41L));
        assertFalse(VulkanDeviceCapabilityState.isBufferDeviceAddressEnabled(42L));
        assertEquals(List.of("info"), loggingMethodNames);
        assertEquals(1, loggingArguments.size());
        assertEquals(true, loggingArguments.getFirst()[1]);
        assertEquals("29", loggingArguments.getFirst()[2]);
    }

    /**
     * @note ThreadSafety: Not thread-safe. Each test owns its mutable capture lists.
     * Creates an SLF4J proxy that captures warning and informational calls.
     *
     * @param List<String> loggingMethodNames Destination for invoked logging method names
     * @param List<Object[]> loggingArguments Destination for exact logging arguments
     * @return Logger Capturing logger proxy
     */
    private static Logger createCapturingLogger(
        List<String> loggingMethodNames,
        List<Object[]> loggingArguments) {
        return (Logger) Proxy.newProxyInstance(
            Logger.class.getClassLoader(),
            new Class<?>[] {Logger.class},
            (proxy, method, arguments) -> {
                if (method.getName().equals("warn") || method.getName().equals("info")) {
                    loggingMethodNames.add(method.getName());
                    loggingArguments.add(arguments);
                }
                if (method.getReturnType().equals(boolean.class)) {
                    return false;
                }
                return null;
            });
    }
}
