package barrieww.mod;

import java.io.IOException;
import java.nio.file.Path;
import java.util.Locale;
import java.util.Optional;
import net.fabricmc.api.ClientModInitializer;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * @note ThreadSafety: Fabric invokes initialization once on the client thread. Published Native
 * library readiness is immutable and safe for concurrent reads after class initialization.
 * Extracts the packaged platform Native library during client initialization without loading it or
 * resolving any Core or Native symbols.
 */
public final class BarriEwwClientInitializer implements ClientModInitializer {
    private static final Logger s_logger = LoggerFactory.getLogger("Barri-Eww");

    /**
     * @note ThreadSafety: Called once by Fabric on the client initialization thread. The method
     * reads only immutable one-time readiness state.
     * Resolves and extracts the packaged Native library, then reports readiness without taking over
     * presentation or crashing startup when the platform or resource is unavailable.
     */
    @Override
    public void onInitializeClient() {
        logNativeLibraryReadiness(
            s_logger,
            NativeLibraryReadinessHolder.s_nativeLibraryReadiness);
    }

    /**
     * @note ThreadSafety: Safe for concurrent immutable readiness reads; logger ordering and output
     * synchronization are delegated to the established SLF4J implementation.
     * Logs exactly one readiness outcome, preserving checked extraction failure context when present.
     *
     * @param Logger logger Logger receiving exactly one readiness outcome
     * @param NativeLibraryReadiness nativeLibraryReadiness Immutable one-time readiness result
     */
    static void logNativeLibraryReadiness(
        Logger logger,
        NativeLibraryReadiness nativeLibraryReadiness) {
        Optional<Path> nativeLibraryPath = nativeLibraryReadiness.nativeLibraryPath();
        if (nativeLibraryPath.isPresent()) {
            logger.info("Barri-Eww Native library is ready at {}", nativeLibraryPath.orElseThrow());
            return;
        }

        Optional<IOException> extractionException = nativeLibraryReadiness.extractionException();
        if (extractionException.isPresent()) {
            String warningMessage = "Barri-Eww Native library extraction failed for operating " +
                "system '" + nativeLibraryReadiness.operatingSystemName() +
                "', architecture '" + nativeLibraryReadiness.architectureName() +
                "', resource '" + nativeLibraryReadiness.nativeLibraryResource().orElseThrow() +
                "'; presentation remains unchanged";
            logger.warn(warningMessage, extractionException.orElseThrow());
            return;
        }

        logger.warn(
            "Barri-Eww Native library is unavailable for operating system '{}' and architecture '{}'; " +
                "presentation remains unchanged",
            nativeLibraryReadiness.operatingSystemName(),
            nativeLibraryReadiness.architectureName());
    }

    /**
     * @note ThreadSafety: Concurrency-safe. Java class initialization publishes the immutable path
     * holder before this accessor returns.
     * Returns the extracted Native-library path when client readiness is available.
     *
     * @return Optional<Path> Immutable readiness path, or empty when unsupported or extraction failed
     */
    public static Optional<Path> nativeLibraryPath() {
        return NativeLibraryReadinessHolder.s_nativeLibraryReadiness.nativeLibraryPath();
    }

    /**
     * @note ThreadSafety: Concurrency-safe because platform mapping is stateless.
     * Maps runtime operating-system and architecture names to the exact packaged resource.
     *
     * @param String operatingSystemName Runtime operating-system name
     * @param String architectureName Runtime processor architecture name
     * @return Optional<String> Exact Native resource path, or empty for an unsupported target
     */
    static Optional<String> resolveNativeLibraryResource(
        String operatingSystemName,
        String architectureName) {
        if (operatingSystemName == null || architectureName == null) {
            return Optional.empty();
        }

        String normalizedOperatingSystemName = operatingSystemName.toLowerCase(Locale.ROOT);
        String normalizedArchitectureName = architectureName.toLowerCase(Locale.ROOT);
        boolean isSupportedArchitecture = normalizedArchitectureName.equals("amd64")
            || normalizedArchitectureName.equals("x86_64");
        if (!isSupportedArchitecture) {
            return Optional.empty();
        }

        if (normalizedOperatingSystemName.startsWith("windows")) {
            return Optional.of("barrieww/native/windows-x86_64/BarriEwwNativeFfm.dll");
        }
        if (normalizedOperatingSystemName.startsWith("linux")) {
            return Optional.of("barrieww/native/linux-x86_64/libBarriEwwNativeFfm.so");
        }
        return Optional.empty();
    }

    /**
     * @note ThreadSafety: Concurrency-safe through NativeLibraryExtractor content-addressed
     * publication. The returned value is immutable.
     * Resolves and extracts the Native library, converting unsupported targets and checked
     * extraction failures into unavailable readiness.
     *
     * @param ClassLoader resourceClassLoader Class loader containing the packaged Native resource
     * @param String operatingSystemName Runtime operating-system name
     * @param String architectureName Runtime processor architecture name
     * @return Optional<Path> Extracted normalized absolute path, or empty when unavailable
     */
    static Optional<Path> resolveNativeLibraryPath(
        ClassLoader resourceClassLoader,
        String operatingSystemName,
        String architectureName) {
        return resolveNativeLibraryReadiness(
            resourceClassLoader,
            operatingSystemName,
            architectureName).nativeLibraryPath();
    }

