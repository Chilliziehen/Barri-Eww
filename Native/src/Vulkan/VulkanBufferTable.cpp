#include "BarriEww/Vulkan/VulkanBufferTable.hpp"

#include <cstdint>

#include "BarriEww/CommandStream/CommandStreamBufferMemoryKind.hpp"
#include "BarriEww/CommandStream/CommandStreamBufferUsage.hpp"

namespace barrieww {

namespace {

/** Maps the backend-neutral usage mask onto VkBufferUsageFlags (T0[3] mapping point). */
VkBufferUsageFlags mapUsageFlags(std::uint32_t neutralUsageFlags) {
    VkBufferUsageFlags vulkanUsageFlags = 0;
    const auto mapBit = [&](CommandStreamBufferUsage neutralBit,
                            VkBufferUsageFlagBits vulkanBit) {
        if (neutralUsageFlags & static_cast<std::uint32_t>(neutralBit)) {
            vulkanUsageFlags |= vulkanBit;
        }
    };
    mapBit(CommandStreamBufferUsage::TransferSource, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    mapBit(CommandStreamBufferUsage::TransferDestination, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    mapBit(CommandStreamBufferUsage::Vertex, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    mapBit(CommandStreamBufferUsage::Index, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    mapBit(CommandStreamBufferUsage::Uniform, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    mapBit(CommandStreamBufferUsage::Storage, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    mapBit(CommandStreamBufferUsage::Indirect, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
    mapBit(CommandStreamBufferUsage::DeviceAddress,
           VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    return vulkanUsageFlags;
}

/**
 * @note ThreadSafety: Pure function; thread-safe.
 * @brief Selects a memory type index for the given kind. Algorithm principle: a memory
 *        type is eligible when the buffer's memoryTypeBits allow it AND it has every
 *        required property; among eligible types, one that also has the preferred
 *        properties wins. Two passes implement "required + preferred first, required
 *        only as fallback".
 *
 *        Pseudocode:
 *          selectMemoryType(allowedTypeBits, requiredProperties, preferredProperties):
 *            for candidateProperties in [required | preferred, required]:
 *              for typeIndex in 0 .. memoryTypeCount - 1:
 *                if allowedTypeBits has typeIndex
 *                   and memoryTypes[typeIndex].propertyFlags contains candidateProperties:
 *                  return typeIndex
 *            return NotFound
 * @param memoryProperties The physical device's memory properties.
 * @param allowedTypeBits The buffer's memoryTypeBits requirement.
 * @param requiredProperties Property flags that must be present.
 * @param preferredProperties Property flags that should be present when possible.
 * @return std::int32_t The selected type index, or -1 when none qualifies.
 */
std::int32_t selectMemoryType(const VkPhysicalDeviceMemoryProperties& memoryProperties,
                              std::uint32_t allowedTypeBits,
                              VkMemoryPropertyFlags requiredProperties,
                              VkMemoryPropertyFlags preferredProperties) {
    const VkMemoryPropertyFlags candidateSets[2] = {
        requiredProperties | preferredProperties, requiredProperties};
    for (VkMemoryPropertyFlags candidateProperties : candidateSets) {
        for (std::uint32_t typeIndex = 0; typeIndex < memoryProperties.memoryTypeCount;
             ++typeIndex) {
            const bool isAllowedType = (allowedTypeBits & (1u << typeIndex)) != 0u;
            const bool hasAllProperties =
                (memoryProperties.memoryTypes[typeIndex].propertyFlags & candidateProperties)
                == candidateProperties;
            if (isAllowedType && hasAllProperties) {
                return static_cast<std::int32_t>(typeIndex);
            }
        }
    }
    return -1;
}

} // namespace

std::expected<VulkanBufferTable, VulkanBufferTableCreationFailure>
VulkanBufferTable::createFromTable(const VulkanContext& vulkanContext,
                                   const CommandStreamBufferHandleTableView& tableView) {
    using enum VulkanBufferTableCreationError;

    const VkDevice logicalDevice = vulkanContext.logicalDevice();
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(vulkanContext.physicalDevice(), &memoryProperties);

    // Partially-built table; RAII rollback on any failure path via its destructor.
    VulkanBufferTable partialTable{logicalDevice, {}};
    partialTable.m_bufferSlots.reserve(tableView.entryCount());

    const auto fail = [](VulkanBufferTableCreationError error, std::uint32_t slotIndex,
                         VkResult vulkanResult) {
        return std::unexpected(VulkanBufferTableCreationFailure{
            error, slotIndex, static_cast<std::int32_t>(vulkanResult)});
    };

    for (std::uint32_t slotIndex = 0; slotIndex < tableView.entryCount(); ++slotIndex) {
        const CommandStreamBufferHandleTableEntry entry = tableView.entry(slotIndex);

        if (entry.isImported()) {
            // v0.1: imported slots stay unbound placeholders (host binding is a later
            // increment); the recorder rejects commands that reference them.
            partialTable.m_bufferSlots.push_back(BufferSlot{});
            continue;
        }

        VkBufferCreateInfo bufferCreateInfo{};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = entry.byteSize;
        bufferCreateInfo.usage = mapUsageFlags(entry.usageFlags);
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        BufferSlot bufferSlot{};
        bufferSlot.byteSize = entry.byteSize;
        VkResult vulkanResult =
            vkCreateBuffer(logicalDevice, &bufferCreateInfo, nullptr, &bufferSlot.buffer);
        if (vulkanResult != VK_SUCCESS) {
            return fail(BufferCreationFailed, slotIndex, vulkanResult);
        }
        // Own the partially-initialized slot immediately so rollback covers it.
        partialTable.m_bufferSlots.push_back(bufferSlot);
        BufferSlot& ownedSlot = partialTable.m_bufferSlots.back();

        VkMemoryRequirements memoryRequirements{};
        vkGetBufferMemoryRequirements(logicalDevice, ownedSlot.buffer, &memoryRequirements);

        const auto memoryKind =
            static_cast<CommandStreamBufferMemoryKind>(entry.memoryKindValue);
        VkMemoryPropertyFlags requiredProperties = 0;
        VkMemoryPropertyFlags preferredProperties = 0;
        switch (memoryKind) {
            case CommandStreamBufferMemoryKind::DeviceLocal:
                requiredProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                break;
            case CommandStreamBufferMemoryKind::HostVisiblePersistentMapped:
                requiredProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                     | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                break;
            case CommandStreamBufferMemoryKind::HostVisibleReadback:
                requiredProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                     | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                preferredProperties = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
                break;
            case CommandStreamBufferMemoryKind::None:
                break; // Unreachable: validated created entries never carry None.
        }

        const std::int32_t memoryTypeIndex =
            selectMemoryType(memoryProperties, memoryRequirements.memoryTypeBits,
                             requiredProperties, preferredProperties);
        if (memoryTypeIndex < 0) {
            return fail(NoSuitableMemoryType, slotIndex, VK_SUCCESS);
        }

        const bool wantsDeviceAddress =
            (entry.usageFlags
             & static_cast<std::uint32_t>(CommandStreamBufferUsage::DeviceAddress))
            != 0u;
        VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
        memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        VkMemoryAllocateInfo memoryAllocateInfo{};
        memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocateInfo.pNext = wantsDeviceAddress ? &memoryAllocateFlagsInfo : nullptr;
        memoryAllocateInfo.allocationSize = memoryRequirements.size;
        memoryAllocateInfo.memoryTypeIndex = static_cast<std::uint32_t>(memoryTypeIndex);
        vulkanResult = vkAllocateMemory(logicalDevice, &memoryAllocateInfo, nullptr,
                                        &ownedSlot.deviceMemory);
        if (vulkanResult != VK_SUCCESS) {
            return fail(MemoryAllocationFailed, slotIndex, vulkanResult);
        }

        vulkanResult =
            vkBindBufferMemory(logicalDevice, ownedSlot.buffer, ownedSlot.deviceMemory, 0);
        if (vulkanResult != VK_SUCCESS) {
            return fail(MemoryBindingFailed, slotIndex, vulkanResult);
        }

        if (memoryKind == CommandStreamBufferMemoryKind::HostVisiblePersistentMapped
            || memoryKind == CommandStreamBufferMemoryKind::HostVisibleReadback) {
            void* mappedAddress = nullptr;
            vulkanResult = vkMapMemory(logicalDevice, ownedSlot.deviceMemory, 0,
                                       VK_WHOLE_SIZE, 0, &mappedAddress);
            if (vulkanResult != VK_SUCCESS) {
                return fail(MemoryMappingFailed, slotIndex, vulkanResult);
            }
            ownedSlot.mappedPointer = static_cast<std::byte*>(mappedAddress);
        }
        if (wantsDeviceAddress) {
            // Requires the bufferDeviceAddress device feature (core 1.2), enabled by
            // the host that owns the device (the test harness, later Minecraft).
            VkBufferDeviceAddressInfo bufferDeviceAddressInfo{};
            bufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            bufferDeviceAddressInfo.buffer = ownedSlot.buffer;
            ownedSlot.deviceAddress =
                vkGetBufferDeviceAddress(logicalDevice, &bufferDeviceAddressInfo);
        }
        ownedSlot.isBound = true;
    }

    return partialTable;
}

VulkanBufferTable::~VulkanBufferTable() {
    for (BufferSlot& bufferSlot : m_bufferSlots) {
        if (bufferSlot.mappedPointer != nullptr) {
            vkUnmapMemory(m_logicalDevice, bufferSlot.deviceMemory);
        }
        if (bufferSlot.buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_logicalDevice, bufferSlot.buffer, nullptr);
        }
        if (bufferSlot.deviceMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_logicalDevice, bufferSlot.deviceMemory, nullptr);
        }
    }
}

} // namespace barrieww
