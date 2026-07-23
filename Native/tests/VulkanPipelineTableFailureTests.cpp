#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include <vulkan/vulkan.h>

#include "BarriEww/CommandStream/CommandStreamPipelineHandleTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamShaderModuleTableValidator.hpp"
#include "BarriEww/Vulkan/VulkanContext.hpp"
#include "BarriEww/Vulkan/VulkanPipelineTable.hpp"
#include "BarriEww/Vulkan/VulkanPipelineTableCreationError.hpp"

using barrieww::CommandStreamPipelineHandleTableValidator;
using barrieww::CommandStreamShaderModuleTableValidator;
using barrieww::VulkanContext;
using barrieww::VulkanContextCreateInfo;
using barrieww::VulkanPipelineTable;
using barrieww::VulkanPipelineTableCreationError;

namespace {

VkResult g_shaderModuleResult = VK_SUCCESS;
VkResult g_pipelineLayoutResult = VK_SUCCESS;
VkResult g_pipelineResult = VK_SUCCESS;
std::uint32_t g_destroyShaderModuleCount = 0u;
std::uint32_t g_destroyPipelineLayoutCount = 0u;
std::uint32_t g_destroyPipelineCount = 0u;

/** Resets deterministic Vulkan results and destruction counters. */
void resetVulkanCallState() {
    g_shaderModuleResult = VK_SUCCESS;
    g_pipelineLayoutResult = VK_SUCCESS;
    g_pipelineResult = VK_SUCCESS;
    g_destroyShaderModuleCount = 0u;
    g_destroyPipelineLayoutCount = 0u;
    g_destroyPipelineCount = 0u;
}

/**
 * Appends one little-endian value to a byte vector.
 * @param std::vector<std::byte>& targetBytes Destination byte vector
 * @param ValueType value Fixed-width value to append
 */
template <typename ValueType>
void appendValue(std::vector<std::byte>& targetBytes, ValueType value) {
    const std::size_t writeOffset = targetBytes.size();
    targetBytes.resize(writeOffset + sizeof value);
    std::memcpy(targetBytes.data() + writeOffset, &value, sizeof value);
}

/** Returns one validator-accepted shader table for deterministic Vulkan replacements.
 * @return std::vector<std::byte> Complete one-blob shader table bytes
 */
std::vector<std::byte> makeShaderTableBytes() {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint64_t{24});
    appendValue(tableBytes, std::uint64_t{24});
    appendValue(tableBytes, std::uint32_t{0x07230203});
    for (std::uint32_t wordIndex = 0u; wordIndex < 5u; ++wordIndex) {
        appendValue(tableBytes, std::uint32_t{0});
    }
    return tableBytes;
}

/** Returns one valid compute-pipeline table referencing shader slot zero.
 * @return std::vector<std::byte> Complete one-entry pipeline table bytes
 */
std::vector<std::byte> makePipelineTableBytes() {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{1});
    appendValue(tableBytes, std::uint32_t{0});
    appendValue(tableBytes, std::uint32_t{8});
    appendValue(tableBytes, std::uint32_t{0});
    return tableBytes;
}

/** Creates a context from non-null opaque handles consumed only by local replacements.
 * @return VulkanContext Borrowed-handle context for the isolated failure executable
 */
VulkanContext makeVulkanContext() {
    VulkanContextCreateInfo createInfo{};
    createInfo.instance = reinterpret_cast<VkInstance>(1u);
    createInfo.physicalDevice = reinterpret_cast<VkPhysicalDevice>(2u);
    createInfo.logicalDevice = reinterpret_cast<VkDevice>(3u);
    createInfo.queueFamilyIndices.graphicsFamily = 0u;
    createInfo.queueFamilyIndices.presentFamily = 0u;
    createInfo.graphicsQueue = reinterpret_cast<VkQueue>(4u);
    createInfo.presentQueue = reinterpret_cast<VkQueue>(4u);
    return VulkanContext{createInfo};
}

} // namespace

