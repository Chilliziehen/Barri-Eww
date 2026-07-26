package barrieww.core.interoperability;

import java.nio.file.Path;

/**
 * @note ThreadSafety: Immutable checked exception; safe to inspect from any thread.
 * Reports failure to load the explicit Native FFM library or resolve its Version 1 symbol.
 */
public final class NativeLibraryLoadingException extends Exception {
    private final Path m_nativeLibraryPath;
    private final String m_nativeSymbolName;

    /**
     * Creates a checked library-loading failure with complete lookup context.
     *
     * @param String message Human-readable failure description
     * @param Path nativeLibraryPath Absolute native shared-library path
     * @param String nativeSymbolName Required Version 1 symbol name
     * @param Throwable cause Underlying FFM loading or lookup failure
     */
    public NativeLibraryLoadingException(String message, Path nativeLibraryPath,
                                         String nativeSymbolName, Throwable cause) {
        super(message, cause);
        m_nativeLibraryPath = nativeLibraryPath;
        m_nativeSymbolName = nativeSymbolName;
    }

    /** Returns the absolute native shared-library path. */
    public Path nativeLibraryPath() {
        return m_nativeLibraryPath;
    }

    /** Returns the required Version 1 native symbol name. */
    public String nativeSymbolName() {
        return m_nativeSymbolName;
    }
}
