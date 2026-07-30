package barrieww.mod;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.AtomicMoveNotSupportedException;
import java.nio.file.FileAlreadyExistsException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;
import java.util.HexFormat;

/**
 * @note ThreadSafety: Concurrency-safe. Content-addressed destinations and atomic publication
 * allow concurrent callers and processes to extract the same resource deterministically.
 * Extracts an embedded Native shared library into a content-hash-named temporary directory.
 */
public final class NativeLibraryExtractor {
    private static final String s_hashAlgorithmName = "SHA-256";

    /**
     * @note ThreadSafety: Construction is unavailable because this class contains only stateless
     * operations.
     * Prevents utility-class instantiation.
     */
    private NativeLibraryExtractor() {
    }

    /**
     * @note ThreadSafety: Concurrency-safe. Concurrent callers publish identical bytes to the same
     * content-addressed destination without replacing an already published file.
     * Extracts one class-path resource beneath the Barri-Eww temporary directory. The returned path
     * is normalized and absolute. The extracted file is retained until process exit so Windows can
     * keep a loaded dynamic library open.
     *
     * @param ClassLoader resourceClassLoader Class loader that owns the embedded resource
     * @param String resourceName Class-path resource name whose final filename must be preserved
     * @return Path Normalized absolute path to the extracted resource
     * @throws IOException When the resource is missing, unreadable, corrupted at its destination,
     * or cannot be published
     */
    public static Path extract(
        ClassLoader resourceClassLoader,
        String resourceName) throws IOException {
        if (resourceClassLoader == null) {
            throw new IOException("Resource class loader must not be null");
        }
        if (resourceName == null || resourceName.isBlank()) {
            throw new IOException("Native library resource name must not be blank");
        }

        byte[] resourceBytes;
        try (InputStream resourceStream = resourceClassLoader.getResourceAsStream(resourceName)) {
            if (resourceStream == null) {
                throw new IOException("Native library resource is missing: " + resourceName);
            }
            resourceBytes = resourceStream.readAllBytes();
        }

        String resourceFileName = Path.of(resourceName).getFileName().toString();
        String contentHash = calculateContentHash(resourceBytes);
        Path extractionDirectory = Path.of(
            System.getProperty("java.io.tmpdir"),
            "barrieww-native",
            contentHash).toAbsolutePath().normalize();
        Path extractedLibraryPath = extractionDirectory.resolve(resourceFileName).normalize();
        Files.createDirectories(extractionDirectory);

        if (Files.exists(extractedLibraryPath)) {
            verifyPublishedBytes(extractedLibraryPath, resourceBytes);
            registerDeletionAtProcessExit(extractionDirectory, extractedLibraryPath);
            return extractedLibraryPath;
        }

        Path temporaryLibraryPath = Files.createTempFile(
            extractionDirectory,
            resourceFileName + ".",
            ".temporary");
        try {
            Files.write(temporaryLibraryPath, resourceBytes);
            publishTemporaryFile(temporaryLibraryPath, extractedLibraryPath, resourceBytes);
        } finally {
            Files.deleteIfExists(temporaryLibraryPath);
        }

        registerDeletionAtProcessExit(extractionDirectory, extractedLibraryPath);
        return extractedLibraryPath;
    }

    /**
     * @note ThreadSafety: Thread-safe because each invocation owns its digest instance and input.
     * Calculates the lowercase SHA-256 hexadecimal content hash.
     *
     * @param byte[] resourceBytes Resource bytes to hash
     * @return String Lowercase hexadecimal SHA-256 hash
     * @throws IOException When the required Java SHA-256 implementation is unavailable
     */
    private static String calculateContentHash(byte[] resourceBytes) throws IOException {
        try {
            MessageDigest messageDigest = MessageDigest.getInstance(s_hashAlgorithmName);
            return HexFormat.of().formatHex(messageDigest.digest(resourceBytes));
        } catch (NoSuchAlgorithmException algorithmException) {
            throw new IOException(
                "Required content-hash algorithm is unavailable: " + s_hashAlgorithmName,
                algorithmException);
        }
    }

    /**
     * @note ThreadSafety: Concurrency-safe. A competing publisher is accepted only when it wrote
     * exactly the expected bytes.
     * Publishes a completed sibling file atomically when supported, with a non-atomic move fallback.
     *
     * @param Path temporaryLibraryPath Fully written temporary sibling path
     * @param Path extractedLibraryPath Content-addressed destination path
     * @param byte[] expectedResourceBytes Bytes required at the destination
     * @throws IOException When publication or competing-output verification fails
     */
    private static void publishTemporaryFile(
        Path temporaryLibraryPath,
        Path extractedLibraryPath,
        byte[] expectedResourceBytes) throws IOException {
        try {
            Files.move(
                temporaryLibraryPath,
                extractedLibraryPath,
                StandardCopyOption.ATOMIC_MOVE);
        } catch (AtomicMoveNotSupportedException atomicMoveException) {
            moveWithoutReplacement(
                temporaryLibraryPath,
                extractedLibraryPath,
                expectedResourceBytes);
        } catch (FileAlreadyExistsException competingPublicationException) {
            verifyPublishedBytes(extractedLibraryPath, expectedResourceBytes);
        }
    }

    /**
     * @note ThreadSafety: Concurrency-safe. A competing destination is verified and never replaced.
     * Moves a temporary sibling without replacement when atomic moves are unavailable.
     *
     * @param Path temporaryLibraryPath Fully written temporary sibling path
     * @param Path extractedLibraryPath Content-addressed destination path
     * @param byte[] expectedResourceBytes Bytes required at the destination
     * @throws IOException When publication or competing-output verification fails
     */
    private static void moveWithoutReplacement(
        Path temporaryLibraryPath,
        Path extractedLibraryPath,
        byte[] expectedResourceBytes) throws IOException {
        try {
            Files.move(temporaryLibraryPath, extractedLibraryPath);
        } catch (FileAlreadyExistsException competingPublicationException) {
            verifyPublishedBytes(extractedLibraryPath, expectedResourceBytes);
        }
    }

    /**
     * @note ThreadSafety: Thread-safe for stable content-addressed files after publication.
     * Verifies that an existing content-addressed file has exactly the expected bytes.
     *
     * @param Path extractedLibraryPath Existing content-addressed destination path
     * @param byte[] expectedResourceBytes Bytes required at the destination
     * @throws IOException When the destination bytes differ or cannot be read
     */
    private static void verifyPublishedBytes(
        Path extractedLibraryPath,
        byte[] expectedResourceBytes) throws IOException {
        byte[] publishedResourceBytes = Files.readAllBytes(extractedLibraryPath);
        if (!Arrays.equals(publishedResourceBytes, expectedResourceBytes)) {
            throw new IOException(
                "Content-addressed Native library has unexpected bytes: " +
                    extractedLibraryPath);
        }
    }

    /**
     * @note ThreadSafety: Thread-safe according to the JVM delete-on-exit registry contract.
     * Registers file-before-directory cleanup ordering for process exit.
     *
     * @param Path extractionDirectory Content-hash-named extraction directory
     * @param Path extractedLibraryPath Extracted Native library path
     */
    private static void registerDeletionAtProcessExit(
        Path extractionDirectory,
        Path extractedLibraryPath) {
        extractionDirectory.toFile().deleteOnExit();
        extractedLibraryPath.toFile().deleteOnExit();
    }
}
