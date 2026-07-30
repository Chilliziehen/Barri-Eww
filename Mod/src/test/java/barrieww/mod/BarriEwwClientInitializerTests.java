package barrieww.mod;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Optional;
import org.junit.jupiter.api.Test;

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
    void unsupportedPlatformsAreUnavailable() {
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
    }

    /**
     * @note ThreadSafety: Safe to run concurrently because extraction is content-addressed.
     * Verifies a supported embedded resource produces a readable absolute readiness path.
     *
     * @throws Exception When the extracted test resource cannot be inspected
     */
    @Test
    void supportedEmbeddedResourceBecomesAvailable() throws Exception {
        byte[] nativeLibraryBytes = new byte[] {0x42, 0x45, 0x57, 0x57};
        ClassLoader resourceClassLoader = new SingleResourceClassLoader(
            "barrieww/native/windows-x86_64/BarriEwwNativeFfm.dll",
            nativeLibraryBytes);

        Optional<Path> nativeLibraryPath = BarriEwwClientInitializer.resolveNativeLibraryPath(
            resourceClassLoader,
            "Windows 11",
            "amd64");

        assertTrue(nativeLibraryPath.isPresent());
        assertTrue(nativeLibraryPath.orElseThrow().isAbsolute());
        assertEquals(4L, Files.size(nativeLibraryPath.orElseThrow()));
    }

    /**
     * @note ThreadSafety: Safe to run concurrently because the class loader is immutable.
     * Verifies a missing supported-platform resource reports unavailable without throwing.
     */
    @Test
    void missingEmbeddedResourceIsUnavailable() {
        Optional<Path> nativeLibraryPath = BarriEwwClientInitializer.resolveNativeLibraryPath(
            new SingleResourceClassLoader("unrelated/resource", new byte[] {0x00}),
            "Windows 11",
            "amd64");

        assertTrue(nativeLibraryPath.isEmpty());
    }

    /**
     * @note ThreadSafety: Fabric initialization is expected to run once; this test invokes it once.
     * Verifies immutable published readiness remains unavailable without crashing test startup.
     */
    @Test
    void clientInitializationPublishesUnavailableReadinessWithoutCrashing() {
        BarriEwwClientInitializer clientInitializer = new BarriEwwClientInitializer();

        clientInitializer.onInitializeClient();

        assertTrue(BarriEwwClientInitializer.nativeLibraryPath().isEmpty());
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
