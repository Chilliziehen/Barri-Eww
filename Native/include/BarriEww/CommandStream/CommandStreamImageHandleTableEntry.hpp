#pragma once

#include <cstddef>
#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief One 40-byte ImageHandleTable entry (schema v0.1). Two mutually exclusive
 *        shapes share the layout (same convention as the buffer table):
 *        - CREATED (importIdentifier == 0): the loader creates the image from the
 *          descriptive fields at load time (device-local, optimal tiling, initial
 *          layout Undefined — first use must transition via an image barrier).
 *        - IMPORTED (importIdentifier != 0): the host binds an existing image (for
 *          example a Minecraft swapchain image) under that identifier at load time;
 *          the provider keeps ownership (§6.3).
 *        Layout is the wire format, pinned by the static_asserts below (ADR-0002 D1).
 */
struct CommandStreamImageHandleTableEntry {
    std::uint32_t imageKindValue;
    std::uint32_t formatValue;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t depth;
    std::uint32_t mipLevelCount;
    std::uint32_t arrayLayerCount;
    std::uint32_t sampleCountValue;
    std::uint32_t usageFlags;
    std::uint32_t importIdentifier;

    /** Whether this entry is imported rather than loader-created (see struct notes). */
    [[nodiscard]] bool isImported() const noexcept { return importIdentifier != 0u; }
};

static_assert(sizeof(CommandStreamImageHandleTableEntry) == 40u,
              "ImageHandleTable entries must be exactly 40 bytes (schema v0.1)");
static_assert(offsetof(CommandStreamImageHandleTableEntry, formatValue) == 4u);
static_assert(offsetof(CommandStreamImageHandleTableEntry, depth) == 16u);
static_assert(offsetof(CommandStreamImageHandleTableEntry, sampleCountValue) == 28u);
static_assert(offsetof(CommandStreamImageHandleTableEntry, importIdentifier) == 36u);

} // namespace barrieww
