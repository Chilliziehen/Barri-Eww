#include "HostImagePresentationExecutionFixture.hpp"

#include <array>
#include <cstring>
#include <utility>

namespace barrieww::testing {

namespace {

constexpr VkImageSubresourceRange g_colorSubresourceRange{
    VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};

} // namespace

HostImagePresentationExecutionFixture::HostImagePresentationExecutionFixture(
    const VulkanContext& vulkanContext, VkExtent2D destinationExtent)
    : m_physicalDevice(vulkanContext.physicalDevice()),
      m_logicalDevice(vulkanContext.logicalDevice()),
      m_destinationExtent(destinationExtent),
      m_destinationImageByteCount(
          static_cast<VkDeviceSize>(destinationExtent.width)
          * destinationExtent.height * 4u) {}

HostImagePresentationExecutionFixture::~HostImagePresentationExecutionFixture() {
    m_compositionResources.reset();
    if (m_destinationImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_logicalDevice, m_destinationImageView, nullptr);
    }
    if (m_destinationImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_logicalDevice, m_destinationImage, nullptr);
    }
    if (m_destinationImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_logicalDevice, m_destinationImageMemory, nullptr);
    }
    if (m_hostImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_logicalDevice, m_hostImage, nullptr);
    }
    if (m_hostImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_logicalDevice, m_hostImageMemory, nullptr);
    }
    if (m_readbackBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_logicalDevice, m_readbackBuffer, nullptr);
    }
    if (m_readbackMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_logicalDevice, m_readbackMemory, nullptr);
    }
    if (m_stagingBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_logicalDevice, m_stagingBuffer, nullptr);
    }
    if (m_stagingMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_logicalDevice, m_stagingMemory, nullptr);
    }
}

bool HostImagePresentationExecutionFixture::initialize() {
    if (m_destinationExtent.width == 0u || m_destinationExtent.height == 0u) {
        return false;
    }
    if (!createBuffer(s_hostImageByteCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      m_stagingBuffer, m_stagingMemory,
                      m_stagingMemoryPropertyFlags)) {
        return false;
    }
    if (!createBuffer(m_destinationImageByteCount,
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT, m_readbackBuffer,
                      m_readbackMemory, m_readbackMemoryPropertyFlags)) {
        return false;
    }
    if (!createImage(VkExtent2D{s_hostImageWidth, s_hostImageHeight},
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     m_hostImage, m_hostImageMemory)) {
        return false;
    }
    if (!createImage(m_destinationExtent,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                         | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                     m_destinationImage, m_destinationImageMemory)) {
        return false;
    }
    if (!createDestinationImageView()) {
        return false;
    }
    auto compositionResources = VulkanHostImageCompositionResources::create({
        m_logicalDevice,
        m_hostImage,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
        m_destinationExtent,
    });
    if (!compositionResources.has_value()) {
        return false;
    }
    m_compositionResources.emplace(std::move(compositionResources.value()));
    return true;
}

