#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <vulkan/vulkan.h>

#include "BarriEww/Vulkan/Shaders/HostImagePresentationFragmentShader.hpp"
#include "BarriEww/Vulkan/Shaders/HostImagePresentationVertexShader.hpp"
#include "BarriEww/Vulkan/VulkanContext.hpp"
#include "TestVulkanDeviceHarness.hpp"

using barrieww::VulkanContext;
using barrieww::testing::TestVulkanDeviceHarness;

namespace {

constexpr std::uint32_t g_imageWidth = 4u;
constexpr std::uint32_t g_imageHeight = 4u;
constexpr VkDeviceSize g_imageByteCount = g_imageWidth * g_imageHeight * 4u;
constexpr VkImageSubresourceRange g_colorSubresourceRange{
    VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};

/**
 * @note ThreadSafety: Test-thread confined; every operation and destruction must remain
 *       on the Catch2 thread that owns this fixture.
 * @brief Owns the offscreen Vulkan resources used to execute the production host-image
 *        shaders once. Initialization is transactional from the caller's perspective;
 *        the destructor releases every handle created before any failure.
 * @warning MemoryOwnership: Owns all Vulkan handles stored by the fixture and borrows the
 *          device and physical device from a TestVulkanDeviceHarness that must outlive it.
 */
class HostImagePresentationExecutionFixture {
public:
    /**
     * @note ThreadSafety: Test-thread confined.
     * @brief Captures borrowed device handles without creating Vulkan resources.
     * @param const VulkanContext& vulkanContext Context whose harness outlives this fixture
     * @warning MemoryOwnership: Borrows device handles and acquires no ownership.
     */
    explicit HostImagePresentationExecutionFixture(const VulkanContext& vulkanContext)
        : m_physicalDevice(vulkanContext.physicalDevice()),
          m_logicalDevice(vulkanContext.logicalDevice()) {}

    /**
     * @note ThreadSafety: Copying is unavailable under every threading condition.
     * @brief Prevents duplication of unique test Vulkan resource ownership.
     * @param const HostImagePresentationExecutionFixture& copiedFrom Unavailable source
     * @warning MemoryOwnership: Copies no Vulkan resource ownership.
     */
    HostImagePresentationExecutionFixture(
        const HostImagePresentationExecutionFixture& copiedFrom) = delete;

    /**
     * @note ThreadSafety: Copy assignment is unavailable under every threading condition.
     * @brief Prevents replacement through duplicated test Vulkan resource ownership.
     * @param const HostImagePresentationExecutionFixture& copiedFrom Unavailable source
     * @return HostImagePresentationExecutionFixture& Unavailable destination reference
     * @warning MemoryOwnership: Copies and releases no Vulkan resource ownership.
     */
    HostImagePresentationExecutionFixture& operator=(
        const HostImagePresentationExecutionFixture& copiedFrom) = delete;

