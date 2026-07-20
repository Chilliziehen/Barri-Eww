#pragma once

#include <cstddef>
#include <expected>
#include <span>

#include "BarriEww/CommandStream/CommandStreamHandleTableValidationFailure.hpp"
#include "BarriEww/CommandStream/CommandStreamPipelineHandleTableView.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Stateless; validate() is a pure function of its input bytes and
 *       may be called concurrently on distinct or identical byte ranges.
 * @brief Load-time schema validator for PipelineHandleTable sections (schema v0.1):
 *        an 8-byte header, then exact fixed 16-byte entries with an assigned pipeline
 *        kind and a push constant size that is a multiple of 4 and at most 128 (the
 *        Vulkan-guaranteed minimum maxPushConstantsSize, kept as the portable bound).
 */
class CommandStreamPipelineHandleTableValidator {
public:
    /**
     * @note ThreadSafety: Thread-safe (pure function, see class note).
     * @brief Fully validates one PipelineHandleTable section.
     * @param tableBytes The complete section byte range, table header included. Must be
     *        8-byte aligned at its base.
     * @return std::expected<CommandStreamPipelineHandleTableView,
     *         CommandStreamHandleTableValidationFailure> A view on success; the first
     *         failure (category + byte offset) otherwise.
     * @warning MemoryOwnership: tableBytes is BORROWED (§6.3); the returned view
     *          aliases it and the provider must keep it alive and unmodified while the
     *          view is in use.
     */
    [[nodiscard]] static std::expected<CommandStreamPipelineHandleTableView,
                                       CommandStreamHandleTableValidationFailure>
    validate(std::span<const std::byte> tableBytes) noexcept;
};

} // namespace barrieww