bool HostImagePresentationExecutionFixture::record(
    VkCommandBuffer commandBuffer,
    const std::array<std::byte, s_hostImageByteCount>& hostBytes) {
    void* mappedBytes = nullptr;
    if (vkMapMemory(m_logicalDevice, m_stagingMemory, 0u, VK_WHOLE_SIZE, 0u,
                    &mappedBytes) != VK_SUCCESS) {
        return false;
    }
    std::memcpy(mappedBytes, hostBytes.data(), hostBytes.size());
    VkResult vulkanResult = VK_SUCCESS;
    if ((m_stagingMemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0u) {
        const VkMappedMemoryRange mappedMemoryRange{
            VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, m_stagingMemory,
            0u, VK_WHOLE_SIZE};
        vulkanResult = vkFlushMappedMemoryRanges(m_logicalDevice, 1u,
                                                 &mappedMemoryRange);
    }
    vkUnmapMemory(m_logicalDevice, m_stagingMemory);
    if (vulkanResult != VK_SUCCESS) {
        return false;
    }

    const VkCommandBufferBeginInfo beginInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr};
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        return false;
    }

    const std::array initialBarriers{
        VkImageMemoryBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, 0u,
                             VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                             m_hostImage, g_colorSubresourceRange},
        VkImageMemoryBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, 0u,
                             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                             m_destinationImage, g_colorSubresourceRange},
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT
                             | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         0u, 0u, nullptr, 0u, nullptr,
                         static_cast<std::uint32_t>(initialBarriers.size()),
                         initialBarriers.data());

    const VkBufferImageCopy uploadRegion{
        0u, 0u, 0u,
        VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u},
        VkOffset3D{0, 0, 0},
        VkExtent3D{s_hostImageWidth, s_hostImageHeight, 1u}};
    vkCmdCopyBufferToImage(commandBuffer, m_stagingBuffer, m_hostImage,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &uploadRegion);

    const VkImageMemoryBarrier samplingBarrier{
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, m_hostImage,
        g_colorSubresourceRange};
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 0u, nullptr,
                         0u, nullptr, 1u, &samplingBarrier);

    m_compositionResources->recordCommands(commandBuffer, m_destinationImageView);

    const VkImageMemoryBarrier readbackBarrier{
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED, m_destinationImage, g_colorSubresourceRange};
    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u,
                         nullptr, 1u, &readbackBarrier);
    const VkBufferImageCopy readbackRegion{
        0u, 0u, 0u,
        VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u},
        VkOffset3D{0, 0, 0},
        VkExtent3D{m_destinationExtent.width, m_destinationExtent.height, 1u}};
    vkCmdCopyImageToBuffer(commandBuffer, m_destinationImage,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_readbackBuffer,
                           1u, &readbackRegion);
    const VkBufferMemoryBarrier hostReadBarrier{
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, m_readbackBuffer,
        0u, VK_WHOLE_SIZE};
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 1u,
                         &hostReadBarrier, 0u, nullptr);
    return vkEndCommandBuffer(commandBuffer) == VK_SUCCESS;
}

bool HostImagePresentationExecutionFixture::readback(
    std::vector<std::byte>& readbackBytes) const {
    void* mappedBytes = nullptr;
    if (vkMapMemory(m_logicalDevice, m_readbackMemory, 0u, VK_WHOLE_SIZE, 0u,
                    &mappedBytes) != VK_SUCCESS) {
        return false;
    }
    if ((m_readbackMemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0u) {
        const VkMappedMemoryRange mappedMemoryRange{
            VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, m_readbackMemory,
            0u, VK_WHOLE_SIZE};
        if (vkInvalidateMappedMemoryRanges(m_logicalDevice, 1u,
                                           &mappedMemoryRange) != VK_SUCCESS) {
            vkUnmapMemory(m_logicalDevice, m_readbackMemory);
            return false;
        }
    }
    readbackBytes.resize(static_cast<std::size_t>(m_destinationImageByteCount));
    std::memcpy(readbackBytes.data(), mappedBytes, readbackBytes.size());
    vkUnmapMemory(m_logicalDevice, m_readbackMemory);
    return true;
}

bool HostImagePresentationExecutionFixture::findMemoryType(
    std::uint32_t memoryTypeBits,
    VkMemoryPropertyFlags requiredProperties,
    std::uint32_t& memoryTypeIndex,
    VkMemoryPropertyFlags& selectedProperties) const {
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memoryProperties);
    for (std::uint32_t candidateIndex = 0u;
         candidateIndex < memoryProperties.memoryTypeCount; ++candidateIndex) {
        const VkMemoryPropertyFlags candidateProperties =
            memoryProperties.memoryTypes[candidateIndex].propertyFlags;
        if ((memoryTypeBits & (1u << candidateIndex)) != 0u
            && (candidateProperties & requiredProperties) == requiredProperties) {
            memoryTypeIndex = candidateIndex;
            selectedProperties = candidateProperties;
            return true;
        }
    }
    return false;
}

