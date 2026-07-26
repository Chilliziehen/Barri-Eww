#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumerations and the constexpr predicate below are stateless and
 *       fully thread-safe.
 * @brief Backend-neutral attachment load operations of the RenderingTemplateTable
 *        schema (v0.1). The recorder maps them onto the concrete API (Vulkan:
 *        VkAttachmentLoadOp). Clear uses the record's clear value; DontCare discards
 *        prior contents.
 */
enum class CommandStreamAttachmentLoadOp : std::uint32_t {
    Load = 0,
    Clear = 1,
    DontCare = 2,
};

/**
 * @note ThreadSafety: Thread-safe (pure constant expression).
 * @brief Whether the raw value names an assigned load operation.
 * @param rawLoadOpValue The raw load operation value read from an attachment record.
 * @return bool True when the value is assigned in the v0.1 schema.
 */
[[nodiscard]] constexpr bool
isAssignedCommandStreamAttachmentLoadOp(std::uint32_t rawLoadOpValue) noexcept {
    return rawLoadOpValue <= 2u;
}

} // namespace barrieww
