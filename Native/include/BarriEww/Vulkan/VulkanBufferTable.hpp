#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <vector>

#include <vulkan/vulkan.h>

#include "BarriEww/CommandStream/CommandStreamBufferHandleTableView.hpp"
#include "BarriEww/Vulkan/VulkanBufferTableCreationFailure.hpp"
#include "BarriEww/Vulkan/VulkanContext.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Creation and destruction are single-threaded (load path, ADR-0003
 *       microkernel whitelist item "装载"). After creation the accessors are read-only
 *       and thread-safe; writing through a mapped pointer is the caller's data race to
 *       manage (one lane/writer per buffer, ADR-0001).
 * @brief Runtime materialization of one validated BufferHandleTable: a dense array of
 *        native buffers indexed by slot (ADR-0002 D2 slot indirection). CREATED entries
 *        are created and bound here; host-visible kinds are persistently mapped
 *        (ADR-0001). IMPORTED entries occupy their slot as UNBOUND placeholders in
 *        v0.1 — the host-binding mechanism is a later increment, and the recorder
 *        rejects commands referencing unbound slots.
 * @warning MemoryOwnership: OWNS every created VkBuffer / VkDeviceMemory and destroys
 *          them in the destructor (RAII, §1.4). BORROWS the VulkanContext's device,
 *          which must outlive this table. Mapped pointers are owned by the table and
 *          die with it.
 */
class VulkanBufferTable {
public:
    /**
     * @note ThreadSafety: Plain value type; see class note for mapped-pointer writes.
     * @brief One materialized slot. isBound is false only for v0.1 imported
     *        placeholders. mappedPointer is non-null exactly for host-visible kinds.
     */
    struct BufferSlot {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory deviceMemory = VK_NULL_HANDLE;
        std::byte* mappedPointer = nullptr;
        std::uint64_t byteSize = 0;
        bool isBound = false;
    };

    /**
     * @note ThreadSafety: Not thread-safe; call on the load path.
     * @brief Materializes every entry of a validated table: creates buffers, selects
     *        memory types per the entry's memory kind, binds, and persistently maps
     *        host-visible kinds. On failure everything already created is released
     *        (strong guarantee at table granularity).
     * @param vulkanContext The borrowed device context to create against.
     * @param tableView The validated BufferHandleTable to materialize.
     * @return std::expected<VulkanBufferTable, VulkanBufferTableCreationFailure> The
     *         owning table on success; the first failure (category, slot, VkResult)
     *         otherwise. Errors are values so the FFM boundary needs no unwinding (§6.4).
     * @warning MemoryOwnership: The returned table owns all created objects; see the
     *          class note. vulkanContext is borrowed and must outlive the table.
     */
    [[nodiscard]] static std::expected<VulkanBufferTable, VulkanBufferTableCreationFailure>
    createFromTable(const VulkanContext& vulkanContext,
                    const CommandStreamBufferHandleTableView& tableView);

    VulkanBufferTable(const VulkanBufferTable&) = delete;
    VulkanBufferTable& operator=(const VulkanBufferTable&) = delete;

    /** Move transfers ownership of all slots (source becomes empty). */
    VulkanBufferTable(VulkanBufferTable&& movedFrom) noexcept
        : m_logicalDevice(movedFrom.m_logicalDevice)
        , m_bufferSlots(std::move(movedFrom.m_bufferSlots)) {
        movedFrom.m_bufferSlots.clear();
    }

    VulkanBufferTable& operator=(VulkanBufferTable&&) = delete;

    /**
     * @note ThreadSafety: Not thread-safe; destroy on the load/teardown path only.
     * @brief Unmaps, destroys and frees every created slot (RAII).
     */
    ~VulkanBufferTable();

    /** The number of slots (equal to the table's entryCount; see class notes). */
    [[nodiscard]] std::uint32_t slotCount() const noexcept {
        return static_cast<std::uint32_t>(m_bufferSlots.size());
    }

    /** The materialized slot at slotIndex; precondition slotIndex < slotCount(). */
    [[nodiscard]] const BufferSlot& slot(std::uint32_t slotIndex) const noexcept {
        return m_bufferSlots[slotIndex];
    }

private:
    VulkanBufferTable(VkDevice logicalDevice, std::vector<BufferSlot> bufferSlots) noexcept
        : m_logicalDevice(logicalDevice), m_bufferSlots(std::move(bufferSlots)) {}

    VkDevice m_logicalDevice;
    std::vector<BufferSlot> m_bufferSlots;
};

} // namespace barrieww
