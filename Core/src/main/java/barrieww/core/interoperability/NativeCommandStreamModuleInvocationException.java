package barrieww.core.interoperability;

/**
 * @note ThreadSafety: Immutable checked exception; safe to inspect from any thread.
 * Reports an operation-level Native FFM failure rather than invalid BECS input.
 */
public final class NativeCommandStreamModuleInvocationException extends Exception {
    private final String m_nativeSymbolName;
    private final int m_operationResultCode;

    /**
     * Creates an operation-level FFM invocation failure.
     *
     * @param String message Human-readable failure description
     * @param String nativeSymbolName Invoked Version 1 native symbol
     * @param int operationResultCode Native operation result, or -1 before a result existed
     * @param Throwable cause Underlying invocation or decoding failure, when available
     */
    public NativeCommandStreamModuleInvocationException(
            String message, String nativeSymbolName, int operationResultCode, Throwable cause) {
        super(message, cause);
        m_nativeSymbolName = nativeSymbolName;
        m_operationResultCode = operationResultCode;
    }

    /** Returns the invoked Version 1 native symbol name. */
    public String nativeSymbolName() {
        return m_nativeSymbolName;
    }

    /** Returns the native operation result, or -1 when invocation produced none. */
    public int operationResultCode() {
        return m_operationResultCode;
    }
}
