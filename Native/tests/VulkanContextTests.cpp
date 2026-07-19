#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <stdexcept>

#include "BarriEww/Vulkan/VulkanContext.hpp"
#include "BarriEww/Vulkan/VulkanContextCreateInformation.hpp"

using barrieww::VulkanContext;
using barrieww::VulkanContextCreateInformation;

namespace {

// Sentinel non-null Vulkan handles for holder-level tests. VulkanContext only stores and
// returns these; it performs no Vulkan calls, so the handles are never dereferenced.
template <typename HandleType>
HandleType makeSentinelHandle(std::uintptr_t value) {
    return reinterpret_cast<HandleType>(value);
}

VulkanContextCreateInformation makeValidCreateInformation() {
    VulkanContextCreateInformation createInformation{};
    createInformation.instance = makeSentinelHandle<VkInstance>(0x1);
    createInformation.physicalDevice = makeSentinelHandle<VkPhysicalDevice>(0x2);
    createInformation.logicalDevice = makeSentinelHandle<VkDevice>(0x3);
    createInformation.queueFamilyIndices.graphicsFamily = 0u;
    createInformation.queueFamilyIndices.presentFamily = 0u;
    createInformation.graphicsQueue = makeSentinelHandle<VkQueue>(0x4);
    createInformation.presentQueue = makeSentinelHandle<VkQueue>(0x5);
    return createInformation;
}

} // namespace

TEST_CASE("VulkanContext exposes the handles it was constructed with", "[vulkanContext]") {
    const VulkanContext context{makeValidCreateInformation()};

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
        VulkanContextCreateInformation createInformation = makeValidCreateInformation();
        createInformation.instance = VK_NULL_HANDLE;
        REQUIRE_THROWS_AS(VulkanContext{createInformation}, std::invalid_argument);
    }

    SECTION("null logical device is rejected") {
        VulkanContextCreateInformation createInformation = makeValidCreateInformation();
        createInformation.logicalDevice = VK_NULL_HANDLE;
        REQUIRE_THROWS_AS(VulkanContext{createInformation}, std::invalid_argument);
    }

    SECTION("null graphics queue is rejected") {
        VulkanContextCreateInformation createInformation = makeValidCreateInformation();
        createInformation.graphicsQueue = VK_NULL_HANDLE;
        REQUIRE_THROWS_AS(VulkanContext{createInformation}, std::invalid_argument);
    }

    SECTION("missing present family is rejected") {
        VulkanContextCreateInformation createInformation = makeValidCreateInformation();
        createInformation.queueFamilyIndices.presentFamily.reset();
        REQUIRE_THROWS_AS(VulkanContext{createInformation}, std::invalid_argument);
    }
}
