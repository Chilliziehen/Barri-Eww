#pragma once

#include <cstddef>
#include <expected>
#include <span>

#include "BarriEww/CommandStream/CommandStreamHandleTableValidationFailure.hpp"
#include "BarriEww/CommandStream/CommandStreamShaderModuleTableView.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Stateless; validate() is a pure function of its input bytes and
 *       may be called concurrently on distinct or identical byte ranges.
 * @brief Load-time schema validator for ShaderModuleTable sections (schema v0.1):
 *        an 8-byte header (entryCount, zero reserved flags), entryCount 16-byte
 *        directory entries, then the blob region. Every blob must be 8-aligned, in
 *        bounds, at least the SPIR-V minimum size, a multiple of 4 bytes, and start
 *        with the SPIR-V magic word 0x07230203. Failures share the handle-table error
 *        space (§6.4 single error space per artifact family).
 */
class CommandStreamShaderModuleTableValidator {
public:
    /**
     * @note ThreadSafety: Thread-safe (pure function, see class note).
     * @brief Fully validates one ShaderModuleTable section.
     * @param tableBytes The complete section byte range, table header included. Must be
     *        8-byte aligned at its base.
     * @return std::expected<CommandStreamShaderModuleTableView,
     *         CommandStreamHandleTableValidationFailure> A view on success; the first
     *         failure (category + byte offset) otherwise.
     * @warning MemoryOwnership: tableBytes is BORROWED (§6.3); the returned view
     *          aliases it and the provider must keep it alive and unmodified while the
     *          view is in use.
     */
    [[nodiscard]] static std::expected<CommandStreamShaderModuleTableView,
                                       CommandStreamHandleTableValidationFailure>
    validate(std::span<const std::byte> tableBytes) noexcept;
};

} // namespace barrieww
