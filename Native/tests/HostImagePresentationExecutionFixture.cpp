#include "HostImagePresentationExecutionFixture.hpp"

#include <array>
#include <cstring>

#include "BarriEww/Vulkan/Shaders/HostImagePresentationFragmentShader.hpp"
#include "BarriEww/Vulkan/Shaders/HostImagePresentationVertexShader.hpp"

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
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_logicalDevice, m_pipeline, nullptr);
    }
    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_logicalDevice, m_pipelineLayout, nullptr);
    }
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_logicalDevice, m_descriptorPool, nullptr);
    }
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_logicalDevice, m_descriptorSetLayout, nullptr);
    }
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_logicalDevice, m_sampler, nullptr);
    }
    if (m_destinationImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_logicalDevice, m_destinationImageView, nullptr);
    }
    if (m_hostImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_logicalDevice, m_hostImageView, nullptr);
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
    return createImageView(m_hostImage, m_hostImageView)
        && createImageView(m_destinationImage, m_destinationImageView)
        && createDescriptorResources()
        && createPipeline();
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

    const VkRenderingAttachmentInfoKHR colorAttachment{
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR, nullptr,
        m_destinationImageView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_RESOLVE_MODE_NONE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, {}};
    const VkRenderingInfoKHR renderingInfo{
        VK_STRUCTURE_TYPE_RENDERING_INFO_KHR, nullptr, 0u,
        VkRect2D{{0, 0}, m_destinationExtent}, 1u, 0u, 1u,
        &colorAttachment, nullptr, nullptr};
    m_beginRenderingFunction(commandBuffer, &renderingInfo);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipelineLayout, 0u, 1u, &m_descriptorSet, 0u, nullptr);
    vkCmdDraw(commandBuffer, 3u, 1u, 0u, 0u);
    m_endRenderingFunction(commandBuffer);

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

bool HostImagePresentationExecutionFixture::createImageView(
    VkImage image, VkImageView& imageView) {
    const VkImageViewCreateInfo createInfo{
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr, 0u, image,
        VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM,
        VkComponentMapping{VK_COMPONENT_SWIZZLE_IDENTITY,
                           VK_COMPONENT_SWIZZLE_IDENTITY,
                           VK_COMPONENT_SWIZZLE_IDENTITY,
                           VK_COMPONENT_SWIZZLE_IDENTITY},
        g_colorSubresourceRange};
    return vkCreateImageView(m_logicalDevice, &createInfo, nullptr, &imageView)
        == VK_SUCCESS;
}

bool HostImagePresentationExecutionFixture::createDescriptorResources() {
    const VkSamplerCreateInfo samplerCreateInfo{
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, nullptr, 0u,
        VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0.0f, VK_FALSE, 1.0f,
        VK_FALSE, VK_COMPARE_OP_ALWAYS, 0.0f, 0.0f,
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, VK_FALSE};
    if (vkCreateSampler(m_logicalDevice, &samplerCreateInfo, nullptr, &m_sampler)
        != VK_SUCCESS) {
        return false;
    }
    const VkDescriptorSetLayoutBinding layoutBinding{
        0u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u,
        VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    const VkDescriptorSetLayoutCreateInfo layoutCreateInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0u,
        1u, &layoutBinding};
    if (vkCreateDescriptorSetLayout(m_logicalDevice, &layoutCreateInfo, nullptr,
                                    &m_descriptorSetLayout) != VK_SUCCESS) {
        return false;
    }
    const VkDescriptorPoolSize poolSize{
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u};
    const VkDescriptorPoolCreateInfo poolCreateInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr, 0u,
        1u, 1u, &poolSize};
    if (vkCreateDescriptorPool(m_logicalDevice, &poolCreateInfo, nullptr,
                               &m_descriptorPool) != VK_SUCCESS) {
        return false;
    }
    const VkDescriptorSetAllocateInfo allocationInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
        m_descriptorPool, 1u, &m_descriptorSetLayout};
    if (vkAllocateDescriptorSets(m_logicalDevice, &allocationInfo, &m_descriptorSet)
        != VK_SUCCESS) {
        return false;
    }
    const VkDescriptorImageInfo imageInfo{
        m_sampler, m_hostImageView, VK_IMAGE_LAYOUT_GENERAL};
    const VkWriteDescriptorSet descriptorWrite{
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_descriptorSet,
        0u, 0u, 1u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        &imageInfo, nullptr, nullptr};
    vkUpdateDescriptorSets(m_logicalDevice, 1u, &descriptorWrite, 0u, nullptr);
    return true;
}