/**
 * Deterministic link-time replacement for shader-module creation.
 * @param VkDevice device Opaque device handle
 * @param const VkShaderModuleCreateInfo* createInfo Requested module description
 * @param const VkAllocationCallbacks* allocationCallbacks Optional allocator callbacks
 * @param VkShaderModule* shaderModule Writable shader-module handle
 * @return VkResult Configured creation result
 */
extern "C" VkResult VKAPI_CALL vkCreateShaderModule(
    VkDevice device, const VkShaderModuleCreateInfo* createInfo,
    const VkAllocationCallbacks* allocationCallbacks, VkShaderModule* shaderModule) {
    static_cast<void>(device);
    static_cast<void>(createInfo);
    static_cast<void>(allocationCallbacks);
    if (g_shaderModuleResult == VK_SUCCESS) {
        *shaderModule = reinterpret_cast<VkShaderModule>(100u);
    }
    return g_shaderModuleResult;
}

/**
 * Records deterministic shader-module destruction.
 * @param VkDevice device Opaque device handle
 * @param VkShaderModule shaderModule Shader module being destroyed
 * @param const VkAllocationCallbacks* allocationCallbacks Optional allocator callbacks
 */
extern "C" void VKAPI_CALL vkDestroyShaderModule(
    VkDevice device, VkShaderModule shaderModule,
    const VkAllocationCallbacks* allocationCallbacks) {
    static_cast<void>(device);
    static_cast<void>(shaderModule);
    static_cast<void>(allocationCallbacks);
    ++g_destroyShaderModuleCount;
}

/**
 * Deterministic link-time replacement for pipeline-layout creation.
 * @param VkDevice device Opaque device handle
 * @param const VkPipelineLayoutCreateInfo* createInfo Requested layout description
 * @param const VkAllocationCallbacks* allocationCallbacks Optional allocator callbacks
 * @param VkPipelineLayout* pipelineLayout Writable pipeline-layout handle
 * @return VkResult Configured creation result
 */
extern "C" VkResult VKAPI_CALL vkCreatePipelineLayout(
    VkDevice device, const VkPipelineLayoutCreateInfo* createInfo,
    const VkAllocationCallbacks* allocationCallbacks, VkPipelineLayout* pipelineLayout) {
    static_cast<void>(device);
    static_cast<void>(createInfo);
    static_cast<void>(allocationCallbacks);
    if (g_pipelineLayoutResult == VK_SUCCESS) {
        *pipelineLayout = reinterpret_cast<VkPipelineLayout>(200u);
    }
    return g_pipelineLayoutResult;
}

/**
 * Records deterministic pipeline-layout destruction.
 * @param VkDevice device Opaque device handle
 * @param VkPipelineLayout pipelineLayout Pipeline layout being destroyed
 * @param const VkAllocationCallbacks* allocationCallbacks Optional allocator callbacks
 */
extern "C" void VKAPI_CALL vkDestroyPipelineLayout(
    VkDevice device, VkPipelineLayout pipelineLayout,
    const VkAllocationCallbacks* allocationCallbacks) {
    static_cast<void>(device);
    static_cast<void>(pipelineLayout);
    static_cast<void>(allocationCallbacks);
    ++g_destroyPipelineLayoutCount;
}

/**
 * Deterministic link-time replacement for compute-pipeline creation.
 * @param VkDevice device Opaque device handle
 * @param VkPipelineCache pipelineCache Optional pipeline cache
 * @param std::uint32_t createInfoCount Number of pipeline descriptions
 * @param const VkComputePipelineCreateInfo* createInfos Pipeline descriptions
 * @param const VkAllocationCallbacks* allocationCallbacks Optional allocator callbacks
 * @param VkPipeline* pipelines Writable pipeline handles
 * @return VkResult Configured creation result
 */
