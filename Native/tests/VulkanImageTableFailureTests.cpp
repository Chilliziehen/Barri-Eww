#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include <vulkan/vulkan.h>

#include "BarriEww/CommandStream/CommandStreamImageHandleTableValidator.hpp"
#include "BarriEww/Vulkan/VulkanContext.hpp"
#include "BarriEww/Vulkan/VulkanImageTable.hpp"
#include "BarriEww/Vulkan/VulkanImageTableCreationError.hpp"

using barrieww::CommandStreamImageHandleTableValidator;
using barrieww::VulkanContext;
using barrieww::VulkanContextCreateInfo;
using barrieww::VulkanImageTable;
using barrieww::VulkanImageTableCreationError;

namespace {

bool g_hasDeviceLocalMemoryType = true;
std::uint32_t g_destroyImageCallCount = 0u;
std::uint32_t g_freeMemoryCallCount = 0u;
VkResult g_createImageResult = VK_SUCCESS;
VkResult g_allocateMemoryResult = VK_SUCCESS;
VkResult g_bindImageMemoryResult = VK_SUCCESS;

/** Resets deterministic Vulkan results and destruction counters. */
void resetVulkanCallState() {
    g_createImageResult = VK_SUCCESS;
    g_allocateMemoryResult = VK_SUCCESS;
    g_bindImageMemoryResult = VK_SUCCESS;
    g_hasDeviceLocalMemoryType = true;
    g_destroyImageCallCount = 0u;
    g_freeMemoryCallCount = 0u;
}

/**
 * Appends one little-endian value to a byte vector.
 * @param std::vector<std::byte>& targetBytes Destination byte vector
 * @param ValueType value Fixed-width value to append
 */
template <typename ValueType>
void appendValue(std::vector<std::byte>& targetBytes, ValueType value) {
    const std::size_t writeOffset = targetBytes.size();
    targetBytes.resize(writeOffset + sizeof value);
    std::memcpy(targetBytes.data() + writeOffset, &value, sizeof value);
}

/** Builds one valid loader-created two-dimensional image entry.
 * @return std::vector<std::byte> Complete one-entry table bytes
 */
std::vector<std::byte> makeImageTableBytes() {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{2}); // TwoDimensional
    appendValue(tableBytes, std::uint32_t{1}); // R8G8B8A8Unorm
    appendValue(tableBytes, std::uint32_t{8});
    appendValue(tableBytes, std::uint32_t{8});
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0x2}); // TransferDestination
    appendValue(tableBytes, std::uint32_t{0});
    return tableBytes;
}

/** Creates a context from non-null opaque handles consumed only by local replacements.
 * @return VulkanContext Borrowed-handle context for the isolated failure executable
 */
VulkanContext makeVulkanContext() {
    VulkanContextCreateInfo createInfo{};
    createInfo.instance = reinterpret_cast<VkInstance>(1u);
    createInfo.physicalDevice = reinterpret_cast<VkPhysicalDevice>(2u);
    createInfo.logicalDevice = reinterpret_cast<VkDevice>(3u);
    createInfo.queueFamilyIndices.graphicsFamily = 0u;
    createInfo.queueFamilyIndices.presentFamily = 0u;
    createInfo.graphicsQueue = reinterpret_cast<VkQueue>(4u);
    createInfo.presentQueue = reinterpret_cast<VkQueue>(4u);
    return VulkanContext{createInfo};
}

} // namespace

/**
 * Supplies deterministic physical-device memory properties.
 * @param VkPhysicalDevice physicalDevice Opaque physical-device handle
 * @param VkPhysicalDeviceMemoryProperties* memoryProperties Writable properties
 */
extern "C" void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* memoryProperties) {
    static_cast<void>(physicalDevice);
    *memoryProperties = {};
    memoryProperties->memoryTypeCount = 1u;
    memoryProperties->memoryTypes[0].propertyFlags =
        g_hasDeviceLocalMemoryType
            ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            : 0u;
}

/**
 * Deterministic link-time replacement for image creation.
 * @param VkDevice device Opaque device handle
 * @param const VkImageCreateInfo* createInfo Requested image description
 * @param const VkAllocationCallbacks* allocationCallbacks Optional allocator callbacks
 * @param VkImage* image Writable image handle
 * @return VkResult Configured creation result
 */
extern "C" VkResult VKAPI_CALL vkCreateImage(
    VkDevice device, const VkImageCreateInfo* createInfo,
    const VkAllocationCallbacks* allocationCallbacks, VkImage* image) {
    static_cast<void>(device);
    static_cast<void>(createInfo);
    static_cast<void>(allocationCallbacks);
    if (g_createImageResult == VK_SUCCESS) {
        *image = reinterpret_cast<VkImage>(100u);
    }
    return g_createImageResult;
}

/**
 * Supplies deterministic memory requirements for the replacement image.
 * @param VkDevice device Opaque device handle
 * @param VkImage image Opaque image handle
 * @param VkMemoryRequirements* memoryRequirements Writable requirements
 */
extern "C" void VKAPI_CALL vkGetImageMemoryRequirements(
    VkDevice device, VkImage image, VkMemoryRequirements* memoryRequirements) {
    static_cast<void>(device);
    static_cast<void>(image);
    memoryRequirements->size = 4096u;
    memoryRequirements->alignment = 256u;
    memoryRequirements->memoryTypeBits = 1u;
}

