package barrieww.mod;

import static org.junit.jupiter.api.Assertions.assertDoesNotThrow;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertInstanceOf;
import static org.junit.jupiter.api.Assertions.assertSame;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import org.junit.jupiter.api.Test;
import org.slf4j.Logger;

/**
 * @note ThreadSafety: Tests use immutable resource data and content-addressed extraction.
 * Verifies platform resource mapping and non-crashing Native-library readiness behavior.
 */
final class BarriEwwClientInitializerTests {
    /**
     * @note ThreadSafety: Safe to run concurrently because platform mapping is stateless.
     * Verifies supported Windows and Linux x86_64 runtime names map to exact resources.
     */
    @Test
    void supportedPlatformsMapToExactNativeResources() {
        assertEquals(
            Optional.of("barrieww/native/windows-x86_64/BarriEwwNativeFfm.dll"),
            BarriEwwClientInitializer.resolveNativeLibraryResource("Windows 11", "amd64"));
        assertEquals(
            Optional.of("barrieww/native/linux-x86_64/libBarriEwwNativeFfm.so"),
            BarriEwwClientInitializer.resolveNativeLibraryResource("Linux", "x86_64"));
    }

    /**
     * @note ThreadSafety: Safe to run concurrently because platform mapping is stateless.
     * Verifies unsupported operating systems and architectures report unavailable readiness.
     */
    @Test
    void unsupportedPlatformsAreUnavailableWithoutExtractionFailure()
        throws ReflectiveOperationException {
        assertTrue(BarriEwwClientInitializer.resolveNativeLibraryResource(
            "macOS", "x86_64").isEmpty());
        assertTrue(BarriEwwClientInitializer.resolveNativeLibraryResource(
            "Windows 11", "aarch64").isEmpty());
        assertTrue(BarriEwwClientInitializer.resolveNativeLibraryResource(
            null, "amd64").isEmpty());
        assertTrue(BarriEwwClientInitializer.resolveNativeLibraryResource(
            "Windows 11", null).isEmpty());
        assertTrue(BarriEwwClientInitializer.resolveNativeLibraryPath(
            getClass().getClassLoader(), "macOS", "x86_64").isEmpty());

        Object nativeLibraryReadiness = resolveNativeLibraryReadiness(
            getClass().getClassLoader(),
            "macOS",
            "x86_64");
        assertEquals("macOS", invokeReadinessAccessor(
            nativeLibraryReadiness, "operatingSystemName"));
        assertEquals("x86_64", invokeReadinessAccessor(
            nativeLibraryReadiness, "architectureName"));
        assertTrue(((Optional<?>) invokeReadinessAccessor(
            nativeLibraryReadiness, "nativeLibraryResource")).isEmpty());
        assertTrue(((Optional<?>) invokeReadinessAccessor(
            nativeLibraryReadiness, "extractionException")).isEmpty());
        List<String> loggingMethodNames = new ArrayList<>();
        List<Object[]> loggingArguments = new ArrayList<>();
        Logger logger = createCapturingLogger(loggingMethodNames, loggingArguments);

        invokeReadinessLogging(logger, nativeLibraryReadiness);

        assertEquals(List.of("warn"), loggingMethodNames);
        assertEquals(1, loggingArguments.size());
        Object[] warningArguments = loggingArguments.getFirst();
        assertEquals(3, warningArguments.length);
        assertTrue(((String) warningArguments[0]).contains("operating system"));
        assertEquals("macOS", warningArguments[1]);
        assertEquals("x86_64", warningArguments[2]);
        assertTrue(List.of(warningArguments).stream().noneMatch(Throwable.class::isInstance));
    }

