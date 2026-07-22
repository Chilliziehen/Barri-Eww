#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumerations and the constexpr predicate below are stateless and
 *       fully thread-safe.
 * @brief Backend-neutral compare operations of the GraphicsPipelineTable schema (v0.1;
 *        used by depth testing). The loader maps them onto the concrete API (Vulkan:
 *        VkCompareOp). Zero is deliberately unassigned: a record with depth testing
 *        disabled must carry 0 here, so no usable operation can be confused with the
 *        disabled state.
 */
enum class CommandStreamCompareOperation : std::uint32_t {
    Never = 1,
    Less = 2,
    Equal = 3,
    LessOrEqual = 4,
    Greater = 5,
    NotEqual = 6,
    GreaterOrEqual = 7,
    Always = 8,
};

/**
 * @note ThreadSafety: Thread-safe (pure constant expression).
 * @brief Whether the raw value names an assigned compare operation.
 * @param rawCompareOperationValue The raw compare operation value read from a record.
 * @return bool True when the value is assigned in the v0.1 schema.
 */
[[nodiscard]] constexpr bool
isAssignedCommandStreamCompareOperation(std::uint32_t rawCompareOperationValue) noexcept {
    return rawCompareOperationValue >= 1u && rawCompareOperationValue <= 8u;
}

} // namespace barrieww
