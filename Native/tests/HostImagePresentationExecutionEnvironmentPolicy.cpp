#include "HostImagePresentationExecutionEnvironmentPolicy.hpp"

#include <cstdlib>
#include <string_view>

namespace barrieww::testing {

HostImagePresentationExecutionUnavailableDisposition
hostImagePresentationExecutionUnavailableDisposition(
    bool isContinuousIntegrationEnvironment) noexcept {
    return isContinuousIntegrationEnvironment
        ? HostImagePresentationExecutionUnavailableDisposition::Fail
        : HostImagePresentationExecutionUnavailableDisposition::Skip;
}

bool isContinuousIntegrationEnvironment() noexcept {
    const char* continuousIntegrationValue = std::getenv("CI");
    return continuousIntegrationValue != nullptr
        && std::string_view{continuousIntegrationValue} == "true";
}

} // namespace barrieww::testing