    /**
     * @note ThreadSafety: Safe to run concurrently because extraction is content-addressed.
     * Verifies a supported embedded resource produces a readable absolute readiness path.
     *
     * @throws IOException When the extracted test resource cannot be inspected
     * @throws ReflectiveOperationException When private readiness behavior cannot be invoked
     */
    @Test
    void supportedEmbeddedResourceBecomesAvailable()
        throws IOException, ReflectiveOperationException {
        byte[] nativeLibraryBytes = new byte[] {0x42, 0x45, 0x57, 0x57};
        ClassLoader resourceClassLoader = new SingleResourceClassLoader(
            "barrieww/native/windows-x86_64/BarriEwwNativeFfm.dll",
            nativeLibraryBytes);

        Object nativeLibraryReadiness = resolveNativeLibraryReadiness(
            resourceClassLoader,
            "Windows 11",
            "amd64");
        Optional<?> nativeLibraryPath = (Optional<?>) invokeReadinessAccessor(
            nativeLibraryReadiness,
            "nativeLibraryPath");

        assertTrue(nativeLibraryPath.isPresent());
        Path extractedLibraryPath = assertInstanceOf(Path.class, nativeLibraryPath.orElseThrow());
        assertTrue(extractedLibraryPath.isAbsolute());
        assertEquals(4L, Files.size(extractedLibraryPath));
        List<String> loggingMethodNames = new ArrayList<>();
        List<Object[]> loggingArguments = new ArrayList<>();
        Logger logger = createCapturingLogger(loggingMethodNames, loggingArguments);
        invokeReadinessLogging(logger, nativeLibraryReadiness);
        assertEquals(List.of("info"), loggingMethodNames);
    }

    /**
     * @note ThreadSafety: Safe to run concurrently because the class loader is immutable.
     * Verifies a missing supported-platform resource reports unavailable without throwing.
     */
    @Test
    void missingSupportedResourceRetainsFullFailureContext()
        throws ReflectiveOperationException {
        Object nativeLibraryReadiness = resolveNativeLibraryReadiness(
            new SingleResourceClassLoader("unrelated/resource", new byte[] {0x00}),
            "Windows 11",
            "amd64");

        assertTrue(((Optional<?>) invokeReadinessAccessor(
            nativeLibraryReadiness, "nativeLibraryPath")).isEmpty());
        assertEquals("Windows 11", invokeReadinessAccessor(
            nativeLibraryReadiness, "operatingSystemName"));
        assertEquals("amd64", invokeReadinessAccessor(
            nativeLibraryReadiness, "architectureName"));
        assertEquals(
            Optional.of("barrieww/native/windows-x86_64/BarriEwwNativeFfm.dll"),
            invokeReadinessAccessor(nativeLibraryReadiness, "nativeLibraryResource"));
        Optional<?> extractionException = (Optional<?>) invokeReadinessAccessor(
            nativeLibraryReadiness,
            "extractionException");
        IOException checkedExtractionException = assertInstanceOf(
            IOException.class,
            extractionException.orElseThrow());
        assertTrue(checkedExtractionException.getMessage().contains(
            "barrieww/native/windows-x86_64/BarriEwwNativeFfm.dll"));
        List<String> loggingMethodNames = new ArrayList<>();
        List<Object[]> loggingArguments = new ArrayList<>();
        Logger logger = createCapturingLogger(loggingMethodNames, loggingArguments);

        invokeReadinessLogging(logger, nativeLibraryReadiness);

        assertEquals(List.of("warn"), loggingMethodNames);
        assertEquals(1, loggingArguments.size());
        Object[] warningArguments = loggingArguments.getFirst();
        assertEquals(2, warningArguments.length);
        String warningMessage = assertInstanceOf(String.class, warningArguments[0]);
        assertTrue(warningMessage.contains("Windows 11"));
        assertTrue(warningMessage.contains("amd64"));
        assertTrue(warningMessage.contains(
            "barrieww/native/windows-x86_64/BarriEwwNativeFfm.dll"));
        assertSame(checkedExtractionException, warningArguments[1]);
    }

    /**
     * @note ThreadSafety: Fabric initialization is expected to run once; this test invokes it once.
     * Verifies immutable published readiness initializes without crashing whether the selected build
     * classpath contains the packaged Native resource or intentionally omits it.
     */
    @Test
    void clientInitializationPublishesReadinessWithoutCrashing() {
        BarriEwwClientInitializer clientInitializer = new BarriEwwClientInitializer();

        assertDoesNotThrow(clientInitializer::onInitializeClient);
    }

    /**
     * @note ThreadSafety: Safe to run concurrently because readiness creation uses only supplied
     * immutable inputs and content-addressed extraction.
     * Invokes the private readiness factory so tests can verify retained failure context without
     * widening the production API.
     *
     * @param ClassLoader resourceClassLoader Class loader containing test resources
     * @param String operatingSystemName Runtime operating-system name
     * @param String architectureName Runtime processor architecture name
     * @return Object Private immutable readiness result
     * @throws ReflectiveOperationException When the readiness factory cannot be invoked
     */
    private static Object resolveNativeLibraryReadiness(
        ClassLoader resourceClassLoader,
        String operatingSystemName,
        String architectureName) throws ReflectiveOperationException {
        Method readinessFactoryMethod = BarriEwwClientInitializer.class.getDeclaredMethod(
            "resolveNativeLibraryReadiness",
            ClassLoader.class,
            String.class,
            String.class);
        readinessFactoryMethod.setAccessible(true);
        return readinessFactoryMethod.invoke(
            null,
            resourceClassLoader,
            operatingSystemName,
            architectureName);
    }

