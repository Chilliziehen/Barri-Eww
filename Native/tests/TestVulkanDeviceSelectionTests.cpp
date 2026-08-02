#include <catch2/catch_test_macros.hpp>

#include <vulkan/vulkan.h>

#include "TestVulkanDeviceSelection.hpp"

using barrieww::testing::isStrictDynamicRenderingDeviceEligible;
using barrieww::testing::queueFamilySupportsRequiredOperations;

TEST_CASE("Vulkan test queue selection requires every requested operation",
          "[vulkanDeviceSelection]") {
    constexpr VkQueueFlags graphicsAndCompute = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;

    REQUIRE(queueFamilySupportsRequiredOperations(graphicsAndCompute, graphicsAndCompute));
    REQUIRE(queueFamilySupportsRequiredOperations(graphicsAndCompute, VK_QUEUE_GRAPHICS_BIT));
    REQUIRE_FALSE(queueFamilySupportsRequiredOperations(VK_QUEUE_GRAPHICS_BIT, graphicsAndCompute));
}

TEST_CASE("Strict dynamic rendering eligibility rejects a missing feature bit",
          "[vulkanDeviceSelection]") {
    REQUIRE_FALSE(isStrictDynamicRenderingDeviceEligible(VK_API_VERSION_1_2, true, false));
    REQUIRE(isStrictDynamicRenderingDeviceEligible(VK_API_VERSION_1_2, true, true));
    REQUIRE_FALSE(isStrictDynamicRenderingDeviceEligible(VK_API_VERSION_1_1, true, true));
    REQUIRE_FALSE(isStrictDynamicRenderingDeviceEligible(VK_API_VERSION_1_2, false, true));
}
