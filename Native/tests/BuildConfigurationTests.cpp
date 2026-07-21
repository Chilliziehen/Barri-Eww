#include <catch2/catch_test_macros.hpp>

#include <string>

#include "BarriEww/BuildConfiguration.hpp"

using barrieww::BuildConfiguration;

// Each feature query must agree with the macro CMake injected for it (spec §3.3). These
// are compile-time checks (STATIC_REQUIRE), matching the constexpr nature of the queries
// and the T0[1] compile-time-configuration principle.
TEST_CASE("BuildConfiguration queries match the injected compile-time switches",
          "[buildConfiguration]") {
#if defined(THREADED_RECORDING) && THREADED_RECORDING
    STATIC_REQUIRE(BuildConfiguration::isThreadedRecordingEnabled());
#else
    STATIC_REQUIRE_FALSE(BuildConfiguration::isThreadedRecordingEnabled());
#endif

#if defined(BARRIEWW_BACKEND_VULKAN) && BARRIEWW_BACKEND_VULKAN
    STATIC_REQUIRE(BuildConfiguration::isVulkanBackendEnabled());
#else
    STATIC_REQUIRE_FALSE(BuildConfiguration::isVulkanBackendEnabled());
#endif

#if defined(BARRIEWW_BACKEND_DX12) && BARRIEWW_BACKEND_DX12
    STATIC_REQUIRE(BuildConfiguration::isDirectX12BackendEnabled());
#else
    STATIC_REQUIRE_FALSE(BuildConfiguration::isDirectX12BackendEnabled());
#endif

#if defined(BARRIEWW_BACKEND_METAL) && BARRIEWW_BACKEND_METAL
    STATIC_REQUIRE(BuildConfiguration::isMetalBackendEnabled());
#else
    STATIC_REQUIRE_FALSE(BuildConfiguration::isMetalBackendEnabled());
#endif

#if defined(BARRIEWW_LOGGING) && BARRIEWW_LOGGING
    STATIC_REQUIRE(BuildConfiguration::isLoggingEnabled());
#else
    STATIC_REQUIRE_FALSE(BuildConfiguration::isLoggingEnabled());
#endif

#if defined(BARRIEWW_CRITICAL_HOTPATH_LOG) && BARRIEWW_CRITICAL_HOTPATH_LOG
    STATIC_REQUIRE(BuildConfiguration::isCriticalHotpathLogEnabled());
#else
    STATIC_REQUIRE_FALSE(BuildConfiguration::isCriticalHotpathLogEnabled());
#endif
}

TEST_CASE("BuildConfiguration::describe produces a readable, non-empty summary",
          "[buildConfiguration]") {
    const std::string summary = BuildConfiguration::describe();

    REQUIRE_FALSE(summary.empty());
    REQUIRE(summary.find("Barri-Eww Native build variant") != std::string::npos);
    // Every switch must appear in the summary so diagnostics are complete.
    REQUIRE(summary.find("THREADED_RECORDING") != std::string::npos);
    REQUIRE(summary.find("BARRIEWW_BACKEND_VULKAN") != std::string::npos);
    REQUIRE(summary.find("BARRIEWW_CRITICAL_HOTPATH_LOG") != std::string::npos);
}