bool HostImagePresentationExecutionFixture::createPipeline() {
    m_beginRenderingFunction = reinterpret_cast<PFN_vkCmdBeginRenderingKHR>(
        vkGetDeviceProcAddr(m_logicalDevice, "vkCmdBeginRenderingKHR"));
    m_endRenderingFunction = reinterpret_cast<PFN_vkCmdEndRenderingKHR>(
        vkGetDeviceProcAddr(m_logicalDevice, "vkCmdEndRenderingKHR"));
    if (m_beginRenderingFunction == nullptr || m_endRenderingFunction == nullptr) {
        return false;
    }
    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0u,
        1u, &m_descriptorSetLayout, 0u, nullptr};
    if (vkCreatePipelineLayout(m_logicalDevice, &pipelineLayoutCreateInfo, nullptr,
                               &m_pipelineLayout) != VK_SUCCESS) {
        return false;
    }
    const VkShaderModule vertexShaderModule = createShaderModule(
        vulkan::shaders::g_hostImagePresentationVertexShaderBinary.data(),
        vulkan::shaders::g_hostImagePresentationVertexShaderBinary.size());
    const VkShaderModule fragmentShaderModule = createShaderModule(
        vulkan::shaders::g_hostImagePresentationFragmentShaderBinary.data(),
        vulkan::shaders::g_hostImagePresentationFragmentShaderBinary.size());
    if (vertexShaderModule == VK_NULL_HANDLE
        || fragmentShaderModule == VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_logicalDevice, fragmentShaderModule, nullptr);
        vkDestroyShaderModule(m_logicalDevice, vertexShaderModule, nullptr);
        return false;
    }
    const std::array shaderStages{
        VkPipelineShaderStageCreateInfo{
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0u,
            VK_SHADER_STAGE_VERTEX_BIT, vertexShaderModule, "main", nullptr},
        VkPipelineShaderStageCreateInfo{
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0u,
            VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShaderModule, "main", nullptr}};
    const VkPipelineVertexInputStateCreateInfo vertexInputState{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr,
        0u, 0u, nullptr, 0u, nullptr};
    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr,
        0u, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE};
    const VkViewport viewport{0.0f, 0.0f,
                              static_cast<float>(m_destinationExtent.width),
                              static_cast<float>(m_destinationExtent.height),
                              0.0f, 1.0f};
    const VkRect2D scissor{{0, 0}, m_destinationExtent};
    const VkPipelineViewportStateCreateInfo viewportState{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr,
        0u, 1u, &viewport, 1u, &scissor};
    const VkPipelineRasterizationStateCreateInfo rasterizationState{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr,
        0u, VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE,
        VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f};
    const VkPipelineMultisampleStateCreateInfo multisampleState{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr,
        0u, VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 1.0f, nullptr, VK_FALSE, VK_FALSE};
    const VkPipelineColorBlendAttachmentState blendAttachment{
        VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
        VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
    const VkPipelineColorBlendStateCreateInfo blendState{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr,
        0u, VK_FALSE, VK_LOGIC_OP_COPY, 1u, &blendAttachment, {}};
    constexpr VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const VkPipelineRenderingCreateInfoKHR renderingCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR, nullptr, 0u,
        1u, &colorFormat, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED};
    const VkGraphicsPipelineCreateInfo pipelineCreateInfo{
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &renderingCreateInfo,
        0u, static_cast<std::uint32_t>(shaderStages.size()), shaderStages.data(),
        &vertexInputState, &inputAssemblyState, nullptr, &viewportState,
        &rasterizationState, &multisampleState, nullptr, &blendState, nullptr,
        m_pipelineLayout, VK_NULL_HANDLE, 0u, VK_NULL_HANDLE, -1};
    const VkResult vulkanResult = vkCreateGraphicsPipelines(
        m_logicalDevice, VK_NULL_HANDLE, 1u, &pipelineCreateInfo, nullptr,
        &m_pipeline);
    vkDestroyShaderModule(m_logicalDevice, fragmentShaderModule, nullptr);
    vkDestroyShaderModule(m_logicalDevice, vertexShaderModule, nullptr);
    return vulkanResult == VK_SUCCESS;
}

VkShaderModule HostImagePresentationExecutionFixture::createShaderModule(
    const std::byte* shaderBytes, std::size_t shaderByteCount) const {
    const VkShaderModuleCreateInfo createInfo{
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0u,
        shaderByteCount, reinterpret_cast<const std::uint32_t*>(shaderBytes)};
    VkShaderModule shaderModule = VK_NULL_HANDLE;
    return vkCreateShaderModule(m_logicalDevice, &createInfo, nullptr, &shaderModule)
               == VK_SUCCESS
        ? shaderModule
        : VK_NULL_HANDLE;
}

} // namespace barrieww::testing