    /**
     * @note ThreadSafety: Safe to run concurrently because readiness results are immutable.
     * Invokes one private readiness accessor without widening the production API.
     *
     * @param Object nativeLibraryReadiness Private immutable readiness result
     * @param String accessorName Exact private accessor method name
     * @return Object Accessor result
     * @throws ReflectiveOperationException When the accessor cannot be invoked
     */
    private static Object invokeReadinessAccessor(
        Object nativeLibraryReadiness,
        String accessorName) throws ReflectiveOperationException {
        Method readinessAccessorMethod = nativeLibraryReadiness.getClass().getDeclaredMethod(
            accessorName);
        readinessAccessorMethod.setAccessible(true);
        return readinessAccessorMethod.invoke(nativeLibraryReadiness);
    }

    /**
     * @note ThreadSafety: Safe to run concurrently for immutable readiness results; logger ordering
     * is delegated to the established SLF4J implementation.
     * Invokes production readiness logging to cover success and unsupported warning behavior.
     *
     * @param Logger logger Logger receiving exactly one readiness outcome
     * @param Object nativeLibraryReadiness Private immutable readiness result
     * @throws ReflectiveOperationException When the logging method cannot be invoked
     */
    private static void invokeReadinessLogging(
        Logger logger,
        Object nativeLibraryReadiness)
        throws ReflectiveOperationException {
        Method readinessLoggingMethod = BarriEwwClientInitializer.class.getDeclaredMethod(
            "logNativeLibraryReadiness",
            Logger.class,
            nativeLibraryReadiness.getClass());
        readinessLoggingMethod.setAccessible(true);
        readinessLoggingMethod.invoke(null, logger, nativeLibraryReadiness);
    }

    /**
     * @note ThreadSafety: Not thread-safe. Each test owns and mutates its capture lists from one
     * thread.
     * Creates an SLF4J proxy that records logging method names and exact invocation arguments.
     *
     * @param List<String> loggingMethodNames Destination for invoked logging method names
     * @param List<Object[]> loggingArguments Destination for exact logging invocation arguments
     * @return Logger Capturing logger proxy
     */
    private static Logger createCapturingLogger(
        List<String> loggingMethodNames,
        List<Object[]> loggingArguments) {
        return (Logger) Proxy.newProxyInstance(
            BarriEwwClientInitializerTests.class.getClassLoader(),
            new Class<?>[] {Logger.class},
            (loggerProxy, loggingMethod, methodArguments) -> {
                if (loggingMethod.getName().equals("warn") ||
                    loggingMethod.getName().equals("info")) {
                    loggingMethodNames.add(loggingMethod.getName());
                    loggingArguments.add(methodArguments.clone());
                }
                return null;
            });
    }

    /**
     * @note ThreadSafety: Immutable after construction and safe for concurrent resource reads.
     * Supplies one exact in-memory resource to initializer readiness tests.
     */
    private static final class SingleResourceClassLoader extends ClassLoader {
        private final String m_resourceName;
        private final byte[] m_resourceBytes;

        /**
         * @note ThreadSafety: Construction requires exclusive access; the resulting instance is
         * immutable and concurrency-safe.
         * Creates a class loader containing one resource.
         *
         * @param String resourceName Available class-path resource name
         * @param byte[] resourceBytes Exact bytes returned for the resource
         */
        private SingleResourceClassLoader(String resourceName, byte[] resourceBytes) {
            super(null);
            m_resourceName = resourceName;
            m_resourceBytes = resourceBytes.clone();
        }

        /**
         * @note ThreadSafety: Concurrency-safe because immutable bytes back independent streams.
         * Opens the configured resource, or returns null for every other name.
         *
         * @param String resourceName Requested class-path resource name
         * @return InputStream Independent stream for the configured resource, or null when absent
         */
        @Override
        public InputStream getResourceAsStream(String resourceName) {
            if (!m_resourceName.equals(resourceName)) {
                return null;
            }
            return new ByteArrayInputStream(m_resourceBytes);
        }
    }
}
