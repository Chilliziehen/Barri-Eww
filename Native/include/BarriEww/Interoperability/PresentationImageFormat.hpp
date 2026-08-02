#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumeration; trivially thread-safe.
 * @brief Backend-neutral image formats accepted by host-image presentation.
 */
enum class PresentationImageFormat : std::uint32_t {
    R8G8B8A8Unorm = 1,
    B8G8R8A8Unorm = 2,
};

} // namespace barrieww
