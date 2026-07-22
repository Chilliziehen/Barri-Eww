package barrieww.core.interoperability;

import java.util.Optional;
import java.util.OptionalLong;

/**
 * @note ThreadSafety: Immutable checked exception; safe to inspect from any thread.
 * Reports a semantic BECS module-validation failure returned through the Version 1 ABI.
 */
public final class CommandStreamModuleValidationException extends Exception {
    private final CommandStreamModuleValidationError m_moduleValidationError;
    private final long m_moduleByteOffset;
    private final CommandStreamValidationError m_laneStreamValidationError;
    private final Long m_laneStreamByteOffset;

    /**
     * Creates a checked validation failure with module and optional nested lane context.
     *
     * @param CommandStreamModuleValidationError moduleValidationError Module error category
     * @param long moduleByteOffset Module-relative failure byte offset
     * @param CommandStreamValidationError laneStreamValidationError Optional nested lane error
     * @param Long laneStreamByteOffset Optional lane-relative failure byte offset
     */
    public CommandStreamModuleValidationException(
            CommandStreamModuleValidationError moduleValidationError,
            long moduleByteOffset,
            CommandStreamValidationError laneStreamValidationError,
            Long laneStreamByteOffset) {
        super(buildMessage(moduleValidationError, moduleByteOffset,
                laneStreamValidationError, laneStreamByteOffset));
        m_moduleValidationError = moduleValidationError;
        m_moduleByteOffset = moduleByteOffset;
        m_laneStreamValidationError = laneStreamValidationError;
        m_laneStreamByteOffset = laneStreamByteOffset;
    }

    /** Returns the semantic module-validation error category. */
    public CommandStreamModuleValidationError moduleValidationError() {
        return m_moduleValidationError;
    }

    /** Returns the module-relative failure byte offset. */
    public long moduleByteOffset() {
        return m_moduleByteOffset;
    }

    /** Returns the optional semantic nested lane-validation error. */
    public Optional<CommandStreamValidationError> laneStreamValidationError() {
        return Optional.ofNullable(m_laneStreamValidationError);
    }

    /** Returns the optional lane-relative failure byte offset. */
    public OptionalLong laneStreamByteOffset() {
        return m_laneStreamByteOffset == null
                ? OptionalLong.empty()
                : OptionalLong.of(m_laneStreamByteOffset);
    }

    /** Builds a deterministic message containing both applicable offset domains. */
    private static String buildMessage(
            CommandStreamModuleValidationError moduleValidationError,
            long moduleByteOffset,
            CommandStreamValidationError laneStreamValidationError,
            Long laneStreamByteOffset) {
        String message = "Native command-stream module validation failed: "
                + moduleValidationError + " at module byte offset " + moduleByteOffset;
        if (laneStreamValidationError != null) {
            message += ", nested lane error " + laneStreamValidationError
                    + " at lane byte offset " + laneStreamByteOffset;
        }
        return message;
    }
}
