#pragma once

#include <cstdint>
#include <expected>
#include <vector>

#include <vulkan/vulkan.h>

#include "BarriEww/CommandStream/CommandStreamGraphicsPipelineRecord.hpp"
#include "BarriEww/CommandStream/CommandStreamGraphicsPipelineTableView.hpp"
#include "BarriEww/CommandStream/CommandStreamShaderModuleTableView.hpp"
#include "BarriEww/Vulkan/VulkanContext.hpp"
#include "BarriEww/Vulkan/VulkanGraphicsPipelineTableCreationFailure.hpp"

namespace barrieww {

/**
 * @note ThreadSafety: Creation and destruction are single-threaded (load path,
 *       ADR-0003 whitelist item "装载"). After creation the accessors are read-only
 *       and thread-safe. This blanket statement covers all accessors below (§2.6).
 * @brief Runtime materialization of one validated GraphicsPipelineTable: a dense array
 *        of {VkPipeline, VkPipelineLayout} indexed by slot (ADR-0002 D2), created
 *        against dynamic rendering (VkPipelineRenderingCreateInfo, no render passes —
 *        ADR-0003 / MC 26.2). Hot handles live in the slot array; the cold neutral
 *        records are retained so the recorder can check attachment-format compatibility
 *        against the open rendering template at bind time (§9.11). Shader modules are
 *        creation-time-only inputs (created, consumed, destroyed). Entry points are the
 *        fixed convention "main"; the push range is visible to vertex+fragment; the
 *        layout is push-constants only (§9.14).
 * @warning MemoryOwnership: OWNS every created VkPipeline / VkPipelineLayout and
 *          destroys them in the destructor (RAII, §1.4). BORROWS the VulkanContext's
 *          device, which must outlive this table.
 */
class VulkanGraphicsPipelineTable {
public:
    /**
     * @note ThreadSafety: Plain value type; trivially thread-safe.
     * @brief One materialized graphics pipeline slot with the data recording needs:
     *        the pipeline, its layout (for push constants) and the declared push range.
     */
    struct GraphicsPipelineSlot {
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        std::uint32_t pushConstantByteSize = 0;
    };

    /**
     * @note ThreadSafety: Not thread-safe; call on the load path.
     * @brief Materializes every graphics pipeline record, resolving shader module slots
     *        against the shader table (the cross-table check lives here, where both
     *        validated tables are present). On failure everything already created is
     *        released through the partial table destructor.
     * @param vulkanContext The borrowed device context to create against. The device
     *        must have been created with the dynamicRendering feature enabled.
     * @param graphicsPipelineTableView The validated GraphicsPipelineTable to materialize.
     * @param shaderModuleTableView The validated ShaderModuleTable the records index.
     * @return std::expected<VulkanGraphicsPipelineTable,
     *         VulkanGraphicsPipelineTableCreationFailure> The owning table on success;
     *         the first failure otherwise (§6.4 values).
     */
    [[nodiscard]] static std::expected<VulkanGraphicsPipelineTable,
                                       VulkanGraphicsPipelineTableCreationFailure>
    createFromTables(const VulkanContext& vulkanContext,
                     const CommandStreamGraphicsPipelineTableView& graphicsPipelineTableView,
                     const CommandStreamShaderModuleTableView& shaderModuleTableView);

    VulkanGraphicsPipelineTable(const VulkanGraphicsPipelineTable&) = delete;
    VulkanGraphicsPipelineTable& operator=(const VulkanGraphicsPipelineTable&) = delete;

    /** Move transfers ownership of all slots (source becomes empty). */
    VulkanGraphicsPipelineTable(VulkanGraphicsPipelineTable&& movedFrom) noexcept
        : m_logicalDevice(movedFrom.m_logicalDevice)
        , m_pipelineSlots(std::move(movedFrom.m_pipelineSlots))
        , m_pipelineRecords(std::move(movedFrom.m_pipelineRecords)) {
        movedFrom.m_pipelineSlots.clear();
        movedFrom.m_pipelineRecords.clear();
    }

    VulkanGraphicsPipelineTable& operator=(VulkanGraphicsPipelineTable&&) = delete;

    /**
     * @note ThreadSafety: Not thread-safe; destroy on the load/teardown path only.
     * @brief Destroys every created pipeline and layout (RAII).
     */
    ~VulkanGraphicsPipelineTable();

    /** The number of graphics pipeline slots (see class notes). */
    [[nodiscard]] std::uint32_t slotCount() const noexcept {
        return static_cast<std::uint32_t>(m_pipelineSlots.size());
    }

    /** The materialized hot slot at slotIndex; precondition slotIndex < slotCount(). */
    [[nodiscard]] const GraphicsPipelineSlot& slot(std::uint32_t slotIndex) const noexcept {
        return m_pipelineSlots[slotIndex];
    }

    /** The cold neutral record at slotIndex; precondition slotIndex < slotCount(). */
    [[nodiscard]] const CommandStreamGraphicsPipelineRecord&
    pipelineRecord(std::uint32_t slotIndex) const noexcept {
        return m_pipelineRecords[slotIndex];
    }

private:
    VulkanGraphicsPipelineTable(VkDevice logicalDevice) noexcept
        : m_logicalDevice(logicalDevice) {}

    VkDevice m_logicalDevice;
    std::vector<GraphicsPipelineSlot> m_pipelineSlots;
    std::vector<CommandStreamGraphicsPipelineRecord> m_pipelineRecords;
};

} // namespace barrieww
