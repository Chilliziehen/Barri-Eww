#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumerations and the constexpr predicate below are stateless and
 *       fully thread-safe.
 * @brief Backend-neutral front-face winding orders of the GraphicsPipelineTable schema
 *        (v0.1). The loader maps them onto the concrete API (Vulkan: VkFrontFace). Zero
 *        is deliberately unassigned so cleared memory never decodes as a usable order.
 */
enum class CommandStreamFrontFace : std::uint32_t {
    CounterClockwise = 1,
    Clockwise = 2,
};

/**
 * @note ThreadSafety: Thread-safe (pure constant expression).
 * @brief Whether the raw value names an assigned front-face winding order.
 * @param rawFrontFaceValue The raw front-face value read from a pipeline record.
 * @return bool True when the value is assigned in the v0.1 schema.
 */
[[nodiscard]] constexpr bool
isAssignedCommandStreamFrontFace(std::uint32_t rawFrontFaceValue) noexcept {
    return rawFrontFaceValue >= 1u && rawFrontFaceValue <= 2u;
}

} // namespace barrieww
