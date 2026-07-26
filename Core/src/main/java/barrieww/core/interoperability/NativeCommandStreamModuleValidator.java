package barrieww.core.interoperability;

import static java.lang.foreign.MemoryLayout.PathElement.groupElement;

import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.Linker;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.StructLayout;
import java.lang.foreign.SymbolLookup;
import java.lang.foreign.ValueLayout;
import java.lang.invoke.MethodHandle;
import java.nio.file.Path;
import java.util.Objects;

/**
 * @note ThreadSafety: Thread-confined. The opening thread owns the confined lookup Arena
 *       and must perform validation and close operations; concurrent use is unsupported.
 * Opens the non-critical Version 1 Native module-validation binding once and reuses one
 * immutable downcall handle for synchronous validations.
 * @warning MemoryOwnership: This instance owns and closes the confined library-lookup
 * Arena. Callers own every module MemorySegment and keep it live and unmodified for each
 * validate call. Call-local status segments are owned and closed by validate; Native
 * retains no pointer or validated view after return.
 */
public final class NativeCommandStreamModuleValidator implements AutoCloseable {
    public static final String s_nativeSymbolName =
            "barriEwwValidateCommandStreamModuleVersion1";

    private static final int s_operationSuccess = 0;
    private static final int s_operationInvalidArgument = 1;
    private static final int s_operationValidationFailure = 2;
    private static final int s_operationInternalFailure = 3;
    private static final long s_requiredModuleAlignment = 8;

    private static final StructLayout s_validationStatusLayout = MemoryLayout.structLayout(
            ValueLayout.JAVA_INT.withName("moduleValidationErrorCode"),
            ValueLayout.JAVA_INT.withName("laneStreamValidationErrorCode"),
            ValueLayout.JAVA_LONG.withName("moduleByteOffset"),
            ValueLayout.JAVA_LONG.withName("laneStreamByteOffset"));

    private static final long s_moduleValidationErrorCodeOffset = s_validationStatusLayout
            .byteOffset(groupElement("moduleValidationErrorCode"));
    private static final long s_laneStreamValidationErrorCodeOffset = s_validationStatusLayout
            .byteOffset(groupElement("laneStreamValidationErrorCode"));
    private static final long s_moduleByteOffset = s_validationStatusLayout
            .byteOffset(groupElement("moduleByteOffset"));
    private static final long s_laneStreamByteOffset = s_validationStatusLayout
            .byteOffset(groupElement("laneStreamByteOffset"));

    private static final FunctionDescriptor s_validationFunctionDescriptor =
            FunctionDescriptor.of(ValueLayout.JAVA_INT,
                    ValueLayout.ADDRESS, ValueLayout.JAVA_LONG, ValueLayout.ADDRESS);

    private final Arena m_libraryArena;
    private final MethodHandle m_validationHandle;

    private NativeCommandStreamModuleValidator(Arena libraryArena,
                                                MethodHandle validationHandle) {
        m_libraryArena = libraryArena;
        m_validationHandle = validationHandle;
    }

