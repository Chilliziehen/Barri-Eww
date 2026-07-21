#include "BarriEww/Vulkan/VulkanPipelineTable.hpp"

#include <cstdint>

namespace barrieww {

/*
 * Pipeline materialization (load path). Algorithm principle: one linear pass over the
 * pipeline entries; per entry the shader module is created from its blob, consumed by
 * vkCreateComputePipelines and destroyed immediately (modules are creation-time-only
 * inputs). The partially-built table rolls back through its own destructor on any
 * failure (same strong-guarantee pattern as VulkanBufferTable).
 *
 * Pseudocode (complete semantics):
 *   createFromTables(context, pipelineTable, shaderTable):
 *     for each pipeline slot:
 *       entry <- pipelineTable.entry(slot)
 *       if entry.shaderModuleSlot >= shaderTable.entryCount -> ShaderModuleSlotOutOfRange
 *       shaderModule <- vkCreateShaderModule(blob)           -> ShaderModuleCreationFailed
 *       layout <- vkCreatePipelineLayout(push range only)    -> PipelineLayoutCreationFailed
 *       pipeline <- vkCreateComputePipelines("main", module, layout)
 *                                                            -> PipelineCreationFailed
 *       vkDestroyShaderModule(shaderModule)   // consumed; not needed after creation
 *     return table
 */
std::expected<VulkanPipelineTable, VulkanPipelineTableCreationFailure>
VulkanPipelineTable::createFromTables(
    const VulkanContext& vulkanContext,
    const CommandStreamPipelineHandleTableView& pipelineTableView,
    const CommandStreamShaderModuleTableView& shaderModuleTableView) {
    using enum VulkanPipelineTableCreationError;

    const VkDevice logicalDevice = vulkanContext.logicalDevice();
    VulkanPipelineTable partialTable{logicalDevice, {}};
    partialTable.m_pipelineSlots.reserve(pipelineTableView.entryCount());

    const auto fail = [](VulkanPipelineTableCreationError error, std::uint32_t slotIndex,
                         VkResult vulkanResult) {
        return std::unexpected(VulkanPipelineTableCreationFailure{
            error, slotIndex, static_cast<std::int32_t>(vulkanResult)});
    };

    for (std::uint32_t slotIndex = 0; slotIndex < pipelineTableView.entryCount();
         ++slotIndex) {
        const CommandStreamPipelineHandleTableEntry entry = pipelineTableView.entry(slotIndex);
        if (entry.shaderModuleSlot >= shaderModuleTableView.entryCount()) {
            return fail(ShaderModuleSlotOutOfRange, slotIndex, VK_SUCCESS);
        }
        const std::span<const std::byte> shaderBlob =
            shaderModuleTableView.blob(entry.shaderModuleSlot);

        VkShaderModuleCreateInfo shaderModuleCreateInfo{};
        shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderModuleCreateInfo.codeSize = shaderBlob.size();
        shaderModuleCreateInfo.pCode =
            reinterpret_cast<const std::uint32_t*>(shaderBlob.data());
        VkShaderModule shaderModule = VK_NULL_HANDLE;
        VkResult vulkanResult = vkCreateShaderModule(logicalDevice, &shaderModuleCreateInfo,
                                                     nullptr, &shaderModule);
        if (vulkanResult != VK_SUCCESS) {
            return fail(ShaderModuleCreationFailed, slotIndex, vulkanResult);
        }

        PipelineSlot pipelineSlot{};
        pipelineSlot.pushConstantByteSize = entry.pushConstantByteSize;
        // Own the partially-initialized slot immediately so rollback covers it.
        partialTable.m_pipelineSlots.push_back(pipelineSlot);
        PipelineSlot& ownedSlot = partialTable.m_pipelineSlots.back();

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = entry.pushConstantByteSize;
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.pushConstantRangeCount =
            entry.pushConstantByteSize > 0u ? 1u : 0u;
        pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
        vulkanResult = vkCreatePipelineLayout(logicalDevice, &pipelineLayoutCreateInfo,
                                              nullptr, &ownedSlot.pipelineLayout);
        if (vulkanResult != VK_SUCCESS) {
            vkDestroyShaderModule(logicalDevice, shaderModule, nullptr);
            return fail(PipelineLayoutCreationFailed, slotIndex, vulkanResult);
        }

        VkComputePipelineCreateInfo computePipelineCreateInfo{};
        computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineCreateInfo.stage.sType =
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computePipelineCreateInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computePipelineCreateInfo.stage.module = shaderModule;
        computePipelineCreateInfo.stage.pName = "main"; // Fixed v0.1 entry point convention.
        computePipelineCreateInfo.layout = ownedSlot.pipelineLayout;
        vulkanResult = vkCreateComputePipelines(logicalDevice, VK_NULL_HANDLE, 1u,
                                                &computePipelineCreateInfo, nullptr,
                                                &ownedSlot.pipeline);
        vkDestroyShaderModule(logicalDevice, shaderModule, nullptr);
        if (vulkanResult != VK_SUCCESS) {
            return fail(PipelineCreationFailed, slotIndex, vulkanResult);
        }
    }

    return partialTable;
}

VulkanPipelineTable::~VulkanPipelineTable() {
    for (PipelineSlot& pipelineSlot : m_pipelineSlots) {
        if (pipelineSlot.pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(m_logicalDevice, pipelineSlot.pipeline, nullptr);
        }
        if (pipelineSlot.pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(m_logicalDevice, pipelineSlot.pipelineLayout, nullptr);
        }
    }
}

} // namespace barrieww
