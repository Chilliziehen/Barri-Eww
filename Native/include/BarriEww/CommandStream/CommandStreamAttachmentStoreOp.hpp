#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumerations and the constexpr predicate below are stateless and
 *       fully thread-safe.
 * @brief Backend-neutral attachment store operations of the RenderingTemplateTable
 *        schema (v0.1). The recorder maps them onto the concrete API (Vulkan:
 *        VkAttachmentStoreOp). Store keeps the rendered contents; DontCare may discard
 *        them.
 */
enum class CommandStreamAttachmentStoreOp : std::uint32_t {
    Store = 0,
    DontCare = 1,
};

/**
 * @note ThreadSafety: Thread-safe (pure constant expression).
 * @brief Whether the raw value names an assigned store operation.
 * @param rawStoreOpValue The raw store operation value read from an attachment record.
 * @return bool True when the value is assigned in the v0.1 schema.
 */
[[nodiscard]] constexpr bool
isAssignedCommandStreamAttachmentStoreOp(std::uint32_t rawStoreOpValue) noexcept {
    return rawStoreOpValue <= 1u;
}

} // namespace barrieww