    /**
     * @note ThreadSafety: The returned validator is confined to the calling thread.
     * Opens an explicit Native FFM shared library and resolves the Version 1 validation
     * symbol exactly once. The resulting handle is non-critical and immutable.
     *
     * @param Path nativeLibraryPath Absolute path to BarriEwwNativeFfm
     * @return NativeCommandStreamModuleValidator Open thread-confined binding
     * @throws NativeLibraryLoadingException When the path is invalid, the library cannot
     *         be opened, or the Version 1 symbol cannot be resolved
     * @warning MemoryOwnership: The returned validator owns its library-lookup Arena;
     *          callers must close the validator and must not retain its handle indirectly.
     */
    public static NativeCommandStreamModuleValidator open(Path nativeLibraryPath)
            throws NativeLibraryLoadingException {
        Objects.requireNonNull(nativeLibraryPath, "nativeLibraryPath");
        Path absoluteLibraryPath = nativeLibraryPath.toAbsolutePath().normalize();
        if (!nativeLibraryPath.isAbsolute()) {
            throw new NativeLibraryLoadingException(
                    "Native library path must be absolute: " + nativeLibraryPath,
                    absoluteLibraryPath, s_nativeSymbolName, null);
        }

        Arena libraryArena = Arena.ofConfined();
        try {
            SymbolLookup symbolLookup = SymbolLookup.libraryLookup(
                    absoluteLibraryPath, libraryArena);
            MemorySegment nativeFunctionAddress = symbolLookup.find(s_nativeSymbolName)
                    .orElseThrow(() -> new IllegalArgumentException(
                            "Native symbol is absent: " + s_nativeSymbolName));
            MethodHandle validationHandle = Linker.nativeLinker().downcallHandle(
                    nativeFunctionAddress, s_validationFunctionDescriptor);
            return new NativeCommandStreamModuleValidator(libraryArena, validationHandle);
        } catch (Throwable loadingFailure) {
            libraryArena.close();
            throw new NativeLibraryLoadingException(
                    "Failed to open Native FFM module validator at " + absoluteLibraryPath,
                    absoluteLibraryPath, s_nativeSymbolName, loadingFailure);
        }
    }

    /**
     * @note ThreadSafety: Thread-confined and non-critical. The opening thread must call
     *       this method serially; Native validation itself has no shared mutable state.
     * Synchronously validates one complete native BECM segment and translates stable
     * Native status values into checked Java exceptions.
     *
     * @param MemorySegment moduleSegment Live native segment containing the complete BECM
     *        candidate at an 8-byte-aligned base
     * @throws CommandStreamModuleValidationException When Native rejects module or lane bytes
     * @throws NativeCommandStreamModuleInvocationException When the boundary reports an
     *         invalid operation, an internal failure, an unknown result, or invocation fails
     * @warning MemoryOwnership: The caller owns moduleSegment and keeps its Arena live and
     *          its bytes unmodified for this synchronous call. This method owns its 24-byte
     *          status segment. Native retains neither segment after return.
     */
    public void validate(MemorySegment moduleSegment)
            throws CommandStreamModuleValidationException,
            NativeCommandStreamModuleInvocationException {
        Objects.requireNonNull(moduleSegment, "moduleSegment");
        requireOpen();
        if (!moduleSegment.scope().isAlive()) {
            throw new IllegalStateException("moduleSegment is not alive");
        }
        if (!moduleSegment.isAccessibleBy(Thread.currentThread())) {
            throw new IllegalArgumentException(
                    "moduleSegment is not accessible by the current thread");
        }
        if (!moduleSegment.isNative()) {
            throw new IllegalArgumentException("moduleSegment must be native memory");
        }
        if (moduleSegment.address() % s_requiredModuleAlignment != 0) {
            throw new IllegalArgumentException("moduleSegment base must be 8-byte aligned");
        }

        try (Arena statusArena = Arena.ofConfined()) {
            MemorySegment validationStatus = statusArena.allocate(s_validationStatusLayout);
            int operationResult;
            try {
                operationResult = (int) m_validationHandle.invokeExact(
                        moduleSegment, moduleSegment.byteSize(), validationStatus);
            } catch (Throwable invocationFailure) {
                throw new NativeCommandStreamModuleInvocationException(
                        "Native module-validation invocation failed", s_nativeSymbolName,
                        -1, invocationFailure);
            }
            translateOperationResult(operationResult, validationStatus);
        }
    }

    /**
     * @note ThreadSafety: Thread-confined. The opening thread closes this instance once;
     *       repeated close calls are harmless.
     * Closes the library-lookup Arena and invalidates this validator.
     * @warning MemoryOwnership: This method releases the Arena owned by this instance;
     *          callers retain ownership of all module MemorySegments.
     */
    @Override
    public void close() {
        if (m_libraryArena.scope().isAlive()) {
            m_libraryArena.close();
        }
    }

