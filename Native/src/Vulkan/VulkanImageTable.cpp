#include "BarriEww/Vulkan/VulkanImageTable.hpp"

#include <cstdint>

#include "BarriEww/CommandStream/CommandStreamImageFormat.hpp"
#include "BarriEww/CommandStream/CommandStreamImageKind.hpp"
#include "BarriEww/CommandStream/CommandStreamImageUsage.hpp"
#include "BarriEww/Vulkan/VulkanFormatMapping.hpp"

namespace barrieww {

namespace {

/** Maps the backend-neutral image usage mask onto VkImageUsageFlags (T0[3] mapping point). */
VkImageUsageFlags mapImageUsageFlags(std::uint32_t neutralUsageFlags) {
    VkImageUsageFlags vulkanUsageFlags = 0;
    const auto mapBit = [&](CommandStreamImageUsage neutralBit,
                            VkImageUsageFlagBits vulkanBit) {
        if (neutralUsageFlags & static_cast<std::uint32_t>(neutralBit)) {
            vulkanUsageFlags |= vulkanBit;
        }
    };
    mapBit(CommandStreamImageUsage::TransferSource, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    mapBit(CommandStreamImageUsage::TransferDestination, VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    mapBit(CommandStreamImageUsage::Sampled, VK_IMAGE_USAGE_SAMPLED_BIT);
    mapBit(CommandStreamImageUsage::Storage, VK_IMAGE_USAGE_STORAGE_BIT);
    mapBit(CommandStreamImageUsage::ColorAttachment, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    mapBit(CommandStreamImageUsage::DepthStencilAttachment,
           VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    return vulkanUsageFlags;
}

/** Maps the neutral image kind onto VkImageType. */
VkImageType mapImageKind(std::uint32_t imageKindValue) {
    switch (static_cast<CommandStreamImageKind>(imageKindValue)) {
        case CommandStreamImageKind::OneDimensional: return VK_IMAGE_TYPE_1D;
        case CommandStreamImageKind::TwoDimensional: return VK_IMAGE_TYPE_2D;
        case CommandStreamImageKind::ThreeDimensional: return VK_IMAGE_TYPE_3D;
    }
    return VK_IMAGE_TYPE_2D;
}

/** Selects the first device-local memory type allowed by typeBits, or -1. */
std::int32_t selectDeviceLocalMemoryType(
    const VkPhysicalDeviceMemoryProperties& memoryProperties,
    std::uint32_t allowedTypeBits) {
    for (std::uint32_t typeIndex = 0; typeIndex < memoryProperties.memoryTypeCount;
         ++typeIndex) {
        const bool isAllowedType = (allowedTypeBits & (1u << typeIndex)) != 0u;
        const bool isDeviceLocal =
            (memoryProperties.memoryTypes[typeIndex].propertyFlags
             & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0u;
        if (isAllowedType && isDeviceLocal) {
            return static_cast<std::int32_t>(typeIndex);
        }
    }
    return -1;
}

} // namespace

std::expected<VulkanImageTable, VulkanImageTableCreationFailure>
VulkanImageTable::createFromTable(const VulkanContext& vulkanContext,
                                  const CommandStreamImageHandleTableView& tableView) {
    using enum VulkanImageTableCreationError;

    const VkDevice logicalDevice = vulkanContext.logicalDevice();
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(vulkanContext.physicalDevice(), &memoryProperties);

    VulkanImageTable partialTable{logicalDevice, {}};
    partialTable.m_imageSlots.reserve(tableView.entryCount());

    const auto fail = [](VulkanImageTableCreationError error, std::uint32_t slotIndex,
                         VkResult vulkanResult) {
        return std::unexpected(VulkanImageTableCreationFailure{
            error, slotIndex, static_cast<std::int32_t>(vulkanResult)});
    };

    for (std::uint32_t slotIndex = 0; slotIndex < tableView.entryCount(); ++slotIndex) {
        const CommandStreamImageHandleTableEntry entry = tableView.entry(slotIndex);

        ImageSlot imageSlot{};
        imageSlot.description = entry;
        if (entry.isImported()) {
            // v0.1: imported slots stay unbound placeholders (host binding is a later
            // increment); the recorder rejects commands that reference them.
            partialTable.m_imageSlots.push_back(imageSlot);
            continue;
        }

        VkImageCreateInfo imageCreateInfo{};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType = mapImageKind(entry.imageKindValue);
        imageCreateInfo.format = mapCommandStreamImageFormat(
            static_cast<CommandStreamImageFormat>(entry.formatValue));
        imageCreateInfo.extent = VkExtent3D{entry.width, entry.height, entry.depth};
        imageCreateInfo.mipLevels = entry.mipLevelCount;
        imageCreateInfo.arrayLayers = entry.arrayLayerCount;
        imageCreateInfo.samples =
            static_cast<VkSampleCountFlagBits>(entry.sampleCountValue);
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.usage = mapImageUsageFlags(entry.usageFlags);
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkResult vulkanResult =
            vkCreateImage(logicalDevice, &imageCreateInfo, nullptr, &imageSlot.image);
        if (vulkanResult != VK_SUCCESS) {
            return fail(ImageCreationFailed, slotIndex, vulkanResult);
        }
        partialTable.m_imageSlots.push_back(imageSlot);
        ImageSlot& ownedSlot = partialTable.m_imageSlots.back();

        VkMemoryRequirements memoryRequirements{};
        vkGetImageMemoryRequirements(logicalDevice, ownedSlot.image, &memoryRequirements);
        const std::int32_t memoryTypeIndex = selectDeviceLocalMemoryType(
            memoryProperties, memoryRequirements.memoryTypeBits);
        if (memoryTypeIndex < 0) {
            return fail(NoSuitableMemoryType, slotIndex, VK_SUCCESS);
        }

        VkMemoryAllocateInfo memoryAllocateInfo{};
        memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocateInfo.allocationSize = memoryRequirements.size;
        memoryAllocateInfo.memoryTypeIndex = static_cast<std::uint32_t>(memoryTypeIndex);
        vulkanResult = vkAllocateMemory(logicalDevice, &memoryAllocateInfo, nullptr,
                                        &ownedSlot.deviceMemory);
        if (vulkanResult != VK_SUCCESS) {
            return fail(MemoryAllocationFailed, slotIndex, vulkanResult);
        }

        vulkanResult = vkBindImageMemory(logicalDevice, ownedSlot.image,
                                         ownedSlot.deviceMemory, 0);
        if (vulkanResult != VK_SUCCESS) {
            return fail(MemoryBindingFailed, slotIndex, vulkanResult);
        }
        ownedSlot.isBound = true;
    }

    return partialTable;
}

VulkanImageTable::~VulkanImageTable() {
    for (ImageSlot& imageSlot : m_imageSlots) {
        if (imageSlot.image != VK_NULL_HANDLE) {
            vkDestroyImage(m_logicalDevice, imageSlot.image, nullptr);
        }
        if (imageSlot.deviceMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_logicalDevice, imageSlot.deviceMemory, nullptr);
        }
    }
}

} // namespace barrieww
