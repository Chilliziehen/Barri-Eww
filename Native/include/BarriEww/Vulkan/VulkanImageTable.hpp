#pragma once

#include <cstdint>
#include <expected>
#include <vector>

#include <vulkan/vulkan.h>

#include "BarriEww/CommandStream/CommandStreamImageHandleTableView.hpp"
#include "BarriEww/Vulkan/VulkanContext.hpp"
#include "BarriEww/Vulkan/VulkanImageTableCreationFailure.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Creation and destruction are single-threaded (load path, ADR-0003
 *       microkernel whitelist item "装载"). After creation the accessors are read-only
 *       and thread-safe.
 * @brief Runtime materialization of one validated ImageHandleTable: a dense array of
 *        native images indexed by slot (ADR-0002 D2 slot indirection). CREATED entries
 *        become device-local, optimally tiled images with initial layout Undefined —
 *        the compile-time barrier pass is responsible for the first transition.
 *        IMPORTED entries (Minecraft swapchain images and similar) occupy their slot as
 *        UNBOUND placeholders in v0.1; the host-binding mechanism is a later increment.
 *        Each slot keeps its neutral description so the recorder can run load-time
 *        checks (extents, formats, usage) next to the stream (ADR-0002 D5).
 * @warning MemoryOwnership: OWNS every created VkImage / VkDeviceMemory and destroys
 *          them in the destructor (RAII, §1.4). BORROWS the VulkanContext's device,
 *          which must outlive this table.
 */
class VulkanImageTable {
public:
    /**
     * @note ThreadSafety: Plain value type; see class note.
     * @brief One materialized slot: the native handles plus the entry's neutral
     *        description (kept for recorder-side load-time checks).
     */
    struct ImageSlot {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory deviceMemory = VK_NULL_HANDLE;
        CommandStreamImageHandleTableEntry description{};
        bool isBound = false;
    };

    /**
     * @note ThreadSafety: Not thread-safe; call on the load path.
     * @brief Materializes every entry of a validated table: creates images, allocates
     *        device-local memory and binds. On failure everything already created is
     *        released (strong guarantee at table granularity).
     * @param vulkanContext The borrowed device context to create against.
     * @param tableView The validated ImageHandleTable to materialize.
     * @return std::expected<VulkanImageTable, VulkanImageTableCreationFailure> The
     *         owning table on success; the first failure (category, slot, VkResult)
     *         otherwise. Errors are values (§6.4).
     * @warning MemoryOwnership: The returned table owns all created objects; see the
     *          class note. vulkanContext is borrowed and must outlive the table.
     */
    [[nodiscard]] static std::expected<VulkanImageTable, VulkanImageTableCreationFailure>
    createFromTable(const VulkanContext& vulkanContext,
                    const CommandStreamImageHandleTableView& tableView);

    VulkanImageTable(const VulkanImageTable&) = delete;
    VulkanImageTable& operator=(const VulkanImageTable&) = delete;

    /** Move transfers ownership of all slots (source becomes empty). */
    VulkanImageTable(VulkanImageTable&& movedFrom) noexcept
        : m_logicalDevice(movedFrom.m_logicalDevice)
        , m_imageSlots(std::move(movedFrom.m_imageSlots)) {
        movedFrom.m_imageSlots.clear();
    }

    VulkanImageTable& operator=(VulkanImageTable&&) = delete;

    /**
     * @note ThreadSafety: Not thread-safe; destroy on the load/teardown path only.
     * @brief Destroys and frees every created slot (RAII).
     */
    ~VulkanImageTable();

    /** The number of slots (equal to the table's entryCount; see class notes). */
    [[nodiscard]] std::uint32_t slotCount() const noexcept {
        return static_cast<std::uint32_t>(m_imageSlots.size());
    }

    /** The materialized slot at slotIndex; precondition slotIndex < slotCount(). */
    [[nodiscard]] const ImageSlot& slot(std::uint32_t slotIndex) const noexcept {
        return m_imageSlots[slotIndex];
    }

private:
    VulkanImageTable(VkDevice logicalDevice, std::vector<ImageSlot> imageSlots) noexcept
        : m_logicalDevice(logicalDevice), m_imageSlots(std::move(imageSlots)) {}

    VkDevice m_logicalDevice;
    std::vector<ImageSlot> m_imageSlots;
};

} // namespace barrieww
