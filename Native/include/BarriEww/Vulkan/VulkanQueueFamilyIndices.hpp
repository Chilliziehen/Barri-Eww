#pragma once

#include <cstdint>
#include <optional>

namespace barrieww {

/**
 * @note ThreadSafety: Trivially thread-safe as a plain value type; it carries no
 *       synchronization and may be copied freely across threads.
 * @brief Queue family indices resolved for a physical device. Each field is engaged only
 *        when the corresponding queue family was found. Families may coincide (for example
 *        graphics and present sharing one family), so the indices are not guaranteed to be
 *        distinct.
 */
struct VulkanQueueFamilyIndices {
    std::optional<std::uint32_t> graphicsFamily;
    std::optional<std::uint32_t> presentFamily;
    std::optional<std::uint32_t> transferFamily;
    std::optional<std::uint32_t> computeFamily;

    /**
     * @note ThreadSafety: Thread-safe; reads only local value fields.
     * @brief Whether the families required to drive presentation rendering are present,
     *        namely a graphics family and a present family.
     * @return bool True when both graphicsFamily and presentFamily are engaged.
     */
    [[nodiscard]] bool hasRequiredFamilies() const noexcept {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

} // namespace barrieww
