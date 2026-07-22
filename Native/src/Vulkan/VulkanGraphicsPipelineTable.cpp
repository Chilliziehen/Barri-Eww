#include "BarriEww/Vulkan/VulkanGraphicsPipelineTable.hpp"

#include <cstdint>

#include "BarriEww/CommandStream/CommandStreamCompareOperation.hpp"
#include "BarriEww/CommandStream/CommandStreamCullMode.hpp"
#include "BarriEww/CommandStream/CommandStreamFrontFace.hpp"
#include "BarriEww/CommandStream/CommandStreamImageAspect.hpp"
#include "BarriEww/CommandStream/CommandStreamImageFormat.hpp"
#include "BarriEww/CommandStream/CommandStreamPrimitiveTopology.hpp"
#include "BarriEww/Vulkan/VulkanFormatMapping.hpp"

namespace barrieww {

namespace {

/** Creates one VkShaderModule from a validated SPIR-V blob of the shader table. */
VkResult createShaderModuleFromBlob(VkDevice logicalDevice,
                                    std::span<const std::byte> shaderBlob,
                                    VkShaderModule& outShaderModule) {
    VkShaderModuleCreateInfo shaderModuleCreateInfo{};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = shaderBlob.size();
    shaderModuleCreateInfo.pCode =
        reinterpret_cast<const std::uint32_t*>(shaderBlob.data());
    return vkCreateShaderModule(logicalDevice, &shaderModuleCreateInfo, nullptr,
                                &outShaderModule);
}

} // namespace

/*
 * Graphics pipeline materialization (load path). Algorithm principle: one linear pass
 * over the records; per record the two stage modules are created from their blobs,
 * consumed by vkCreateGraphicsPipelines and destroyed immediately. Every piece of
 * pipeline state is decoded from the 80-byte record or fixed by the v0.1 schema
 * (§9.14) — nothing is decided later than this load-path call (T0[1]). Rendering
 * attachment formats go through VkPipelineRenderingCreateInfo (dynamic rendering, no
 * render passes). The partially-built table rolls back through its own destructor on
 * any failure (same strong-guarantee pattern as VulkanPipelineTable).
 *
 * Pseudocode (complete semantics):
 *   createFromTables(context, graphicsTable, shaderTable):
 *     for each pipeline slot:
 *       record <- graphicsTable.pipelineRecord(slot)
 *       if either shader slot >= shaderTable.entryCount -> ShaderModuleSlotOutOfRange
 *       vertexModule, fragmentModule <- vkCreateShaderModule(blobs)
 *                                                        -> ShaderModuleCreationFailed
 *       layout <- vkCreatePipelineLayout(one vertex+fragment push range when size > 0)
 *                                                        -> PipelineLayoutCreationFailed
 *       renderingInfo <- {mapped color formats, mapped depth format,
 *                         stencil format when the depth format carries stencil}
 *       pipeline <- vkCreateGraphicsPipelines(
 *           stages = [vertex "main", fragment "main"],
 *           vertexInput = empty, inputAssembly = mapped topology,
 *           viewportState = one dynamic viewport + one dynamic scissor,
 *           rasterization = fill, mapped cull/frontFace, lineWidth 1,
 *           multisample = 1 sample,
 *           depthStencil (when depth format present) = mapped enables/compare,
 *           colorBlend = per-attachment blend off, all channels written,
 *           dynamicState = [VIEWPORT, SCISSOR],
 *           pNext = renderingInfo, renderPass = VK_NULL_HANDLE)
 *                                                        -> PipelineCreationFailed
 *       destroy both shader modules   // consumed; not needed after creation
 *     return table
 */
std::expected<VulkanGraphicsPipelineTable, VulkanGraphicsPipelineTableCreationFailure>
VulkanGraphicsPipelineTable::createFromTables(
    const VulkanContext& vulkanContext,
    const CommandStreamGraphicsPipelineTableView& graphicsPipelineTableView,
    const CommandStreamShaderModuleTableView& shaderModuleTableView) {
    using enum VulkanGraphicsPipelineTableCreationError;

    const VkDevice logicalDevice = vulkanContext.logicalDevice();
    VulkanGraphicsPipelineTable partialTable{logicalDevice};
    partialTable.m_pipelineSlots.reserve(graphicsPipelineTableView.pipelineCount());
    partialTable.m_pipelineRecords.reserve(graphicsPipelineTableView.pipelineCount());

    const auto fail = [](VulkanGraphicsPipelineTableCreationError error,
                         std::uint32_t slotIndex, VkResult vulkanResult) {
        return std::unexpected(VulkanGraphicsPipelineTableCreationFailure{
            error, slotIndex, static_cast<std::int32_t>(vulkanResult)});
    };

    for (std::uint32_t slotIndex = 0;
         slotIndex < graphicsPipelineTableView.pipelineCount(); ++slotIndex) {
        const CommandStreamGraphicsPipelineRecord record =
            graphicsPipelineTableView.pipelineRecord(slotIndex);
        if (record.vertexShaderModuleSlot >= shaderModuleTableView.entryCount()
            || record.fragmentShaderModuleSlot >= shaderModuleTableView.entryCount()) {
            return fail(ShaderModuleSlotOutOfRange, slotIndex, VK_SUCCESS);
        }

        VkShaderModule vertexShaderModule = VK_NULL_HANDLE;
        VkResult vulkanResult = createShaderModuleFromBlob(
            logicalDevice, shaderModuleTableView.blob(record.vertexShaderModuleSlot),
            vertexShaderModule);
        if (vulkanResult != VK_SUCCESS) {
            return fail(ShaderModuleCreationFailed, slotIndex, vulkanResult);
        }
        VkShaderModule fragmentShaderModule = VK_NULL_HANDLE;
        vulkanResult = createShaderModuleFromBlob(
            logicalDevice, shaderModuleTableView.blob(record.fragmentShaderModuleSlot),
            fragmentShaderModule);
        if (vulkanResult != VK_SUCCESS) {
            vkDestroyShaderModule(logicalDevice, vertexShaderModule, nullptr);
            return fail(ShaderModuleCreationFailed, slotIndex, vulkanResult);
        }
        // From here both modules are alive until the single destruction point below.
        const auto destroyShaderModules = [&] {
            vkDestroyShaderModule(logicalDevice, vertexShaderModule, nullptr);
            vkDestroyShaderModule(logicalDevice, fragmentShaderModule, nullptr);
        };

        GraphicsPipelineSlot pipelineSlot{};
        pipelineSlot.pushConstantByteSize = record.pushConstantByteSize;
        // Own the partially-initialized slot immediately so rollback covers it.
        partialTable.m_pipelineSlots.push_back(pipelineSlot);
        partialTable.m_pipelineRecords.push_back(record);
        GraphicsPipelineSlot& ownedSlot = partialTable.m_pipelineSlots.back();

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags =
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = record.pushConstantByteSize;
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.pushConstantRangeCount =
            record.pushConstantByteSize > 0u ? 1u : 0u;
        pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
        vulkanResult = vkCreatePipelineLayout(logicalDevice, &pipelineLayoutCreateInfo,
                                              nullptr, &ownedSlot.pipelineLayout);
        if (vulkanResult != VK_SUCCESS) {
            destroyShaderModules();
            return fail(PipelineLayoutCreationFailed, slotIndex, vulkanResult);
        }

        VkPipelineShaderStageCreateInfo shaderStages[2]{};
        shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStages[0].module = vertexShaderModule;
        shaderStages[0].pName = "main"; // Fixed v0.1 entry point convention.
        shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStages[1].module = fragmentShaderModule;
        shaderStages[1].pName = "main";

        // v0.1 fixed state: no vertex input — geometry arrives by vertex pulling
        // through buffer device addresses (§9.14).
        VkPipelineVertexInputStateCreateInfo vertexInputState{};
        vertexInputState.sType =
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{};
        inputAssemblyState.sType =
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyState.topology = mapCommandStreamPrimitiveTopology(
            static_cast<CommandStreamPrimitiveTopology>(record.topologyValue));

        // Viewport and scissor are always dynamic (§9.11): counts only, no pointers.
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1u;
        viewportState.scissorCount = 1u;

        VkPipelineRasterizationStateCreateInfo rasterizationState{};
        rasterizationState.sType =
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizationState.cullMode = mapCommandStreamCullMode(
            static_cast<CommandStreamCullMode>(record.cullModeValue));
        rasterizationState.frontFace = mapCommandStreamFrontFace(
            static_cast<CommandStreamFrontFace>(record.frontFaceValue));
        rasterizationState.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampleState{};
        multisampleState.sType =
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencilState{};
        depthStencilState.sType =
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilState.depthTestEnable =
            record.depthTestEnable == 1u ? VK_TRUE : VK_FALSE;
        depthStencilState.depthWriteEnable =
            record.depthWriteEnable == 1u ? VK_TRUE : VK_FALSE;
        depthStencilState.depthCompareOp =
            record.depthTestEnable == 1u
                ? mapCommandStreamCompareOperation(
                      static_cast<CommandStreamCompareOperation>(
                          record.depthCompareOperationValue))
                : VK_COMPARE_OP_NEVER;

        // v0.1 fixed state: blending disabled, all color channels written (§9.14).
        VkPipelineColorBlendAttachmentState colorBlendAttachments
            [CommandStreamGraphicsPipelineRecord::s_maximumColorAttachmentCount]{};
        for (std::uint32_t attachmentIndex = 0;
             attachmentIndex < record.colorAttachmentCount; ++attachmentIndex) {
            colorBlendAttachments[attachmentIndex].colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        }
        VkPipelineColorBlendStateCreateInfo colorBlendState{};
        colorBlendState.sType =
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendState.attachmentCount = record.colorAttachmentCount;
        colorBlendState.pAttachments = colorBlendAttachments;

        constexpr VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                                    VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = 2u;
        dynamicState.pDynamicStates = dynamicStates;

        // Dynamic rendering: attachment formats replace the classic render pass.
        VkFormat colorAttachmentFormats
            [CommandStreamGraphicsPipelineRecord::s_maximumColorAttachmentCount]{};
        for (std::uint32_t attachmentIndex = 0;
             attachmentIndex < record.colorAttachmentCount; ++attachmentIndex) {
            colorAttachmentFormats[attachmentIndex] =
                mapCommandStreamImageFormat(static_cast<CommandStreamImageFormat>(
                    record.colorAttachmentFormatValues[attachmentIndex]));
        }
        VkPipelineRenderingCreateInfo renderingCreateInfo{};
        renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderingCreateInfo.colorAttachmentCount = record.colorAttachmentCount;
        renderingCreateInfo.pColorAttachmentFormats = colorAttachmentFormats;
        if (record.depthAttachmentFormatValue != 0u) {
            const auto depthFormat =
                static_cast<CommandStreamImageFormat>(record.depthAttachmentFormatValue);
            renderingCreateInfo.depthAttachmentFormat =
                mapCommandStreamImageFormat(depthFormat);
            if ((commandStreamImageFormatAspectMask(depthFormat)
                 & static_cast<std::uint32_t>(CommandStreamImageAspect::Stencil))
                != 0u) {
                renderingCreateInfo.stencilAttachmentFormat =
                    renderingCreateInfo.depthAttachmentFormat;
            }
        }

        VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{};
        graphicsPipelineCreateInfo.sType =
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        graphicsPipelineCreateInfo.pNext = &renderingCreateInfo;
        graphicsPipelineCreateInfo.stageCount = 2u;
        graphicsPipelineCreateInfo.pStages = shaderStages;
        graphicsPipelineCreateInfo.pVertexInputState = &vertexInputState;
        graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
        graphicsPipelineCreateInfo.pViewportState = &viewportState;
        graphicsPipelineCreateInfo.pRasterizationState = &rasterizationState;
        graphicsPipelineCreateInfo.pMultisampleState = &multisampleState;
        graphicsPipelineCreateInfo.pDepthStencilState =
            record.depthAttachmentFormatValue != 0u ? &depthStencilState : nullptr;
        graphicsPipelineCreateInfo.pColorBlendState = &colorBlendState;
        graphicsPipelineCreateInfo.pDynamicState = &dynamicState;
        graphicsPipelineCreateInfo.layout = ownedSlot.pipelineLayout;
        graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
        vulkanResult = vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1u,
                                                 &graphicsPipelineCreateInfo, nullptr,
                                                 &ownedSlot.pipeline);
        destroyShaderModules();
        if (vulkanResult != VK_SUCCESS) {
            return fail(PipelineCreationFailed, slotIndex, vulkanResult);
        }
    }

    return partialTable;
}

VulkanGraphicsPipelineTable::~VulkanGraphicsPipelineTable() {
    for (GraphicsPipelineSlot& pipelineSlot : m_pipelineSlots) {
        if (pipelineSlot.pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(m_logicalDevice, pipelineSlot.pipeline, nullptr);
        }
        if (pipelineSlot.pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(m_logicalDevice, pipelineSlot.pipelineLayout, nullptr);
        }
    }
}

} // namespace barrieww