    /**
     * @note ThreadSafety: Test-thread confined; no submitted work may remain in flight.
     * @brief Releases all owned Vulkan resources in reverse dependency order.
     * @warning MemoryOwnership: Releases every owned handle and no borrowed device handle.
     */
    ~HostImagePresentationExecutionFixture() {
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

    /**
     * @note ThreadSafety: Test-thread confined and single-use before record().
     * @brief Creates images, transfer buffers, descriptors, and the graphics pipeline.
     * @return bool True when every required Vulkan object was created successfully
     * @warning MemoryOwnership: Acquires all successful Vulkan objects into this fixture.
     */
    [[nodiscard]] bool initialize() {
        if (!createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, m_stagingBuffer,
                          m_stagingMemory, m_stagingMemoryPropertyFlags)) {
            return false;
        }
        if (!createBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT, m_readbackBuffer,
                          m_readbackMemory, m_readbackMemoryPropertyFlags)) {
            return false;
        }
        if (!createImage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                         m_hostImage, m_hostImageMemory)) {
            return false;
        }
        if (!createImage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                             | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                         m_destinationImage, m_destinationImageMemory)) {
            return false;
        }
        if (!createImageView(m_hostImage, m_hostImageView)) {
            return false;
        }
        if (!createImageView(m_destinationImage, m_destinationImageView)) {
            return false;
        }
        if (!createDescriptorResources()) {
            return false;
        }
        if (!createPipeline()) {
            return false;
        }
        return true;
    }

    /**
     * @note ThreadSafety: Test-thread confined; the borrowed command buffer is recording.
     * @brief Uploads the exact host bytes, renders one fullscreen triangle, and copies the
     *        destination image into the readback buffer with complete Vulkan barriers.
     * @param VkCommandBuffer commandBuffer Fresh primary command buffer from the harness
     * @param const std::array<std::byte, g_imageByteCount>& hostBytes Source RGBA8 texels
     * @return bool True when upload, recording, and mapped-memory operations succeed
     * @warning MemoryOwnership: Borrows commandBuffer and hostBytes; owned resources remain
     *          attached to this fixture.
     */
    [[nodiscard]] bool record(
        VkCommandBuffer commandBuffer,
        const std::array<std::byte, g_imageByteCount>& hostBytes) {
        void* mappedBytes = nullptr;
        if (vkMapMemory(m_logicalDevice, m_stagingMemory, 0u, VK_WHOLE_SIZE, 0u,
                        &mappedBytes) != VK_SUCCESS) {
            return false;
        }
        std::memcpy(mappedBytes, hostBytes.data(), hostBytes.size());
        const bool stagingIsCoherent =
            (m_stagingMemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0u;
        VkResult vulkanResult = VK_SUCCESS;
        if (!stagingIsCoherent) {
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
            VkOffset3D{0, 0, 0}, VkExtent3D{g_imageWidth, g_imageHeight, 1u}};
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
            VkRect2D{{0, 0}, {g_imageWidth, g_imageHeight}}, 1u, 0u, 1u,
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
        vkCmdCopyImageToBuffer(commandBuffer, m_destinationImage,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_readbackBuffer,
                               1u, &uploadRegion);
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

    /**
     * @note ThreadSafety: Test-thread confined after queue completion.
     * @brief Invalidates non-coherent memory when needed and copies all readback bytes.
     * @param std::array<std::byte, g_imageByteCount>& readbackBytes Receives RGBA8 contents
     * @return bool True when mapping and any required invalidation succeed
     * @warning MemoryOwnership: Writes a value copy and retains Vulkan memory ownership.
     */
    [[nodiscard]] bool readback(
        std::array<std::byte, g_imageByteCount>& readbackBytes) const {
        void* mappedBytes = nullptr;
        if (vkMapMemory(m_logicalDevice, m_readbackMemory, 0u, VK_WHOLE_SIZE, 0u,
                        &mappedBytes) != VK_SUCCESS) {
            return false;
        }
        const bool readbackIsCoherent =
            (m_readbackMemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0u;
        if (!readbackIsCoherent) {
            const VkMappedMemoryRange mappedMemoryRange{
                VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, m_readbackMemory,
                0u, VK_WHOLE_SIZE};
            if (vkInvalidateMappedMemoryRanges(m_logicalDevice, 1u,
                                               &mappedMemoryRange) != VK_SUCCESS) {
                vkUnmapMemory(m_logicalDevice, m_readbackMemory);
                return false;
            }
        }
        std::memcpy(readbackBytes.data(), mappedBytes, readbackBytes.size());
        vkUnmapMemory(m_logicalDevice, m_readbackMemory);
        return true;
    }

private:
    /**
     * @note ThreadSafety: Test-thread confined and read-only over physical-device state.
     * @brief Selects the first compatible memory type containing every required property.
     * @param std::uint32_t memoryTypeBits Compatible types reported by Vulkan
     * @param VkMemoryPropertyFlags requiredProperties Required property bit set
     * @param std::uint32_t& memoryTypeIndex Receives the selected type index
     * @param VkMemoryPropertyFlags& selectedProperties Receives all selected properties
     * @return bool True when a compatible memory type exists
     * @warning MemoryOwnership: Reads driver properties and transfers no memory ownership.
     */
    [[nodiscard]] bool findMemoryType(
        std::uint32_t memoryTypeBits, VkMemoryPropertyFlags requiredProperties,
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

    /**
     * @note ThreadSafety: Test-thread confined during fixture initialization.
     * @brief Creates and binds one image with the fixed 4x4 RGBA8 test shape.
     * @param VkImageUsageFlags usageFlags Required image uses
     * @param VkImage& image Receives the owned image handle
     * @param VkDeviceMemory& imageMemory Receives the owned memory handle
     * @return bool True when image creation, allocation, and binding succeed
     * @warning MemoryOwnership: Transfers successful image and memory handles to fields.
     */
    [[nodiscard]] bool createImage(VkImageUsageFlags usageFlags, VkImage& image,
                                   VkDeviceMemory& imageMemory) {
        const VkImageCreateInfo imageCreateInfo{
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, nullptr, 0u, VK_IMAGE_TYPE_2D,
            VK_FORMAT_R8G8B8A8_UNORM, VkExtent3D{g_imageWidth, g_imageHeight, 1u},
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

    /**
     * @note ThreadSafety: Test-thread confined during fixture initialization.
     * @brief Creates and binds one host-visible fixed-size transfer buffer.
     * @param VkBufferUsageFlags usageFlags Transfer direction required by the buffer
     * @param VkBuffer& buffer Receives the owned buffer handle
     * @param VkDeviceMemory& bufferMemory Receives the owned memory handle
     * @param VkMemoryPropertyFlags& memoryPropertyFlags Receives selected memory properties
     * @return bool True when buffer creation, allocation, and binding succeed
     * @warning MemoryOwnership: Transfers successful buffer and memory handles to fields.
     */
    [[nodiscard]] bool createBuffer(
        VkBufferUsageFlags usageFlags, VkBuffer& buffer, VkDeviceMemory& bufferMemory,
        VkMemoryPropertyFlags& memoryPropertyFlags) {
        const VkBufferCreateInfo bufferCreateInfo{
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0u, g_imageByteCount,
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

    /**
     * @note ThreadSafety: Test-thread confined during fixture initialization.
     * @brief Creates one identity-swizzled color view for a fixed RGBA8 image.
     * @param VkImage image Borrowed image receiving the view
     * @param VkImageView& imageView Receives the owned image-view handle
     * @return bool True when the image view was created
     * @warning MemoryOwnership: Transfers the successful image-view handle to a field.
     */
    [[nodiscard]] bool createImageView(VkImage image, VkImageView& imageView) {
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

    /**
     * @note ThreadSafety: Test-thread confined during fixture initialization.
     * @brief Creates the production-equivalent nearest/clamp descriptor resources.
     * @return bool True when sampler, layout, pool, set, and descriptor write succeed
     * @warning MemoryOwnership: Acquires all created descriptor resources into this fixture.
     */
    [[nodiscard]] bool createDescriptorResources() {
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

    /**
     * @note ThreadSafety: Test-thread confined during fixture initialization.
     * @brief Creates the fixed fullscreen pipeline from the exact generated production
     *        shader byte arrays and resolves KHR dynamic-rendering entry points.
     * @return bool True when entry points, shader modules, layout, and pipeline exist
     * @warning MemoryOwnership: Owns the pipeline and layout; temporary shader modules are
     *          destroyed before return, while generated bytes remain static storage.
     */
    [[nodiscard]] bool createPipeline() {
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
        VkShaderModule vertexShaderModule = createShaderModule(
            barrieww::vulkan::shaders::g_hostImagePresentationVertexShaderBinary.data(),
            barrieww::vulkan::shaders::g_hostImagePresentationVertexShaderBinary.size());
        VkShaderModule fragmentShaderModule = createShaderModule(
            barrieww::vulkan::shaders::g_hostImagePresentationFragmentShaderBinary.data(),
            barrieww::vulkan::shaders::g_hostImagePresentationFragmentShaderBinary.size());
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
        const VkViewport viewport{0.0f, 0.0f, static_cast<float>(g_imageWidth),
                                  static_cast<float>(g_imageHeight), 0.0f, 1.0f};
        const VkRect2D scissor{{0, 0}, {g_imageWidth, g_imageHeight}};
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

    /**
     * @note ThreadSafety: Test-thread confined during pipeline initialization.
     * @brief Creates one shader module from an aligned generated byte array.
     * @param const std::byte* shaderBytes Aligned SPIR-V storage
     * @param std::size_t shaderByteCount SPIR-V byte count divisible by four
     * @return VkShaderModule Owned module handle, or VK_NULL_HANDLE on failure
     * @warning MemoryOwnership: Returns an owned module; shaderBytes remains borrowed.
     */
    [[nodiscard]] VkShaderModule createShaderModule(
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

    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_logicalDevice = VK_NULL_HANDLE;
    VkBuffer m_stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_stagingMemory = VK_NULL_HANDLE;
    VkMemoryPropertyFlags m_stagingMemoryPropertyFlags = 0u;
    VkBuffer m_readbackBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_readbackMemory = VK_NULL_HANDLE;
    VkMemoryPropertyFlags m_readbackMemoryPropertyFlags = 0u;
    VkImage m_hostImage = VK_NULL_HANDLE;
    VkDeviceMemory m_hostImageMemory = VK_NULL_HANDLE;
    VkImage m_destinationImage = VK_NULL_HANDLE;
    VkDeviceMemory m_destinationImageMemory = VK_NULL_HANDLE;
    VkImageView m_hostImageView = VK_NULL_HANDLE;
    VkImageView m_destinationImageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    PFN_vkCmdBeginRenderingKHR m_beginRenderingFunction = nullptr;
    PFN_vkCmdEndRenderingKHR m_endRenderingFunction = nullptr;
};

} // namespace

TEST_CASE("Host image presentation reproduces Minecraft image pixels",
          "[hostImagePresentationExecution][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan graphics driver on this machine");
    }
    if (!harness->supportsDynamicRendering()) {
        SKIP("driver does not support KHR dynamic rendering");
    }
    if (!harness->supportsDynamicRenderingExtensionEntryPoints()) {
        SKIP("driver does not expose KHR dynamic-rendering entry points");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};
    HostImagePresentationExecutionFixture fixture{vulkanContext};
    REQUIRE(fixture.initialize());

    // Row zero is Minecraft's lower edge but Vulkan's positive-height destination upper
    // edge. Identical byte rows therefore encode the required presentation Y inversion.
    constexpr std::array<std::byte, g_imageByteCount> hostBytes{
        std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255},
        std::byte{17}, std::byte{31}, std::byte{47}, std::byte{251},
        std::byte{67}, std::byte{83}, std::byte{101}, std::byte{239},
        std::byte{0}, std::byte{255}, std::byte{0}, std::byte{255},
        std::byte{13}, std::byte{29}, std::byte{43}, std::byte{227},
        std::byte{59}, std::byte{71}, std::byte{89}, std::byte{223},
        std::byte{107}, std::byte{127}, std::byte{149}, std::byte{211},
        std::byte{163}, std::byte{181}, std::byte{199}, std::byte{197},
        std::byte{19}, std::byte{37}, std::byte{53}, std::byte{193},
        std::byte{73}, std::byte{97}, std::byte{109}, std::byte{191},
        std::byte{131}, std::byte{151}, std::byte{173}, std::byte{179},
        std::byte{191}, std::byte{211}, std::byte{229}, std::byte{167},
        std::byte{0}, std::byte{0}, std::byte{255}, std::byte{255},
        std::byte{23}, std::byte{61}, std::byte{113}, std::byte{157},
        std::byte{137}, std::byte{79}, std::byte{41}, std::byte{149},
        std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}};
    constexpr std::array<std::byte, g_imageByteCount> expectedBytes{
        std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255},
        std::byte{17}, std::byte{31}, std::byte{47}, std::byte{251},
        std::byte{67}, std::byte{83}, std::byte{101}, std::byte{239},
        std::byte{0}, std::byte{255}, std::byte{0}, std::byte{255},
        std::byte{13}, std::byte{29}, std::byte{43}, std::byte{227},
        std::byte{59}, std::byte{71}, std::byte{89}, std::byte{223},
        std::byte{107}, std::byte{127}, std::byte{149}, std::byte{211},
        std::byte{163}, std::byte{181}, std::byte{199}, std::byte{197},
        std::byte{19}, std::byte{37}, std::byte{53}, std::byte{193},
        std::byte{73}, std::byte{97}, std::byte{109}, std::byte{191},
        std::byte{131}, std::byte{151}, std::byte{173}, std::byte{179},
        std::byte{191}, std::byte{211}, std::byte{229}, std::byte{167},
        std::byte{0}, std::byte{0}, std::byte{255}, std::byte{255},
        std::byte{23}, std::byte{61}, std::byte{113}, std::byte{157},
        std::byte{137}, std::byte{79}, std::byte{41}, std::byte{149},
        std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}};

    const VkCommandBuffer commandBuffer = harness->allocateCommandBuffer();
    REQUIRE(commandBuffer != VK_NULL_HANDLE);
    REQUIRE(fixture.record(commandBuffer, hostBytes));
    REQUIRE(harness->submitAndWait(commandBuffer));
    std::array<std::byte, g_imageByteCount> readbackBytes{};
    REQUIRE(fixture.readback(readbackBytes));
    REQUIRE(readbackBytes == expectedBytes);
}
