#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <vulkan/vulkan.h>

#include "BarriEww/Vulkan/VulkanContext.hpp"
#include "BarriEww/Vulkan/VulkanHostImageCompositionResources.hpp"

namespace barrieww::testing {

/**
 * @note ThreadSafety: Test-thread confined; every operation and destruction must remain
 *       on the thread that owns this fixture.
 * @brief Owns offscreen Vulkan resources that execute the generated production host-image
 *        shaders against a fixed 4x4 source and a caller-selected destination extent.
 * @warning MemoryOwnership: Owns every stored Vulkan object and borrows the physical and
 *          logical devices from a TestVulkanDeviceHarness that must outlive it.
 */
class HostImagePresentationExecutionFixture {
public:
    static constexpr std::uint32_t s_hostImageWidth = 4u;
    static constexpr std::uint32_t s_hostImageHeight = 4u;
    static constexpr std::size_t s_hostImageByteCount =
        s_hostImageWidth * s_hostImageHeight * 4u;

    /**
     * @note ThreadSafety: Test-thread confined.
     * @brief Captures borrowed device handles and the positive destination extent.
     * @param const VulkanContext& vulkanContext Context whose harness outlives this fixture
     * @param VkExtent2D destinationExtent Destination dimensions used for render and readback
     * @warning MemoryOwnership: Borrows device handles and acquires no Vulkan object yet.
     */
    HostImagePresentationExecutionFixture(const VulkanContext& vulkanContext,
                                          VkExtent2D destinationExtent);

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
     * @warning MemoryOwnership: Releases every owned object and no borrowed device.
     */
    ~HostImagePresentationExecutionFixture();

    /**
     * @note ThreadSafety: Test-thread confined and single-use before record().
     * @brief Creates transfer resources, images, output view, and production composition.
     * @return bool True when every required Vulkan object was created successfully
     * @warning MemoryOwnership: Acquires all successful Vulkan objects into this fixture.
     */
    [[nodiscard]] bool initialize();

    /**
     * @note ThreadSafety: Test-thread confined; the borrowed command buffer is recording.
     * @brief Uploads the exact host bytes, renders one fullscreen triangle, and records
     *        destination readback with complete Vulkan barriers.
     * @param VkCommandBuffer commandBuffer Fresh primary command buffer from the harness
     * @param const std::array<std::byte, s_hostImageByteCount>& hostBytes Source RGBA8 texels
     * @return bool True when upload, recording, and mapped-memory operations succeed
     * @warning MemoryOwnership: Borrows commandBuffer and hostBytes; resources remain owned.
     */
    [[nodiscard]] bool record(
        VkCommandBuffer commandBuffer,
        const std::array<std::byte, s_hostImageByteCount>& hostBytes);

    /**
     * @note ThreadSafety: Test-thread confined after queue completion.
     * @brief Invalidates non-coherent memory when needed and copies all destination bytes.
     * @param std::vector<std::byte>& readbackBytes Receives tightly packed RGBA8 contents
     * @return bool True when mapping and any required invalidation succeed
     * @warning MemoryOwnership: Writes a value copy and retains Vulkan memory ownership.
     */
    [[nodiscard]] bool readback(std::vector<std::byte>& readbackBytes) const;

private:
    /**
     * @note ThreadSafety: Test-thread confined and read-only over physical-device state.
     * @brief Selects the first compatible memory type containing every required property.
     * @param std::uint32_t memoryTypeBits Compatible types reported by Vulkan
     * @param VkMemoryPropertyFlags requiredProperties Required property bit set
     * @param std::uint32_t& memoryTypeIndex Receives the selected type index
     * @param VkMemoryPropertyFlags& selectedProperties Receives all selected properties
     * @return bool True when a compatible memory type exists
     * @warning MemoryOwnership: Reads driver properties and transfers no ownership.
     */
    [[nodiscard]] bool findMemoryType(
        std::uint32_t memoryTypeBits,
        VkMemoryPropertyFlags requiredProperties,
        std::uint32_t& memoryTypeIndex,
        VkMemoryPropertyFlags& selectedProperties) const;

    /**
     * @note ThreadSafety: Test-thread confined during fixture initialization.
     * @brief Creates and binds one RGBA8 image with a specified positive extent.
     * @param VkExtent2D imageExtent Image dimensions
     * @param VkImageUsageFlags usageFlags Required image uses
     * @param VkImage& image Receives the owned image handle
     * @param VkDeviceMemory& imageMemory Receives the owned memory handle
     * @return bool True when image creation, allocation, and binding succeed
     * @warning MemoryOwnership: Transfers successful image and memory handles to fields.
     */
    [[nodiscard]] bool createImage(VkExtent2D imageExtent,
                                   VkImageUsageFlags usageFlags,
                                   VkImage& image,
                                   VkDeviceMemory& imageMemory);

    /**
     * @note ThreadSafety: Test-thread confined during fixture initialization.
     * @brief Creates and binds one host-visible transfer buffer of the requested size.
     * @param VkDeviceSize bufferByteCount Required allocation-visible buffer size
     * @param VkBufferUsageFlags usageFlags Transfer direction required by the buffer
     * @param VkBuffer& buffer Receives the owned buffer handle
     * @param VkDeviceMemory& bufferMemory Receives the owned memory handle
     * @param VkMemoryPropertyFlags& memoryPropertyFlags Receives selected properties
     * @return bool True when buffer creation, allocation, and binding succeed
     * @warning MemoryOwnership: Transfers successful buffer and memory handles to fields.
     */
    [[nodiscard]] bool createBuffer(
        VkDeviceSize bufferByteCount,
        VkBufferUsageFlags usageFlags,
        VkBuffer& buffer,
        VkDeviceMemory& bufferMemory,
        VkMemoryPropertyFlags& memoryPropertyFlags);

    /**
     * @note ThreadSafety: Test-thread confined during fixture initialization.
     * @brief Creates the destination color-attachment view used by composition and readback.
     * @return bool True when the destination image view was created
     * @warning MemoryOwnership: Acquires the created view into this fixture.
     */
    [[nodiscard]] bool createDestinationImageView();

    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_logicalDevice = VK_NULL_HANDLE;
    VkExtent2D m_destinationExtent{};
    VkDeviceSize m_destinationImageByteCount = 0u;
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
    VkImageView m_destinationImageView = VK_NULL_HANDLE;
    std::optional<VulkanHostImageCompositionResources> m_compositionResources;
};

} // namespace barrieww::testing
