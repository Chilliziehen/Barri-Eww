package barrieww.core.interoperability;

/**
 * @note ThreadSafety: Immutable checked exception; safe to inspect from any thread.
 * Reports an operation-level failure of the Native presentation runtime FFM boundary
 * (invalid argument, unsupported surface, Vulkan failure or contained internal failure).
 */
public final class NativePresentationRuntimeException extends Exception {
    private final String m_nativeSymbolName;
    private final int m_operationResultCode;
    private final int m_vulkanResult;

    /**
     * Creates an operation-level presentation runtime failure.
     *
     * @param String message Human-readable failure description
     * @param String nativeSymbolName Invoked Version 1 native symbol
     * @param int operationResultCode Native operation result value
     * @param int vulkanResult Raw VkResult where a Vulkan call was involved, otherwise 0
     */
    public NativePresentationRuntimeException(String message, String nativeSymbolName,
                                               int operationResultCode, int vulkanResult) {
        this(message, nativeSymbolName, operationResultCode, vulkanResult, null);
    }

    /**
     * Creates an operation-level presentation runtime failure with its original cause.
     *
     * @param String message Human-readable failure description
     * @param String nativeSymbolName Invoked Version 1 native symbol
     * @param int operationResultCode Native operation result value
     * @param int vulkanResult Raw VkResult where a Vulkan call was involved, otherwise 0
     * @param Throwable cause Original Java-side boundary or initialization failure
     */
    public NativePresentationRuntimeException(String message, String nativeSymbolName,
                                               int operationResultCode, int vulkanResult,
                                               Throwable cause) {
        super(message, cause);
        m_nativeSymbolName = nativeSymbolName;
        m_operationResultCode = operationResultCode;
        m_vulkanResult = vulkanResult;
    }

    /** Returns the invoked Version 1 native symbol name. */
    public String nativeSymbolName() {
        return m_nativeSymbolName;
    }

    /** Returns the native operation result value. */
    public int operationResultCode() {
        return m_operationResultCode;
    }

    /** Returns the raw VkResult, or 0 when no Vulkan call was involved. */
    public int vulkanResult() {
        return m_vulkanResult;
    }
}
