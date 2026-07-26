#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumeration; trivially thread-safe.
 * @brief Failure categories of load-time graphics pipeline materialization
 *        (GraphicsPipelineTable + ShaderModuleTable → native pipelines against dynamic
 *        rendering). Values are stable (§6.4).
 */
enum class VulkanGraphicsPipelineTableCreationError : std::uint32_t {
    /** A record references a shader module slot outside the shader table. */
    ShaderModuleSlotOutOfRange = 1,
    /** vkCreateShaderModule failed. */
    ShaderModuleCreationFailed = 2,
    /** vkCreatePipelineLayout failed. */
    PipelineLayoutCreationFailed = 3,
    /** vkCreateGraphicsPipelines failed. */
    PipelineCreationFailed = 4,
};

} // namespace barrieww
