package barrieww.mod;

import static org.junit.jupiter.api.Assertions.assertArrayEquals;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.Method;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Map;
import org.junit.jupiter.api.Test;

/**
 * @note ThreadSafety: Each test uses content-addressed output and may run concurrently.
 * Verifies deterministic extraction of embedded Native shared-library resources.
 */
final class NativeLibraryExtractorTests {
    private static final String s_testResourceName = "native/BarriEwwNativeFfm.dll";
    private static final byte[] s_testResourceBytes = new byte[] {0x42, 0x45, 0x57, 0x57};
    private static final ClassLoader s_testResourceClassLoader = new TestResourceClassLoader(
        Map.of(
            s_testResourceName, s_testResourceBytes,
            "native/DistinctBarriEwwNativeFfm.dll", new byte[] {0x44}));

    /**
     * @note ThreadSafety: Safe to run concurrently because extraction is content-addressed.
     * Verifies repeated extraction returns one normalized absolute path with exact bytes.
     *
     * @throws IOException When test resource access or temporary-file access fails
     */
    @Test
    void repeatedExtractionReturnsSameNormalizedAbsolutePathWithExactBytes() throws IOException {
        Path firstExtractedPath = NativeLibraryExtractor.extract(
            s_testResourceClassLoader, s_testResourceName);
        Path secondExtractedPath = NativeLibraryExtractor.extract(
            s_testResourceClassLoader, s_testResourceName);

        assertEquals(firstExtractedPath, secondExtractedPath);
        assertTrue(firstExtractedPath.isAbsolute());
        assertEquals(firstExtractedPath.normalize(), firstExtractedPath);
        assertArrayEquals(
            s_testResourceBytes,
            Files.readAllBytes(firstExtractedPath));
    }

    /**
     * @note ThreadSafety: Safe to run concurrently because no shared state is modified.
     * Verifies a missing class-path resource produces a checked failure with context.
     */
    @Test
    void missingResourceProducesCheckedFailure() {
        IOException extractionException = assertThrows(
            IOException.class,
            () -> NativeLibraryExtractor.extract(
                s_testResourceClassLoader, "native/MissingBarriEwwNativeFfm.dll"));

        assertTrue(extractionException.getMessage().contains("MissingBarriEwwNativeFfm.dll"));
    }

    /**
     * @note ThreadSafety: Safe to run concurrently because each resource has distinct contents.
     * Verifies different resource contents select different content-hash directories.
     *
     * @throws IOException When test resource access or temporary-file access fails
     */
    @Test
    void distinctResourceContentsUseDistinctHashDirectories() throws IOException {
        Path firstExtractedPath = NativeLibraryExtractor.extract(
            s_testResourceClassLoader, s_testResourceName);
        Path secondExtractedPath = NativeLibraryExtractor.extract(
            s_testResourceClassLoader, "native/DistinctBarriEwwNativeFfm.dll");

        assertNotEquals(firstExtractedPath.getParent(), secondExtractedPath.getParent());
    }

    /**
     * @note ThreadSafety: Safe to run concurrently because no shared extraction file is created.
     * Verifies invalid extractor inputs produce checked failures.
     */
    @Test
    void invalidInputsProduceCheckedFailures() {
        assertThrows(
            IOException.class,
            () -> NativeLibraryExtractor.extract(null, s_testResourceName));
        assertThrows(
            IOException.class,
            () -> NativeLibraryExtractor.extract(s_testResourceClassLoader, null));
        assertThrows(
            IOException.class,
            () -> NativeLibraryExtractor.extract(s_testResourceClassLoader, " "));
    }

    /**
     * @note ThreadSafety: Not safe to run concurrently with extraction of the same unique resource.
     * Verifies an existing corrupted content-addressed destination is rejected.
     *
     * @throws IOException When test setup cannot create or alter the extracted resource
     */
    @Test
    void corruptedExistingDestinationProducesCheckedFailure() throws IOException {
        String resourceName = "native/CorruptionBarriEwwNativeFfm.dll";
        ClassLoader resourceClassLoader = new TestResourceClassLoader(
            Map.of(resourceName, new byte[] {0x21, 0x22, 0x23}));
        Path extractedLibraryPath = NativeLibraryExtractor.extract(
            resourceClassLoader,
            resourceName);
        Files.write(extractedLibraryPath, new byte[] {0x00});

        IOException extractionException = assertThrows(
            IOException.class,
            () -> NativeLibraryExtractor.extract(resourceClassLoader, resourceName));

        assertTrue(extractionException.getMessage().contains("unexpected bytes"));
    }

    /**
     * @note ThreadSafety: Safe to run concurrently because each invocation owns a fresh directory.
     * Verifies the non-atomic fallback publishes once and accepts identical competing output.
     *
     * @throws Exception When reflection or temporary-file operations fail
     */
    @Test
    void nonAtomicFallbackPublishesAndAcceptsIdenticalExistingOutput() throws Exception {
        Method moveWithoutReplacementMethod = NativeLibraryExtractor.class.getDeclaredMethod(
            "moveWithoutReplacement",
            Path.class,
            Path.class,
            byte[].class);
        moveWithoutReplacementMethod.setAccessible(true);
        Path temporaryDirectory = Files.createTempDirectory("barrieww-extractor-fallback-");
        Path extractedLibraryPath = temporaryDirectory.resolve("BarriEwwNativeFfm.dll");
        byte[] expectedResourceBytes = new byte[] {0x31, 0x32};
        Path firstTemporaryLibraryPath = Files.write(
            temporaryDirectory.resolve("first.temporary"),
            expectedResourceBytes);

        moveWithoutReplacementMethod.invoke(
            null,
            firstTemporaryLibraryPath,
            extractedLibraryPath,
            expectedResourceBytes);

        Path competingTemporaryLibraryPath = Files.write(
            temporaryDirectory.resolve("competing.temporary"),
            expectedResourceBytes);
        moveWithoutReplacementMethod.invoke(
            null,
            competingTemporaryLibraryPath,
            extractedLibraryPath,
            expectedResourceBytes);
        assertArrayEquals(expectedResourceBytes, Files.readAllBytes(extractedLibraryPath));
    }

    /**
     * @note ThreadSafety: Immutable after construction and safe for concurrent resource reads.
     * Supplies exact in-memory resource bytes without requiring a Native binary on the test path.
     */
    private static final class TestResourceClassLoader extends ClassLoader {
        private final Map<String, byte[]> m_resourceContents;

        /**
         * @note ThreadSafety: Construction requires exclusive access; the resulting instance is
         * immutable and concurrency-safe.
         * Creates a class loader backed by immutable resource content mappings.
         *
         * @param Map<String, byte[]> resourceContents Resource names and their exact bytes
         */
        private TestResourceClassLoader(Map<String, byte[]> resourceContents) {
            super(null);
            m_resourceContents = Map.copyOf(resourceContents);
        }

        /**
         * @note ThreadSafety: Concurrency-safe because resource mappings are immutable and each call
         * creates an independent stream.
         * Opens an in-memory resource, or returns null when the resource name is absent.
         *
         * @param String resourceName Requested class-path resource name
         * @return InputStream Independent stream over the exact resource bytes, or null when absent
         */
        @Override
        public InputStream getResourceAsStream(String resourceName) {
            byte[] resourceBytes = m_resourceContents.get(resourceName);
            return resourceBytes == null ? null : new ByteArrayInputStream(resourceBytes);
        }
    }
}
