#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <stdexcept>

#include "BarriEww/Vulkan/VulkanContext.hpp"
#include "BarriEww/Vulkan/VulkanContextCreateInfo.hpp"

using barrieww::VulkanContext;
using barrieww::VulkanContextCreateInfo;

namespace {

// Sentinel non-null Vulkan handles for holder-level tests. VulkanContext only stores and
// returns these; it performs no Vulkan calls, so the handles are never dereferenced.
template <typename HandleType>
HandleType makeSentinelHandle(std::uintptr_t value) {
    return reinterpret_cast<HandleType>(value);
}

VulkanContextCreateInfo makeValidCreateInfo() {
    VulkanContextCreateInfo createInfo{};
    createInfo.instance = makeSentinelHandle<VkInstance>(0x1);
    createInfo.physicalDevice = makeSentinelHandle<VkPhysicalDevice>(0x2);
    createInfo.logicalDevice = makeSentinelHandle<VkDevice>(0x3);
    createInfo.queueFamilyIndices.graphicsFamily = 0u;
    createInfo.queueFamilyIndices.presentFamily = 0u;
    createInfo.graphicsQueue = makeSentinelHandle<VkQueue>(0x4);
    createInfo.presentQueue = makeSentinelHandle<VkQueue>(0x5);
    return createInfo;
}

} // namespace

TEST_CASE("VulkanContext exposes the handles it was constructed with", "[vulkanContext]") {
    const VulkanContext context{makeValidCreateInfo()};

    REQUIRE(context.instance() == makeSentinelHandle<VkInstance>(0x1));
    REQUIRE(context.physicalDevice() == makeSentinelHandle<VkPhysicalDevice>(0x2));
    REQUIRE(context.logicalDevice() == makeSentinelHandle<VkDevice>(0x3));
    REQUIRE(context.graphicsQueue() == makeSentinelHandle<VkQueue>(0x4));
    REQUIRE(context.presentQueue() == makeSentinelHandle<VkQueue>(0x5));
    REQUIRE(context.queueFamilyIndices().hasRequiredFamilies());
}

TEST_CASE("VulkanContext rejects null mandatory handles and missing families",
          "[vulkanContext]") {
    SECTION("null instance is rejected") {
        VulkanContextCreateInfo createInfo = makeValidCreateInfo();
        createInfo.instance = VK_NULL_HANDLE;
        REQUIRE_THROWS_AS(VulkanContext{createInfo}, std::invalid_argument);
    }

    SECTION("null logical device is rejected") {
        VulkanContextCreateInfo createInfo = makeValidCreateInfo();
        createInfo.logicalDevice = VK_NULL_HANDLE;
        REQUIRE_THROWS_AS(VulkanContext{createInfo}, std::invalid_argument);
    }

    SECTION("null graphics queue is rejected") {
        VulkanContextCreateInfo createInfo = makeValidCreateInfo();
        createInfo.graphicsQueue = VK_NULL_HANDLE;
        REQUIRE_THROWS_AS(VulkanContext{createInfo}, std::invalid_argument);
    }

    SECTION("missing present family is rejected") {
        VulkanContextCreateInfo createInfo = makeValidCreateInfo();
        createInfo.queueFamilyIndices.presentFamily.reset();
        REQUIRE_THROWS_AS(VulkanContext{createInfo}, std::invalid_argument);
    }
}
