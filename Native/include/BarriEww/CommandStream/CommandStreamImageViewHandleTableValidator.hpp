#pragma once

#include <cstddef>
#include <expected>
#include <span>

#include "BarriEww/CommandStream/CommandStreamHandleTableValidationFailure.hpp"
#include "BarriEww/CommandStream/CommandStreamImageViewHandleTableView.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Stateless; validate() is a pure function of its input bytes and
 *       may be called concurrently on distinct or identical byte ranges.
 * @brief Load-time schema validator for ImageViewHandleTable sections (schema v0.1):
 *        an 8-byte header (entryCount, zero reserved flags) followed by fixed 32-byte
 *        entries. It validates only fields independent of the source ImageHandleTable;
 *        source slot, format/kind compatibility, bounds and Vulkan checks belong to
 *        native materialization where both tables are available.
 */
class CommandStreamImageViewHandleTableValidator {
public:
    /**
     * @note ThreadSafety: Thread-safe (pure function, see class note).
     * @brief Fully validates one ImageViewHandleTable section: header, exact size,
     *        assigned kind and format, usable aspect mask, positive non-sentinel mip count,
     *        and v0.1 non-array rules (one layer; 3D views begin at layer zero).
     * @param tableBytes The complete section byte range, table header included. Must be
     *        8-byte aligned at its base.
     * @return std::expected<CommandStreamImageViewHandleTableView,
     *         CommandStreamHandleTableValidationFailure> A view on success; the first
     *         failure (category + byte offset) otherwise.
     * @warning MemoryOwnership: tableBytes is BORROWED (§6.3); the returned view aliases
     *          it and the provider must keep it alive and unmodified while the view is
     *          in use.
     */
    [[nodiscard]] static std::expected<CommandStreamImageViewHandleTableView,
                                       CommandStreamHandleTableValidationFailure>
    validate(std::span<const std::byte> tableBytes) noexcept;
};

} // namespace barrieww
