#include "BarriEww/BuildConfiguration.hpp"

#include <string>

namespace barrieww {

std::string BuildConfiguration::describe() {
    // Assemble a one-switch-per-line summary of the compiled variant. This runs on the
    // slow path only (startup / diagnostics); the queries below are all constexpr, so the
    // branches collapse at compile time and only the string assembly is done at runtime.
    const auto renderState = [](bool isEnabled) -> const char* {
        return isEnabled ? "ON" : "OFF";
    };

    std::string summary = "Barri-Eww Native build variant:";
    summary += "\n  THREADED_RECORDING            = ";
    summary += renderState(isThreadedRecordingEnabled());
    summary += "\n  BARRIEWW_BACKEND_VULKAN       = ";
    summary += renderState(isVulkanBackendEnabled());
    summary += "\n  BARRIEWW_BACKEND_DX12         = ";
    summary += renderState(isDirectX12BackendEnabled());
    summary += "\n  BARRIEWW_BACKEND_METAL        = ";
    summary += renderState(isMetalBackendEnabled());
    summary += "\n  BARRIEWW_LOGGING              = ";
    summary += renderState(isLoggingEnabled());
    summary += "\n  BARRIEWW_CRITICAL_HOTPATH_LOG = ";
    summary += renderState(isCriticalHotpathLogEnabled());
    return summary;
}

} // namespace barrieww