/**
 * Deterministic link-time replacement for memory allocation.
 * @param VkDevice device Opaque device handle
 * @param const VkMemoryAllocateInfo* allocateInfo Requested allocation
 * @param const VkAllocationCallbacks* allocationCallbacks Optional allocator callbacks
 * @param VkDeviceMemory* deviceMemory Writable memory handle
 * @return VkResult Configured allocation result
 */
extern "C" VkResult VKAPI_CALL vkAllocateMemory(
    VkDevice device, const VkMemoryAllocateInfo* allocateInfo,
    const VkAllocationCallbacks* allocationCallbacks, VkDeviceMemory* deviceMemory) {
    static_cast<void>(device);
    static_cast<void>(allocateInfo);
    static_cast<void>(allocationCallbacks);
    if (g_allocateMemoryResult == VK_SUCCESS) {
        *deviceMemory = reinterpret_cast<VkDeviceMemory>(200u);
    }
    return g_allocateMemoryResult;
}

/**
 * Deterministic link-time replacement for image-memory binding.
 * @param VkDevice device Opaque device handle
 * @param VkImage image Opaque image handle
 * @param VkDeviceMemory deviceMemory Opaque memory handle
 * @param VkDeviceSize memoryOffset Binding offset
 * @return VkResult Configured binding result
 */
extern "C" VkResult VKAPI_CALL vkBindImageMemory(
    VkDevice device, VkImage image, VkDeviceMemory deviceMemory,
    VkDeviceSize memoryOffset) {
    static_cast<void>(device);
    static_cast<void>(image);
    static_cast<void>(deviceMemory);
    static_cast<void>(memoryOffset);
    return g_bindImageMemoryResult;
}

/**
 * Records deterministic image destruction.
 * @param VkDevice device Opaque device handle
 * @param VkImage image Image being destroyed
 * @param const VkAllocationCallbacks* allocationCallbacks Optional allocator callbacks
 */
extern "C" void VKAPI_CALL vkDestroyImage(
    VkDevice device, VkImage image, const VkAllocationCallbacks* allocationCallbacks) {
    static_cast<void>(device);
    static_cast<void>(image);
    static_cast<void>(allocationCallbacks);
    ++g_destroyImageCallCount;
}

/**
 * Records deterministic memory release.
 * @param VkDevice device Opaque device handle
 * @param VkDeviceMemory deviceMemory Memory being released
 * @param const VkAllocationCallbacks* allocationCallbacks Optional allocator callbacks
 */
extern "C" void VKAPI_CALL vkFreeMemory(
    VkDevice device, VkDeviceMemory deviceMemory,
    const VkAllocationCallbacks* allocationCallbacks) {
    static_cast<void>(device);
    static_cast<void>(deviceMemory);
    static_cast<void>(allocationCallbacks);
    ++g_freeMemoryCallCount;
}

TEST_CASE("Vulkan image table reports deterministic creation failures and rolls back",
          "[vulkanImageTable][failure]") {
    const VulkanContext vulkanContext = makeVulkanContext();
    const std::vector<std::byte> tableBytes = makeImageTableBytes();
    const auto tableView = CommandStreamImageHandleTableValidator::validate(tableBytes);
    REQUIRE(tableView.has_value());

    SECTION("image creation failure") {
        resetVulkanCallState();
        g_createImageResult = VK_ERROR_FORMAT_NOT_SUPPORTED;
        const auto result = VulkanImageTable::createFromTable(vulkanContext, *tableView);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().error == VulkanImageTableCreationError::ImageCreationFailed);
        REQUIRE(result.error().slotIndex == 0u);
        REQUIRE(result.error().resultValue == VK_ERROR_FORMAT_NOT_SUPPORTED);
        REQUIRE(g_destroyImageCallCount == 0u);
        REQUIRE(g_freeMemoryCallCount == 0u);
    }

    SECTION("no device-local memory type") {
        resetVulkanCallState();
        g_hasDeviceLocalMemoryType = false;
        const auto result = VulkanImageTable::createFromTable(vulkanContext, *tableView);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().error == VulkanImageTableCreationError::NoSuitableMemoryType);
        REQUIRE(result.error().resultValue == VK_SUCCESS);
        REQUIRE(g_destroyImageCallCount == 1u);
        REQUIRE(g_freeMemoryCallCount == 0u);
    }

    SECTION("memory allocation failure") {
        resetVulkanCallState();
        g_allocateMemoryResult = VK_ERROR_OUT_OF_DEVICE_MEMORY;
        const auto result = VulkanImageTable::createFromTable(vulkanContext, *tableView);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().error == VulkanImageTableCreationError::MemoryAllocationFailed);
        REQUIRE(result.error().resultValue == VK_ERROR_OUT_OF_DEVICE_MEMORY);
        REQUIRE(g_destroyImageCallCount == 1u);
        REQUIRE(g_freeMemoryCallCount == 0u);
    }

    SECTION("memory binding failure") {
        resetVulkanCallState();
        g_bindImageMemoryResult = VK_ERROR_DEVICE_LOST;
        const auto result = VulkanImageTable::createFromTable(vulkanContext, *tableView);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().error == VulkanImageTableCreationError::MemoryBindingFailed);
        REQUIRE(result.error().resultValue == VK_ERROR_DEVICE_LOST);
        REQUIRE(g_destroyImageCallCount == 1u);
        REQUIRE(g_freeMemoryCallCount == 1u);
    }
}
