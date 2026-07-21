#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumerations and the constexpr helpers below are stateless and
 *       fully thread-safe.
 * @brief Backend-neutral image formats of the ImageHandleTable schema (v0.1 minimal
 *        set; additive growth per ADR-0002 D6). The loader maps them onto concrete
 *        API formats (Vulkan: VkFormat). Swapchain-provided formats arrive through
 *        IMPORTED entries whose descriptive fields the host fills at bind time.
 */
enum class CommandStreamImageFormat : std::uint32_t {
    R8G8B8A8Unorm = 1,
    B8G8R8A8Unorm = 2,
    R16G16B16A16Float = 3,
    R32Uint = 4,
    D32Float = 5,
    D24UnormS8Uint = 6,
};

/**
 * @note ThreadSafety: Thread-safe (pure constant expression).
 * @brief Whether the raw value names an assigned image format.
 * @param rawImageFormatValue The raw format value read from a table entry.
 * @return bool True when the value is assigned in the v0.1 schema.
 */
[[nodiscard]] constexpr bool
isAssignedCommandStreamImageFormat(std::uint32_t rawImageFormatValue) noexcept {
    return rawImageFormatValue >= 1u && rawImageFormatValue <= 6u;
}

/**
 * @note ThreadSafety: Thread-safe (pure constant expression).
 * @brief The byte size of one texel in the given format (packed color/depth data as
 *        seen by buffer copies). Used for load-time copy range checks.
 * @param imageFormat The assigned image format.
 * @return std::uint32_t The texel byte size.
 */
[[nodiscard]] constexpr std::uint32_t
commandStreamImageFormatTexelByteSize(CommandStreamImageFormat imageFormat) noexcept {
    switch (imageFormat) {
        case CommandStreamImageFormat::R8G8B8A8Unorm: return 4u;
        case CommandStreamImageFormat::B8G8R8A8Unorm: return 4u;
        case CommandStreamImageFormat::R16G16B16A16Float: return 8u;
        case CommandStreamImageFormat::R32Uint: return 4u;
        case CommandStreamImageFormat::D32Float: return 4u;
        case CommandStreamImageFormat::D24UnormS8Uint: return 4u;
    }
    return 0u;
}

} // namespace barrieww