    /** Ensures the instance-owned lookup Arena remains live. */
    private void requireOpen() {
        if (!m_libraryArena.scope().isAlive()) {
            throw new IllegalStateException("Native module validator is closed");
        }
    }

    /** Translates the stable operation result and its fixed status envelope. */
    private static void translateOperationResult(int operationResult,
                                                 MemorySegment validationStatus)
            throws CommandStreamModuleValidationException,
            NativeCommandStreamModuleInvocationException {
        switch (operationResult) {
            case s_operationSuccess -> {
                requireZeroStatus(validationStatus);
                return;
            }
            case s_operationValidationFailure -> throwValidationException(validationStatus);
            case s_operationInvalidArgument -> throw new NativeCommandStreamModuleInvocationException(
                    "Native module-validation boundary rejected Java arguments",
                    s_nativeSymbolName, operationResult, null);
            case s_operationInternalFailure -> throw new NativeCommandStreamModuleInvocationException(
                    "Native module-validation boundary contained an internal failure",
                    s_nativeSymbolName, operationResult, null);
            default -> throw new NativeCommandStreamModuleInvocationException(
                    "Native module-validation boundary returned unknown operation result "
                            + operationResult,
                    s_nativeSymbolName, operationResult, null);
        }
    }

    /** Ensures a successful Native operation returned a fully cleared status envelope. */
    private static void requireZeroStatus(MemorySegment validationStatus)
            throws NativeCommandStreamModuleInvocationException {
        if (validationStatus.get(ValueLayout.JAVA_INT, s_moduleValidationErrorCodeOffset) != 0
                || validationStatus.get(ValueLayout.JAVA_INT,
                        s_laneStreamValidationErrorCodeOffset) != 0
                || validationStatus.get(ValueLayout.JAVA_LONG, s_moduleByteOffset) != 0
                || validationStatus.get(ValueLayout.JAVA_LONG, s_laneStreamByteOffset) != 0) {
            throw new NativeCommandStreamModuleInvocationException(
                    "Native success returned a non-zero validation status",
                    s_nativeSymbolName, s_operationSuccess, null);
        }
    }

    /** Decodes one Native validation failure into its checked Java exception. */
    private static void throwValidationException(MemorySegment validationStatus)
            throws CommandStreamModuleValidationException,
            NativeCommandStreamModuleInvocationException {
        int moduleErrorCode = validationStatus.get(
                ValueLayout.JAVA_INT, s_moduleValidationErrorCodeOffset);
        int laneStreamErrorCode = validationStatus.get(
                ValueLayout.JAVA_INT, s_laneStreamValidationErrorCodeOffset);
        long moduleByteOffset = validationStatus.get(
                ValueLayout.JAVA_LONG, s_moduleByteOffset);
        long laneStreamByteOffset = validationStatus.get(
                ValueLayout.JAVA_LONG, s_laneStreamByteOffset);

        try {
            CommandStreamModuleValidationError moduleValidationError =
                    CommandStreamModuleValidationError.fromCode(moduleErrorCode);
            CommandStreamValidationError laneStreamValidationError =
                    laneStreamErrorCode == 0
                            ? null
                            : CommandStreamValidationError.fromCode(laneStreamErrorCode);
            Long nestedByteOffset = laneStreamValidationError == null
                    ? null
                    : laneStreamByteOffset;
            throw new CommandStreamModuleValidationException(
                    moduleValidationError, moduleByteOffset,
                    laneStreamValidationError, nestedByteOffset);
        } catch (IllegalArgumentException decodingFailure) {
            throw new NativeCommandStreamModuleInvocationException(
                    "Native validation status contains an unknown stable error code",
                    s_nativeSymbolName, s_operationValidationFailure, decodingFailure);
        }
    }
}
