#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace barrieww {

/**
 * @note ThreadSafety: Plain output record; separate instances are concurrency-safe.
 * @brief Host-image resource detachment result containing a raw Vulkan wait result.
 * @warning MemoryOwnership: Java owns the record; Native writes it synchronously and does
 *          not retain its address.
 */
struct NativePresentationDetachHostImageResourcesResultVersion1 {
    std::int32_t vulkanResult;
    std::uint32_t reserved;
};

static_assert(sizeof(NativePresentationDetachHostImageResourcesResultVersion1) == 8u);
static_assert(alignof(NativePresentationDetachHostImageResourcesResultVersion1) == 4u);
static_assert(offsetof(NativePresentationDetachHostImageResourcesResultVersion1,
                       vulkanResult) == 0u);
static_assert(offsetof(NativePresentationDetachHostImageResourcesResultVersion1,
                       reserved) == 4u);
static_assert(std::is_standard_layout_v<
              NativePresentationDetachHostImageResourcesResultVersion1>);
static_assert(std::is_trivially_copyable_v<
              NativePresentationDetachHostImageResourcesResultVersion1>);

} // namespace barrieww
