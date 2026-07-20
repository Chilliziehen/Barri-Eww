#pragma once

#include <cstdint>
#include <expected>
#include <vector>

#include <vulkan/vulkan.h>

#include "BarriEww/CommandStream/CommandStreamPipelineHandleTableView.hpp"
#include "BarriEww/CommandStream/CommandStreamShaderModuleTableView.hpp"
#include "BarriEww/Vulkan/VulkanContext.hpp"
#include "BarriEww/Vulkan/VulkanPipelineTableCreationFailure.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Creation and destruction are single-threaded (load path,
 *       ADR-0003 whitelist item "装载"). After creation the accessors are read-only
 *       and thread-safe. This blanket statement covers all accessors below (§2.6).
 * @brief Runtime materialization of one validated PipelineHandleTable: a dense array
 *        of {VkPipeline, VkPipelineLayout} indexed by slot (ADR-0002 D2 slot
 *        indirection). Shader modules are created from the ShaderModuleTable blobs,
 *        consumed by pipeline creation and destroyed immediately (they are not needed
 *        afterwards). Entry point is the fixed convention "main"; the v0.1 layout is
 *        push constants only (compute stage).
 * @warning MemoryOwnership: OWNS every created VkPipeline / VkPipelineLayout and
 *          destroys them in the destructor (RAII, §1.4). BORROWS the VulkanContext's
 *          device, which must outlive this table.
 */
class VulkanPipelineTable {
public:
    /**
     * @note ThreadSafety: Plain value type; trivially thread-safe.
     * @brief One materialized pipeline slot with the data recording needs: the
     *        pipeline, its layout (for push constants) and the declared push range.
     */
    struct PipelineSlot {
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        std::uint32_t pushConstantByteSize = 0;
    };

    /**
     * @note ThreadSafety: Not thread-safe; call on the load path.
     * @brief Materializes every pipeline entry, resolving shader module slots against
     *        the shader table (the cross-table check lives here, where both validated
     *        tables are present). On failure everything already created is released.
     * @param vulkanContext The borrowed device context to create against.
     * @param pipelineTableView The validated PipelineHandleTable to materialize.
     * @param shaderModuleTableView The validated ShaderModuleTable the entries index.
     * @return std::expected<VulkanPipelineTable, VulkanPipelineTableCreationFailure>
     *         The owning table on success; the first failure otherwise (§6.4 values).
     */
    [[nodiscard]] static std::expected<VulkanPipelineTable, VulkanPipelineTableCreationFailure>
    createFromTables(const VulkanContext& vulkanContext,
                     const CommandStreamPipelineHandleTableView& pipelineTableView,
                     const CommandStreamShaderModuleTableView& shaderModuleTableView);

    VulkanPipelineTable(const VulkanPipelineTable&) = delete;
    VulkanPipelineTable& operator=(const VulkanPipelineTable&) = delete;

    /** Move transfers ownership of all slots (source becomes empty). */
    VulkanPipelineTable(VulkanPipelineTable&& movedFrom) noexcept
        : m_logicalDevice(movedFrom.m_logicalDevice)
        , m_pipelineSlots(std::move(movedFrom.m_pipelineSlots)) {
        movedFrom.m_pipelineSlots.clear();
    }

    VulkanPipelineTable& operator=(VulkanPipelineTable&&) = delete;

    /**
     * @note ThreadSafety: Not thread-safe; destroy on the load/teardown path only.
     * @brief Destroys every created pipeline and layout (RAII).
     */
    ~VulkanPipelineTable();

    /** The number of pipeline slots (see class notes). */
    [[nodiscard]] std::uint32_t slotCount() const noexcept {
        return static_cast<std::uint32_t>(m_pipelineSlots.size());
    }

    /** The materialized slot at slotIndex; precondition slotIndex < slotCount(). */
    [[nodiscard]] const PipelineSlot& slot(std::uint32_t slotIndex) const noexcept {
        return m_pipelineSlots[slotIndex];
    }

private:
    VulkanPipelineTable(VkDevice logicalDevice,
                        std::vector<PipelineSlot> pipelineSlots) noexcept
        : m_logicalDevice(logicalDevice), m_pipelineSlots(std::move(pipelineSlots)) {}

    VkDevice m_logicalDevice;
    std::vector<PipelineSlot> m_pipelineSlots;
};

} // namespace barrieww