    /**
     * @note ThreadSafety: Concurrency-safe through NativeLibraryExtractor content-addressed
     * publication. Each invocation returns a new immutable result.
     * Resolves one complete Native-library readiness result while preserving attempted-resource and
     * checked-exception context for supported-platform extraction failures.
     *
     * @param ClassLoader resourceClassLoader Class loader containing the packaged Native resource
     * @param String operatingSystemName Runtime operating-system name
     * @param String architectureName Runtime processor architecture name
     * @return NativeLibraryReadiness Immutable readiness state with complete initialization context
     */
    private static NativeLibraryReadiness resolveNativeLibraryReadiness(
        ClassLoader resourceClassLoader,
        String operatingSystemName,
        String architectureName) {
        Optional<String> nativeLibraryResource = resolveNativeLibraryResource(
            operatingSystemName,
            architectureName);
        if (nativeLibraryResource.isEmpty()) {
            return new NativeLibraryReadiness(
                operatingSystemName,
                architectureName,
                Optional.empty(),
                Optional.empty(),
                Optional.empty());
        }

        try {
            Path nativeLibraryPath = NativeLibraryExtractor.extract(
                resourceClassLoader,
                nativeLibraryResource.orElseThrow());
            return new NativeLibraryReadiness(
                operatingSystemName,
                architectureName,
                nativeLibraryResource,
                Optional.of(nativeLibraryPath),
                Optional.empty());
        } catch (IOException extractionException) {
            return new NativeLibraryReadiness(
                operatingSystemName,
                architectureName,
                nativeLibraryResource,
                Optional.empty(),
                Optional.of(extractionException));
        }
    }

    /**
     * @note ThreadSafety: Java class initialization computes and safely publishes this immutable
     * state exactly once.
     * Holds the one-time Native-library readiness result without mutable global state.
     */
    private static final class NativeLibraryReadinessHolder {
        private static final NativeLibraryReadiness s_nativeLibraryReadiness =
            resolveNativeLibraryReadiness(
                BarriEwwClientInitializer.class.getClassLoader(),
                System.getProperty("os.name"),
                System.getProperty("os.arch"));

        /**
         * @note ThreadSafety: Construction occurs only during serialized class initialization.
         * Prevents holder instantiation.
         */
        private NativeLibraryReadinessHolder() {
        }
    }

    /**
     * @note ThreadSafety: Immutable after construction and safe for concurrent reads. All accessors
     * return immutable values or the retained checked exception reference without mutation.
     * Preserves the complete result of one Native-library readiness attempt.
     */
    private static final class NativeLibraryReadiness {
        private final String m_operatingSystemName;
        private final String m_architectureName;
        private final Optional<String> m_nativeLibraryResource;
        private final Optional<Path> m_nativeLibraryPath;
        private final Optional<IOException> m_extractionException;

        /**
         * @note ThreadSafety: Construction requires exclusive access; the resulting instance is
         * immutable and concurrency-safe.
         * Creates one complete Native-library readiness result.
         *
         * @param String operatingSystemName Runtime operating-system name used by the attempt
         * @param String architectureName Runtime processor architecture name used by the attempt
         * @param Optional<String> nativeLibraryResource Attempted resource, or empty when unsupported
         * @param Optional<Path> nativeLibraryPath Extracted path, or empty when unavailable
         * @param Optional<IOException> extractionException Checked extraction failure, or empty when
         * extraction was not attempted or succeeded
         */
        private NativeLibraryReadiness(
            String operatingSystemName,
            String architectureName,
            Optional<String> nativeLibraryResource,
            Optional<Path> nativeLibraryPath,
            Optional<IOException> extractionException) {
            m_operatingSystemName = operatingSystemName;
            m_architectureName = architectureName;
            m_nativeLibraryResource = nativeLibraryResource;
            m_nativeLibraryPath = nativeLibraryPath;
            m_extractionException = extractionException;
        }

        /**
         * @note ThreadSafety: Concurrency-safe immutable accessor.
         * Returns the runtime operating-system name captured by this readiness attempt.
         *
         * @return String Captured runtime operating-system name
         */
        private String operatingSystemName() {
            return m_operatingSystemName;
        }

        /**
         * @note ThreadSafety: Concurrency-safe immutable accessor.
         * Returns the runtime processor architecture captured by this readiness attempt.
         *
         * @return String Captured runtime processor architecture name
         */
        private String architectureName() {
            return m_architectureName;
        }

        /**
         * @note ThreadSafety: Concurrency-safe immutable accessor.
         * Returns the exact attempted Native resource when the platform is supported.
         *
         * @return Optional<String> Attempted resource, or empty when the platform is unsupported
         */
        private Optional<String> nativeLibraryResource() {
            return m_nativeLibraryResource;
        }

        /**
         * @note ThreadSafety: Concurrency-safe immutable accessor.
         * Returns the extracted normalized absolute path when readiness succeeded.
         *
         * @return Optional<Path> Extracted path, or empty when readiness is unavailable
         */
        private Optional<Path> nativeLibraryPath() {
            return m_nativeLibraryPath;
        }

        /**
         * @note ThreadSafety: Concurrency-safe immutable accessor. Callers must not mutate the
         * retained exception through Throwable mutation methods.
         * Returns the checked extraction failure for a supported platform.
         *
         * @return Optional<IOException> Extraction failure, or empty when not attempted or successful
         */
        private Optional<IOException> extractionException() {
            return m_extractionException;
        }
    }
}
