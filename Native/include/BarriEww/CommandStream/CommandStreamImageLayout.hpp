#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumerations and the constexpr predicate below are stateless and
 *       fully thread-safe.
 * @brief Backend-neutral image layouts (v0.1), used by image barrier records and by
 *        image commands that must state the layout the image is in. The recorder maps
 *        them onto concrete API layouts (Vulkan: VkImageLayout); DX12 will map them
 *        onto resource states (T0[3]).
 */
enum class CommandStreamImageLayout : std::uint32_t {
    Undefined = 0,
    General = 1,
    ColorAttachment = 2,
    DepthStencilAttachment = 3,
    ShaderReadOnly = 4,
    TransferSource = 5,
    TransferDestination = 6,
    Present = 7,
};

/**
 * @note ThreadSafety: Thread-safe (pure constant expression).
 * @brief Whether the raw value names an assigned image layout.
 * @param rawImageLayoutValue The raw layout value read from a record or payload.
 * @return bool True when the value is assigned in the v0.1 schema.
 */
[[nodiscard]] constexpr bool
isAssignedCommandStreamImageLayout(std::uint32_t rawImageLayoutValue) noexcept {
    return rawImageLayoutValue <= 7u;
}

} // namespace barrieww
