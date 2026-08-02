#pragma once

#include <cstdint>

namespace barrieww::testing {

/** Describes how an unavailable real Vulkan execution environment affects a test. */
enum class HostImagePresentationExecutionUnavailableDisposition : std::uint8_t {
    Skip,
    Fail,
};

/**
 * @note ThreadSafety: Pure and safe from every thread.
 * @brief Maps continuous-integration presence to unavailable-environment disposition.
 * @param bool isContinuousIntegrationEnvironment Whether the test runs under CI
 * @return HostImagePresentationExecutionUnavailableDisposition Skip locally or fail in CI
 * @warning MemoryOwnership: Returns a value and accesses no external memory.
 */
[[nodiscard]] HostImagePresentationExecutionUnavailableDisposition
hostImagePresentationExecutionUnavailableDisposition(
    bool isContinuousIntegrationEnvironment) noexcept;

/**
 * @note ThreadSafety: Reads process environment storage without modifying it.
 * @brief Reports whether the standard CI environment variable has value true.
 * @return bool True for the GitHub Actions CI=true environment
 * @warning MemoryOwnership: Borrows process environment storage only for the call duration.
 */
[[nodiscard]] bool isContinuousIntegrationEnvironment() noexcept;

} // namespace barrieww::testing
