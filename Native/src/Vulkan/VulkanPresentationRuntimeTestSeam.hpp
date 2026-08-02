#pragma once

#if !defined(BARRIEWW_PRESENTATION_RUNTIME_TEST_SEAM)
    #error "VulkanPresentationRuntimeTestSeam.hpp is test-only infrastructure"
#endif

#include <cstdint>

namespace barrieww::testing {

/** Test-only creation checkpoints compiled out of every production target. */
enum class VulkanPresentationRuntimeCreationCheckpoint : std::uint32_t {
    AfterSwapchainCreation = 0u,
    AfterFirstFrameSlotCreation = 1u,
    AfterCommandPoolCreation = 2u,
};

using VulkanPresentationRuntimeCreationCheckpointOperation =
    void (*)(VulkanPresentationRuntimeCreationCheckpoint creationCheckpoint);

inline VulkanPresentationRuntimeCreationCheckpointOperation
    g_vulkanPresentationRuntimeCreationCheckpointOperation = nullptr;

/**
 * @note ThreadSafety: Test-thread confined through the direct mock presentation executable.
 * @brief Invokes the configured deterministic creation exception seam when present.
 * @param VulkanPresentationRuntimeCreationCheckpoint creationCheckpoint Current checkpoint
 * @warning MemoryOwnership: Invokes a borrowed static function address and retains nothing.
 */
inline void reachVulkanPresentationRuntimeCreationCheckpoint(
    VulkanPresentationRuntimeCreationCheckpoint creationCheckpoint) {
    if (g_vulkanPresentationRuntimeCreationCheckpointOperation != nullptr) {
        g_vulkanPresentationRuntimeCreationCheckpointOperation(creationCheckpoint);
    }
}

} // namespace barrieww::testing
