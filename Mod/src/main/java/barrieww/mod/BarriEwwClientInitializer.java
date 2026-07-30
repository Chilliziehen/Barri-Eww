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
        Optional<Path> nativeLibraryPath = NativeLibraryPathHolder.s_nativeLibraryPath;
        if (nativeLibraryPath.isPresent()) {
            s_logger.info("Barri-Eww Native library is ready at {}", nativeLibraryPath.orElseThrow());
            return;
        }

        s_logger.warn(
            "Barri-Eww Native library is unavailable for operating system '{}' and architecture '{}'; " +
                "presentation remains unchanged",
            System.getProperty("os.name"),
            System.getProperty("os.arch"));
    }

    /**
     * @note ThreadSafety: Concurrency-safe. Java class initialization publishes the immutable path
     * holder before this accessor returns.
     * Returns the extracted Native-library path when client readiness is available.
     *
     * @return Optional<Path> Immutable readiness path, or empty when unsupported or extraction failed
     */
    public static Optional<Path> nativeLibraryPath() {
        return NativeLibraryPathHolder.s_nativeLibraryPath;
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
        Optional<String> nativeLibraryResource = resolveNativeLibraryResource(
            operatingSystemName,
            architectureName);
        if (nativeLibraryResource.isEmpty()) {
            return Optional.empty();
        }

        try {
            return Optional.of(NativeLibraryExtractor.extract(
                resourceClassLoader,
                nativeLibraryResource.orElseThrow()));
        } catch (IOException extractionException) {
            return Optional.empty();
        }
    }

    /**
     * @note ThreadSafety: Java class initialization computes and safely publishes this immutable
     * state exactly once.
     * Holds the one-time Native-library readiness path without mutable global state.
     */
    private static final class NativeLibraryPathHolder {
        private static final Optional<Path> s_nativeLibraryPath = resolveNativeLibraryPath(
            BarriEwwClientInitializer.class.getClassLoader(),
            System.getProperty("os.name"),
            System.getProperty("os.arch"));

        /**
         * @note ThreadSafety: Construction occurs only during serialized class initialization.
         * Prevents holder instantiation.
         */
        private NativeLibraryPathHolder() {
        }
    }
}