bool HostImagePresentationExecutionFixture::createImage(
    VkExtent2D imageExtent, VkImageUsageFlags usageFlags, VkImage& image,
    VkDeviceMemory& imageMemory) {
    const VkImageCreateInfo imageCreateInfo{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, nullptr, 0u, VK_IMAGE_TYPE_2D,
        VK_FORMAT_R8G8B8A8_UNORM, VkExtent3D{imageExtent.width, imageExtent.height, 1u},
        1u, 1u, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, usageFlags,
        VK_SHARING_MODE_EXCLUSIVE, 0u, nullptr, VK_IMAGE_LAYOUT_UNDEFINED};
    if (vkCreateImage(m_logicalDevice, &imageCreateInfo, nullptr, &image)
        != VK_SUCCESS) {
        return false;
    }
    VkMemoryRequirements memoryRequirements{};
    vkGetImageMemoryRequirements(m_logicalDevice, image, &memoryRequirements);
    std::uint32_t memoryTypeIndex = 0u;
    VkMemoryPropertyFlags selectedProperties = 0u;
    if (!findMemoryType(memoryRequirements.memoryTypeBits,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryTypeIndex,
                        selectedProperties)) {
        return false;
    }
    const VkMemoryAllocateInfo allocationInfo{
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr,
        memoryRequirements.size, memoryTypeIndex};
    return vkAllocateMemory(m_logicalDevice, &allocationInfo, nullptr, &imageMemory)
               == VK_SUCCESS
        && vkBindImageMemory(m_logicalDevice, image, imageMemory, 0u) == VK_SUCCESS;
}

bool HostImagePresentationExecutionFixture::createBuffer(
    VkDeviceSize bufferByteCount, VkBufferUsageFlags usageFlags, VkBuffer& buffer,
    VkDeviceMemory& bufferMemory, VkMemoryPropertyFlags& memoryPropertyFlags) {
    const VkBufferCreateInfo bufferCreateInfo{
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0u, bufferByteCount,
        usageFlags, VK_SHARING_MODE_EXCLUSIVE, 0u, nullptr};
    if (vkCreateBuffer(m_logicalDevice, &bufferCreateInfo, nullptr, &buffer)
        != VK_SUCCESS) {
        return false;
    }
    VkMemoryRequirements memoryRequirements{};
    vkGetBufferMemoryRequirements(m_logicalDevice, buffer, &memoryRequirements);
    std::uint32_t memoryTypeIndex = 0u;
    if (!findMemoryType(memoryRequirements.memoryTypeBits,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, memoryTypeIndex,
                        memoryPropertyFlags)) {
        return false;
    }
    const VkMemoryAllocateInfo allocationInfo{
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr,
        memoryRequirements.size, memoryTypeIndex};
    return vkAllocateMemory(m_logicalDevice, &allocationInfo, nullptr, &bufferMemory)
               == VK_SUCCESS
        && vkBindBufferMemory(m_logicalDevice, buffer, bufferMemory, 0u) == VK_SUCCESS;
}

bool HostImagePresentationExecutionFixture::createDestinationImageView() {
    const VkImageViewCreateInfo createInfo{
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr, 0u, m_destinationImage,
        VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM,
        VkComponentMapping{VK_COMPONENT_SWIZZLE_IDENTITY,
                           VK_COMPONENT_SWIZZLE_IDENTITY,
                           VK_COMPONENT_SWIZZLE_IDENTITY,
                           VK_COMPONENT_SWIZZLE_IDENTITY},
        g_colorSubresourceRange};
    return vkCreateImageView(m_logicalDevice, &createInfo, nullptr,
                             &m_destinationImageView) == VK_SUCCESS;
}

} // namespace barrieww::testing
