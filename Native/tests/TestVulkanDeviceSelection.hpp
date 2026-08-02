#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

namespace barrieww::testing {

/**
 * @note ThreadSafety: Pure and safe from every thread.
 * @brief Reports whether one queue family contains every requested operation
 * flag.
 * @param VkQueueFlags availableQueueFlags Operations advertised by the queue
 * family
 * @param VkQueueFlags requiredQueueFlags Operations required by the test
 * profile
 * @return bool True when all required flags are present
 * @warning MemoryOwnership: Accepts and returns values without accessing
 * external memory.
 */
constexpr bool queueFamilySupportsRequiredOperations(VkQueueFlags availableQueueFlags,
                                                     VkQueueFlags requiredQueueFlags) noexcept {
    return (availableQueueFlags & requiredQueueFlags) == requiredQueueFlags;
}

/**
 * @note ThreadSafety: Pure and safe from every thread.
 * @brief Reports strict Vulkan 1.2 KHR dynamic-rendering device eligibility.
 * @param std::uint32_t applicationProgrammingInterfaceVersion Physical-device Vulkan
 *        application programming interface version
 * @param bool hasDynamicRenderingExtension Whether VK_KHR_dynamic_rendering is
 * advertised
 * @param bool hasDynamicRenderingFeature Whether its dynamicRendering feature
 * bit is true
 * @return bool True when all strict profile requirements are satisfied
 * @warning MemoryOwnership: Accepts and returns values without accessing
 * external memory.
 */
constexpr bool isStrictDynamicRenderingDeviceEligible(
    std::uint32_t applicationProgrammingInterfaceVersion,
    bool hasDynamicRenderingExtension,
    bool hasDynamicRenderingFeature) noexcept {
    return applicationProgrammingInterfaceVersion >= VK_API_VERSION_1_2
        && hasDynamicRenderingExtension && hasDynamicRenderingFeature;
}

} // namespace barrieww::testing
