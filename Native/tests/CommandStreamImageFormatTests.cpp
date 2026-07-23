#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>

#include "BarriEww/CommandStream/CommandStreamImageAspect.hpp"
#include "BarriEww/CommandStream/CommandStreamImageFormat.hpp"

using barrieww::CommandStreamImageAspect;
using barrieww::CommandStreamImageFormat;
using barrieww::commandStreamImageFormatAspectMask;
using barrieww::commandStreamImageFormatTexelByteSize;
using barrieww::isAssignedCommandStreamImageFormat;
using barrieww::isCompatibleCommandStreamImageAspectMask;

TEST_CASE("Image format helpers cover the complete assigned catalog",
          "[commandStream][imageFormat]") {
    constexpr std::uint32_t colorAspect =
        static_cast<std::uint32_t>(CommandStreamImageAspect::Color);
    constexpr std::uint32_t depthAspect =
        static_cast<std::uint32_t>(CommandStreamImageAspect::Depth);
    constexpr std::uint32_t stencilAspect =
        static_cast<std::uint32_t>(CommandStreamImageAspect::Stencil);

    struct FormatExpectation {
        CommandStreamImageFormat imageFormat;
        std::uint32_t texelByteSize;
        std::uint32_t aspectMask;
    };
    const std::array formatExpectations{
        FormatExpectation{CommandStreamImageFormat::R8G8B8A8Unorm, 4u, colorAspect},
        FormatExpectation{CommandStreamImageFormat::B8G8R8A8Unorm, 4u, colorAspect},
        FormatExpectation{CommandStreamImageFormat::R16G16B16A16Float, 8u, colorAspect},
        FormatExpectation{CommandStreamImageFormat::R32Uint, 4u, colorAspect},
        FormatExpectation{CommandStreamImageFormat::D32Float, 4u, depthAspect},
        FormatExpectation{CommandStreamImageFormat::D24UnormS8Uint, 4u,
                          depthAspect | stencilAspect},
    };

    REQUIRE_FALSE(isAssignedCommandStreamImageFormat(0u));
    for (std::uint32_t rawImageFormatValue = 1u; rawImageFormatValue <= 6u;
         ++rawImageFormatValue) {
        REQUIRE(isAssignedCommandStreamImageFormat(rawImageFormatValue));
    }
    REQUIRE_FALSE(isAssignedCommandStreamImageFormat(7u));
    REQUIRE_FALSE(isAssignedCommandStreamImageFormat(UINT32_MAX));

    for (const FormatExpectation& expectation : formatExpectations) {
        REQUIRE(commandStreamImageFormatTexelByteSize(expectation.imageFormat)
                == expectation.texelByteSize);
        REQUIRE(commandStreamImageFormatAspectMask(expectation.imageFormat)
                == expectation.aspectMask);
    }

    const auto invalidFormat = static_cast<CommandStreamImageFormat>(UINT32_MAX);
    REQUIRE(commandStreamImageFormatTexelByteSize(invalidFormat) == 0u);
    REQUIRE(commandStreamImageFormatAspectMask(invalidFormat) == 0u);
}

TEST_CASE("Image aspect compatibility rejects empty and unsupported subsets",
          "[commandStream][imageFormat]") {
    constexpr std::uint32_t colorAspect =
        static_cast<std::uint32_t>(CommandStreamImageAspect::Color);
    constexpr std::uint32_t depthAspect =
        static_cast<std::uint32_t>(CommandStreamImageAspect::Depth);
    constexpr std::uint32_t stencilAspect =
        static_cast<std::uint32_t>(CommandStreamImageAspect::Stencil);

    REQUIRE(isCompatibleCommandStreamImageAspectMask(
        CommandStreamImageFormat::R8G8B8A8Unorm, colorAspect));
    REQUIRE_FALSE(isCompatibleCommandStreamImageAspectMask(
        CommandStreamImageFormat::R8G8B8A8Unorm, 0u));
    REQUIRE_FALSE(isCompatibleCommandStreamImageAspectMask(
        CommandStreamImageFormat::R8G8B8A8Unorm, depthAspect));
    REQUIRE_FALSE(isCompatibleCommandStreamImageAspectMask(
        CommandStreamImageFormat::R8G8B8A8Unorm, colorAspect | 0x80000000u));

    REQUIRE(isCompatibleCommandStreamImageAspectMask(
        CommandStreamImageFormat::D32Float, depthAspect));
    REQUIRE_FALSE(isCompatibleCommandStreamImageAspectMask(
        CommandStreamImageFormat::D32Float, stencilAspect));
    REQUIRE(isCompatibleCommandStreamImageAspectMask(
        CommandStreamImageFormat::D24UnormS8Uint, depthAspect));
    REQUIRE(isCompatibleCommandStreamImageAspectMask(
        CommandStreamImageFormat::D24UnormS8Uint, stencilAspect));
    REQUIRE(isCompatibleCommandStreamImageAspectMask(
        CommandStreamImageFormat::D24UnormS8Uint, depthAspect | stencilAspect));

    const auto invalidFormat = static_cast<CommandStreamImageFormat>(UINT32_MAX);
    REQUIRE_FALSE(isCompatibleCommandStreamImageAspectMask(invalidFormat, colorAspect));
}
