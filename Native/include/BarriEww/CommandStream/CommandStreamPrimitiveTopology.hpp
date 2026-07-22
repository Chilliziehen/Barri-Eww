#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumerations and the constexpr predicate below are stateless and
 *       fully thread-safe.
 * @brief Backend-neutral primitive topologies of the GraphicsPipelineTable schema
 *        (v0.1). The loader maps them onto the concrete API (Vulkan:
 *        VkPrimitiveTopology). Zero is deliberately unassigned so cleared memory never
 *        decodes as a usable topology.
 */
enum class CommandStreamPrimitiveTopology : std::uint32_t {
    PointList = 1,
    LineList = 2,
    LineStrip = 3,
    TriangleList = 4,
    TriangleStrip = 5,
};

/**
 * @note ThreadSafety: Thread-safe (pure constant expression).
 * @brief Whether the raw value names an assigned primitive topology.
 * @param rawTopologyValue The raw topology value read from a pipeline record.
 * @return bool True when the value is assigned in the v0.1 schema.
 */
[[nodiscard]] constexpr bool
isAssignedCommandStreamPrimitiveTopology(std::uint32_t rawTopologyValue) noexcept {
    return rawTopologyValue >= 1u && rawTopologyValue <= 5u;
}

} // namespace barrieww
