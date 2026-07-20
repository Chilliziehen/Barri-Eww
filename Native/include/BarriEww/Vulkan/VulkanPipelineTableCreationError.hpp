#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumeration; trivially thread-safe.
 * @brief Failure categories of load-time pipeline materialization (PipelineHandleTable
 *        + ShaderModuleTable → native pipelines). Values are stable (§6.4).
 */
enum class VulkanPipelineTableCreationError : std::uint32_t {
    /** An entry references a shader module slot outside the shader table. */
    ShaderModuleSlotOutOfRange = 1,
    /** vkCreateShaderModule failed. */
    ShaderModuleCreationFailed = 2,
    /** vkCreatePipelineLayout failed. */
    PipelineLayoutCreationFailed = 3,
    /** vkCreateComputePipelines failed. */
    PipelineCreationFailed = 4,
};

} // namespace barrieww