extern "C" VkResult VKAPI_CALL vkCreateComputePipelines(
    VkDevice device, VkPipelineCache pipelineCache, std::uint32_t createInfoCount,
    const VkComputePipelineCreateInfo* createInfos,
    const VkAllocationCallbacks* allocationCallbacks, VkPipeline* pipelines) {
    static_cast<void>(device);
    static_cast<void>(pipelineCache);
    static_cast<void>(createInfoCount);
    static_cast<void>(createInfos);
    static_cast<void>(allocationCallbacks);
    if (g_pipelineResult == VK_SUCCESS) {
        *pipelines = reinterpret_cast<VkPipeline>(300u);
    }
    return g_pipelineResult;
}

/**
 * Records deterministic compute-pipeline destruction.
 * @param VkDevice device Opaque device handle
 * @param VkPipeline pipeline Pipeline being destroyed
 * @param const VkAllocationCallbacks* allocationCallbacks Optional allocator callbacks
 */
extern "C" void VKAPI_CALL vkDestroyPipeline(
    VkDevice device, VkPipeline pipeline,
    const VkAllocationCallbacks* allocationCallbacks) {
    static_cast<void>(device);
    static_cast<void>(pipeline);
    static_cast<void>(allocationCallbacks);
    ++g_destroyPipelineCount;
}

TEST_CASE("Vulkan pipeline table reports deterministic creation failures and rolls back",
          "[vulkanPipelineTable][failure]") {
    const VulkanContext vulkanContext = makeVulkanContext();
    const std::vector<std::byte> shaderTableBytes = makeShaderTableBytes();
    const std::vector<std::byte> pipelineTableBytes = makePipelineTableBytes();
    const auto shaderTableView =
        CommandStreamShaderModuleTableValidator::validate(shaderTableBytes);
    const auto pipelineTableView =
        CommandStreamPipelineHandleTableValidator::validate(pipelineTableBytes);
    REQUIRE(shaderTableView.has_value());
    REQUIRE(pipelineTableView.has_value());

    SECTION("shader module creation failure") {
        resetVulkanCallState();
        g_shaderModuleResult = VK_ERROR_OUT_OF_HOST_MEMORY;
        const auto result = VulkanPipelineTable::createFromTables(
            vulkanContext, *pipelineTableView, *shaderTableView);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().error == VulkanPipelineTableCreationError::ShaderModuleCreationFailed);
        REQUIRE(result.error().slotIndex == 0u);
        REQUIRE(result.error().resultValue == VK_ERROR_OUT_OF_HOST_MEMORY);
        REQUIRE(g_destroyShaderModuleCount == 0u);
    }

    SECTION("pipeline layout creation failure") {
        resetVulkanCallState();
        g_pipelineLayoutResult = VK_ERROR_OUT_OF_DEVICE_MEMORY;
        const auto result = VulkanPipelineTable::createFromTables(
            vulkanContext, *pipelineTableView, *shaderTableView);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().error == VulkanPipelineTableCreationError::PipelineLayoutCreationFailed);
        REQUIRE(result.error().resultValue == VK_ERROR_OUT_OF_DEVICE_MEMORY);
        REQUIRE(g_destroyShaderModuleCount == 1u);
        REQUIRE(g_destroyPipelineLayoutCount == 0u);
    }

    SECTION("compute pipeline creation failure") {
        resetVulkanCallState();
        g_pipelineResult = VK_ERROR_DEVICE_LOST;
        const auto result = VulkanPipelineTable::createFromTables(
            vulkanContext, *pipelineTableView, *shaderTableView);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().error == VulkanPipelineTableCreationError::PipelineCreationFailed);
        REQUIRE(result.error().resultValue == VK_ERROR_DEVICE_LOST);
        REQUIRE(g_destroyShaderModuleCount == 1u);
        REQUIRE(g_destroyPipelineLayoutCount == 1u);
        REQUIRE(g_destroyPipelineCount == 0u);
    }
}
