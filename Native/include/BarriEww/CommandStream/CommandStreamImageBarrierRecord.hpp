#pragma once

#include <cstddef>
#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief One 64-byte image barrier record of a barrier batch (schema v0.1):
 *        synchronization2 masks, the image slot, a neutral aspect mask
 *        (CommandStreamImageAspect bits), the neutral layout transition
 *        (CommandStreamImageLayout values) and the subresource range. Counts of
 *        s_remainingCount mirror Vulkan's VK_REMAINING_MIP_LEVELS /
 *        VK_REMAINING_ARRAY_LAYERS. Layout is the wire format, pinned below.
 */
struct CommandStreamImageBarrierRecord {
    /** Count sentinel meaning "all remaining levels/layers from the base". */
    static constexpr std::uint32_t s_remainingCount = 0xFFFFFFFFu;

    std::uint64_t sourceStageMask;
    std::uint64_t sourceAccessMask;
    std::uint64_t destinationStageMask;
    std::uint64_t destinationAccessMask;
    std::uint32_t imageSlot;
    std::uint32_t aspectMaskValue;
    std::uint32_t oldLayoutValue;
    std::uint32_t newLayoutValue;
    std::uint32_t baseMipLevel;
    std::uint32_t mipLevelCount;
    std::uint32_t baseArrayLayer;
    std::uint32_t arrayLayerCount;
};

static_assert(sizeof(CommandStreamImageBarrierRecord) == 64u,
              "Image barrier records must be exactly 64 bytes (schema v0.1)");
static_assert(offsetof(CommandStreamImageBarrierRecord, imageSlot) == 32u);
static_assert(offsetof(CommandStreamImageBarrierRecord, oldLayoutValue) == 40u);
static_assert(offsetof(CommandStreamImageBarrierRecord, baseMipLevel) == 48u);
static_assert(offsetof(CommandStreamImageBarrierRecord, arrayLayerCount) == 60u);

} // namespace barrieww
