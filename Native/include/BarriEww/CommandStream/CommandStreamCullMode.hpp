#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumerations and the constexpr predicate below are stateless and
 *       fully thread-safe.
 * @brief Backend-neutral cull modes of the GraphicsPipelineTable schema (v0.1). The
 *        loader maps them onto the concrete API (Vulkan: VkCullModeFlags). Zero is
 *        deliberately unassigned so cleared memory never decodes as a usable mode.
 */
enum class CommandStreamCullMode : std::uint32_t {
    None = 1,
    Front = 2,
    Back = 3,
    FrontAndBack = 4,
};

/**
 * @note ThreadSafety: Thread-safe (pure constant expression).
 * @brief Whether the raw value names an assigned cull mode.
 * @param rawCullModeValue The raw cull mode value read from a pipeline record.
 * @return bool True when the value is assigned in the v0.1 schema.
 */
[[nodiscard]] constexpr bool
isAssignedCommandStreamCullMode(std::uint32_t rawCullModeValue) noexcept {
    return rawCullModeValue >= 1u && rawCullModeValue <= 4u;
}

} // namespace barrieww
