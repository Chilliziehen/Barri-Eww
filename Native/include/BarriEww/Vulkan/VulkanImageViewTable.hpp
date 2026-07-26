#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

#include "BarriEww/CommandStream/CommandStreamImageViewHandleTableView.hpp"
#include "BarriEww/Vulkan/VulkanImageTable.hpp"
#include "BarriEww/Vulkan/VulkanImageViewTableCreationFailure.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Creation and destruction are single-threaded (load path,
 *       ADR-0003 microkernel whitelist item "装载"). After creation the accessors are
 *       read-only and thread-safe. This blanket statement covers all accessors below.
 * @brief Runtime materialization of one validated ImageViewHandleTable. Hot native
 *        handles form a cache-dense VkImageView array indexed by slot; cold neutral
 *        descriptions are retained separately for later template materialization. It
 *        joins every entry to its source image once on the load path and performs no
 *        work on recording or frame hot paths.
 * @warning MemoryOwnership: OWNS every created VkImageView and destroys it in the
 *          destructor (RAII, §1.4). SHARES ownership of the source VulkanImageTable so
 *          every VkImage necessarily outlives its views and destruction order is
 *          enforced by the type rather than caller convention.
 */
class VulkanImageViewTable {
public:
    /**
     * @note ThreadSafety: Not thread-safe; call on the load path.
     * @brief Preflights every cross-table relationship before allocation or Vulkan calls,
     *        then creates the complete dense image-view table. On driver failure, earlier
     *        views are released through the partial table destructor.
     * @param sourceImageTable Shared owner of the materialized source images; must not be
     *        null.
     * @param tableView The validated ImageViewHandleTable to materialize.
     * @return std::expected<VulkanImageViewTable, VulkanImageViewTableCreationFailure>
     *         The owning dense table on success; the first contextual failure otherwise.
     * @warning MemoryOwnership: tableView is borrowed. The return value owns every
     *          VkImageView and shares sourceImageTable ownership for its entire lifetime.
     */
    [[nodiscard]] static std::expected<VulkanImageViewTable,
                                       VulkanImageViewTableCreationFailure>
    createFromTable(std::shared_ptr<const VulkanImageTable> sourceImageTable,
                    const CommandStreamImageViewHandleTableView& tableView);

    VulkanImageViewTable(const VulkanImageViewTable&) = delete;
    VulkanImageViewTable& operator=(const VulkanImageViewTable&) = delete;

    /** Move transfers ownership of all views and the source image table. */
    VulkanImageViewTable(VulkanImageViewTable&& movedFrom) noexcept
        : m_sourceImageTable(std::move(movedFrom.m_sourceImageTable))
        , m_logicalDevice(movedFrom.m_logicalDevice)
        , m_imageViews(std::move(movedFrom.m_imageViews))
        , m_descriptions(std::move(movedFrom.m_descriptions)) {
        movedFrom.m_imageViews.clear();
        movedFrom.m_descriptions.clear();
    }

    VulkanImageViewTable& operator=(VulkanImageViewTable&&) = delete;

    /**
     * @note ThreadSafety: Not thread-safe; destroy on the load/teardown path only.
     * @brief Destroys every owned VkImageView before releasing source image ownership.
     */
    ~VulkanImageViewTable();

    /** The number of materialized image-view slots (see class notes). */
    [[nodiscard]] std::uint32_t slotCount() const noexcept {
        return static_cast<std::uint32_t>(m_imageViews.size());
    }

    /** The hot native view at slotIndex; precondition slotIndex < slotCount(). */
    [[nodiscard]] VkImageView imageView(std::uint32_t slotIndex) const noexcept {
        return m_imageViews[slotIndex];
    }

    /** The cold neutral description at slotIndex; precondition slotIndex < slotCount(). */
    [[nodiscard]] const CommandStreamImageViewHandleTableEntry&
    description(std::uint32_t slotIndex) const noexcept {
        return m_descriptions[slotIndex];
    }

private:
    VulkanImageViewTable(std::shared_ptr<const VulkanImageTable> sourceImageTable,
                         VkDevice logicalDevice) noexcept
        : m_sourceImageTable(std::move(sourceImageTable)), m_logicalDevice(logicalDevice) {}

    std::shared_ptr<const VulkanImageTable> m_sourceImageTable;
    VkDevice m_logicalDevice;
    std::vector<VkImageView> m_imageViews;
    std::vector<CommandStreamImageViewHandleTableEntry> m_descriptions;
};

} // namespace barrieww
