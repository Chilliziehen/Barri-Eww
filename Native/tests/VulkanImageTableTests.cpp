#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamImageHandleTableEntry.hpp"
#include "BarriEww/CommandStream/CommandStreamImageHandleTableValidator.hpp"
#include "BarriEww/Vulkan/VulkanContext.hpp"
#include "BarriEww/Vulkan/VulkanImageTable.hpp"
#include "BarriEww/Vulkan/VulkanImageTableCreationError.hpp"
#include "TestVulkanDeviceHarness.hpp"

using barrieww::CommandStreamImageHandleTableEntry;
using barrieww::CommandStreamImageHandleTableValidator;
using barrieww::VulkanContext;
using barrieww::VulkanImageTable;
using barrieww::testing::TestVulkanDeviceHarness;

namespace {

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

/**
 * Builds one aligned image-handle table from fixed-width entries.
 * @param const std::vector<CommandStreamImageHandleTableEntry>& rawEntries Entries to encode
 * @return std::vector<std::byte> Complete image-handle table bytes
 */
std::vector<std::byte> makeImageTableBytes(
    const std::vector<CommandStreamImageHandleTableEntry>& rawEntries) {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, static_cast<std::uint32_t>(rawEntries.size()));
    appendValue(tableBytes, std::uint32_t{0});
    for (const CommandStreamImageHandleTableEntry& rawEntry : rawEntries) {
        appendValue(tableBytes, rawEntry.imageKindValue);
        appendValue(tableBytes, rawEntry.formatValue);
        appendValue(tableBytes, rawEntry.width);
        appendValue(tableBytes, rawEntry.height);
        appendValue(tableBytes, rawEntry.depth);
        appendValue(tableBytes, rawEntry.mipLevelCount);
        appendValue(tableBytes, rawEntry.arrayLayerCount);
        appendValue(tableBytes, rawEntry.sampleCountValue);
        appendValue(tableBytes, rawEntry.usageFlags);
        appendValue(tableBytes, rawEntry.importIdentifier);
    }
    return tableBytes;
}

} // namespace

TEST_CASE("Vulkan image table materializes empty and imported tables",
          "[vulkanImageTable][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};

    SECTION("empty table") {
        const std::vector<std::byte> tableBytes = makeImageTableBytes({});
        const auto tableView = CommandStreamImageHandleTableValidator::validate(tableBytes);
        REQUIRE(tableView.has_value());
        auto imageTable = VulkanImageTable::createFromTable(vulkanContext, *tableView);
        REQUIRE(imageTable.has_value());
        REQUIRE(imageTable->slotCount() == 0u);
        REQUIRE(imageTable->logicalDevice() == vulkanContext.logicalDevice());
    }

    SECTION("imported placeholder") {
        CommandStreamImageHandleTableEntry importedEntry{
            2u, 1u, 8u, 8u, 1u, 1u, 1u, 1u, 0u, 2026u};
        const std::vector<std::byte> tableBytes = makeImageTableBytes({importedEntry});
        const auto tableView = CommandStreamImageHandleTableValidator::validate(tableBytes);
        REQUIRE(tableView.has_value());
        auto imageTable = VulkanImageTable::createFromTable(vulkanContext, *tableView);
        REQUIRE(imageTable.has_value());
        REQUIRE(imageTable->slotCount() == 1u);
        REQUIRE(imageTable->slot(0u).image == VK_NULL_HANDLE);
        REQUIRE(imageTable->slot(0u).deviceMemory == VK_NULL_HANDLE);
        REQUIRE_FALSE(imageTable->slot(0u).isBound);
        REQUIRE(imageTable->slot(0u).description.importIdentifier == 2026u);
    }
}

TEST_CASE("Vulkan image table materializes every image kind and remaining usage bits",
          "[vulkanImageTable][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};

    CommandStreamImageHandleTableEntry oneDimensionalEntry{
        1u, 1u, 16u, 1u, 1u, 1u, 1u, 1u, 0x4u, 0u};
    CommandStreamImageHandleTableEntry threeDimensionalEntry{
        3u, 4u, 4u, 4u, 4u, 1u, 1u, 1u, 0x8u, 0u};
    CommandStreamImageHandleTableEntry depthEntry{
        2u, 5u, 8u, 8u, 1u, 1u, 1u, 1u, 0x20u, 0u};

    const std::array capabilityQueries{
        VkPhysicalDeviceImageFormatInfo2{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2, nullptr,
            VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_1D, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_SAMPLED_BIT, 0u},
        VkPhysicalDeviceImageFormatInfo2{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2, nullptr,
            VK_FORMAT_R32_UINT, VK_IMAGE_TYPE_3D, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_STORAGE_BIT, 0u},
        VkPhysicalDeviceImageFormatInfo2{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2, nullptr,
            VK_FORMAT_D32_SFLOAT, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0u},
    };
    for (const VkPhysicalDeviceImageFormatInfo2& capabilityQuery : capabilityQueries) {
        VkImageFormatProperties2 formatProperties{
            VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2};
        const VkResult queryResult = vkGetPhysicalDeviceImageFormatProperties2(
            vulkanContext.physicalDevice(), &capabilityQuery, &formatProperties);
        if (queryResult == VK_ERROR_FORMAT_NOT_SUPPORTED) {
            SKIP("driver does not support a requested image format/type/usage combination");
        }
        REQUIRE(queryResult == VK_SUCCESS);
    }

    const std::vector<std::byte> tableBytes = makeImageTableBytes(
        {oneDimensionalEntry, threeDimensionalEntry, depthEntry});
    const auto tableView = CommandStreamImageHandleTableValidator::validate(tableBytes);
    REQUIRE(tableView.has_value());
    auto imageTableResult = VulkanImageTable::createFromTable(vulkanContext, *tableView);
    REQUIRE(imageTableResult.has_value());
    REQUIRE(imageTableResult->slotCount() == 3u);
    for (std::uint32_t slotIndex = 0u; slotIndex < 3u; ++slotIndex) {
        REQUIRE(imageTableResult->slot(slotIndex).image != VK_NULL_HANDLE);
        REQUIRE(imageTableResult->slot(slotIndex).deviceMemory != VK_NULL_HANDLE);
        REQUIRE(imageTableResult->slot(slotIndex).isBound);
    }

    VulkanImageTable movedImageTable{std::move(*imageTableResult)};
    REQUIRE(imageTableResult->slotCount() == 0u);
    REQUIRE(movedImageTable.slotCount() == 3u);
    REQUIRE(movedImageTable.slot(1u).description.imageKindValue == 3u);
    REQUIRE(movedImageTable.slot(2u).description.formatValue == 5u);
}
