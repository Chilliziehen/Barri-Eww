#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <utility>

#include "BarriEww/CommandStream/CommandStreamAttachmentLoadOp.hpp"
#include "BarriEww/CommandStream/CommandStreamAttachmentStoreOp.hpp"
#include "BarriEww/CommandStream/CommandStreamCompareOperation.hpp"
#include "BarriEww/CommandStream/CommandStreamCullMode.hpp"
#include "BarriEww/CommandStream/CommandStreamFrontFace.hpp"
#include "BarriEww/CommandStream/CommandStreamImageAspect.hpp"
#include "BarriEww/CommandStream/CommandStreamImageFormat.hpp"
#include "BarriEww/CommandStream/CommandStreamImageLayout.hpp"
#include "BarriEww/CommandStream/CommandStreamPrimitiveTopology.hpp"
#include "BarriEww/Vulkan/VulkanFormatMapping.hpp"

using namespace barrieww;

TEST_CASE("Vulkan image mappings cover every neutral catalog value",
          "[vulkan][formatMapping]") {
    const std::array formatExpectations{
        std::pair{CommandStreamImageFormat::R8G8B8A8Unorm, VK_FORMAT_R8G8B8A8_UNORM},
        std::pair{CommandStreamImageFormat::B8G8R8A8Unorm, VK_FORMAT_B8G8R8A8_UNORM},
        std::pair{CommandStreamImageFormat::R16G16B16A16Float,
                  VK_FORMAT_R16G16B16A16_SFLOAT},
        std::pair{CommandStreamImageFormat::R32Uint, VK_FORMAT_R32_UINT},
        std::pair{CommandStreamImageFormat::D32Float, VK_FORMAT_D32_SFLOAT},
        std::pair{CommandStreamImageFormat::D24UnormS8Uint,
                  VK_FORMAT_D24_UNORM_S8_UINT},
    };
    for (const auto& [imageFormat, vulkanFormat] : formatExpectations) {
        REQUIRE(mapCommandStreamImageFormat(imageFormat) == vulkanFormat);
    }
    REQUIRE(mapCommandStreamImageFormat(
                static_cast<CommandStreamImageFormat>(UINT32_MAX))
            == VK_FORMAT_UNDEFINED);

    const std::array layoutExpectations{
        std::pair{CommandStreamImageLayout::Undefined, VK_IMAGE_LAYOUT_UNDEFINED},
        std::pair{CommandStreamImageLayout::General, VK_IMAGE_LAYOUT_GENERAL},
        std::pair{CommandStreamImageLayout::ColorAttachment,
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        std::pair{CommandStreamImageLayout::DepthStencilAttachment,
                  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
        std::pair{CommandStreamImageLayout::ShaderReadOnly,
                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        std::pair{CommandStreamImageLayout::TransferSource,
                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL},
        std::pair{CommandStreamImageLayout::TransferDestination,
                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL},
        std::pair{CommandStreamImageLayout::Present, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
    };
    for (const auto& [imageLayout, vulkanLayout] : layoutExpectations) {
        REQUIRE(mapCommandStreamImageLayout(imageLayout) == vulkanLayout);
    }
    REQUIRE(mapCommandStreamImageLayout(
                static_cast<CommandStreamImageLayout>(UINT32_MAX))
            == VK_IMAGE_LAYOUT_UNDEFINED);
}

TEST_CASE("Vulkan aspect mapping preserves every assigned bit",
          "[vulkan][formatMapping]") {
    constexpr std::uint32_t colorAspect =
        static_cast<std::uint32_t>(CommandStreamImageAspect::Color);
    constexpr std::uint32_t depthAspect =
        static_cast<std::uint32_t>(CommandStreamImageAspect::Depth);
    constexpr std::uint32_t stencilAspect =
        static_cast<std::uint32_t>(CommandStreamImageAspect::Stencil);

    REQUIRE(mapCommandStreamImageAspectMask(0u) == 0u);
    REQUIRE(mapCommandStreamImageAspectMask(colorAspect) == VK_IMAGE_ASPECT_COLOR_BIT);
    REQUIRE(mapCommandStreamImageAspectMask(depthAspect) == VK_IMAGE_ASPECT_DEPTH_BIT);
    REQUIRE(mapCommandStreamImageAspectMask(stencilAspect) == VK_IMAGE_ASPECT_STENCIL_BIT);
    REQUIRE(mapCommandStreamImageAspectMask(colorAspect | depthAspect | stencilAspect)
            == (VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT
                | VK_IMAGE_ASPECT_STENCIL_BIT));
    REQUIRE(mapCommandStreamImageAspectMask(colorAspect | 0x80000000u)
            == VK_IMAGE_ASPECT_COLOR_BIT);
}

TEST_CASE("Vulkan pipeline-state mappings cover every catalog value",
          "[vulkan][formatMapping]") {
    const std::array loadOperationExpectations{
        std::pair{CommandStreamAttachmentLoadOp::Load, VK_ATTACHMENT_LOAD_OP_LOAD},
        std::pair{CommandStreamAttachmentLoadOp::Clear, VK_ATTACHMENT_LOAD_OP_CLEAR},
        std::pair{CommandStreamAttachmentLoadOp::DontCare,
                  VK_ATTACHMENT_LOAD_OP_DONT_CARE},
    };
    for (const auto& [loadOperation, vulkanOperation] : loadOperationExpectations) {
        REQUIRE(mapCommandStreamAttachmentLoadOp(loadOperation) == vulkanOperation);
    }
    REQUIRE(mapCommandStreamAttachmentLoadOp(
                static_cast<CommandStreamAttachmentLoadOp>(UINT32_MAX))
            == VK_ATTACHMENT_LOAD_OP_DONT_CARE);

    REQUIRE(mapCommandStreamAttachmentStoreOp(CommandStreamAttachmentStoreOp::Store)
            == VK_ATTACHMENT_STORE_OP_STORE);
    REQUIRE(mapCommandStreamAttachmentStoreOp(CommandStreamAttachmentStoreOp::DontCare)
            == VK_ATTACHMENT_STORE_OP_DONT_CARE);
    REQUIRE(mapCommandStreamAttachmentStoreOp(
                static_cast<CommandStreamAttachmentStoreOp>(UINT32_MAX))
            == VK_ATTACHMENT_STORE_OP_DONT_CARE);

    const std::array topologyExpectations{
        std::pair{CommandStreamPrimitiveTopology::PointList,
                  VK_PRIMITIVE_TOPOLOGY_POINT_LIST},
        std::pair{CommandStreamPrimitiveTopology::LineList,
                  VK_PRIMITIVE_TOPOLOGY_LINE_LIST},
        std::pair{CommandStreamPrimitiveTopology::LineStrip,
                  VK_PRIMITIVE_TOPOLOGY_LINE_STRIP},
        std::pair{CommandStreamPrimitiveTopology::TriangleList,
                  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST},
        std::pair{CommandStreamPrimitiveTopology::TriangleStrip,
                  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP},
    };
    for (const auto& [topology, vulkanTopology] : topologyExpectations) {
        REQUIRE(mapCommandStreamPrimitiveTopology(topology) == vulkanTopology);
    }
    REQUIRE(mapCommandStreamPrimitiveTopology(
                static_cast<CommandStreamPrimitiveTopology>(UINT32_MAX))
            == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    const std::array compareOperationExpectations{
        std::pair{CommandStreamCompareOperation::Never, VK_COMPARE_OP_NEVER},
        std::pair{CommandStreamCompareOperation::Less, VK_COMPARE_OP_LESS},
        std::pair{CommandStreamCompareOperation::Equal, VK_COMPARE_OP_EQUAL},
        std::pair{CommandStreamCompareOperation::LessOrEqual, VK_COMPARE_OP_LESS_OR_EQUAL},
        std::pair{CommandStreamCompareOperation::Greater, VK_COMPARE_OP_GREATER},
        std::pair{CommandStreamCompareOperation::NotEqual, VK_COMPARE_OP_NOT_EQUAL},
        std::pair{CommandStreamCompareOperation::GreaterOrEqual,
                  VK_COMPARE_OP_GREATER_OR_EQUAL},
        std::pair{CommandStreamCompareOperation::Always, VK_COMPARE_OP_ALWAYS},
    };
    for (const auto& [compareOperation, vulkanOperation] : compareOperationExpectations) {
        REQUIRE(mapCommandStreamCompareOperation(compareOperation) == vulkanOperation);
    }
    REQUIRE(mapCommandStreamCompareOperation(
                static_cast<CommandStreamCompareOperation>(UINT32_MAX))
            == VK_COMPARE_OP_NEVER);

    const std::array cullModeExpectations{
        std::pair{CommandStreamCullMode::None, VK_CULL_MODE_NONE},
        std::pair{CommandStreamCullMode::Front, VK_CULL_MODE_FRONT_BIT},
        std::pair{CommandStreamCullMode::Back, VK_CULL_MODE_BACK_BIT},
        std::pair{CommandStreamCullMode::FrontAndBack, VK_CULL_MODE_FRONT_AND_BACK},
    };
    for (const auto& [cullMode, vulkanCullMode] : cullModeExpectations) {
        REQUIRE(mapCommandStreamCullMode(cullMode) == vulkanCullMode);
    }
    REQUIRE(mapCommandStreamCullMode(static_cast<CommandStreamCullMode>(UINT32_MAX))
            == VK_CULL_MODE_NONE);

    REQUIRE(mapCommandStreamFrontFace(CommandStreamFrontFace::CounterClockwise)
            == VK_FRONT_FACE_COUNTER_CLOCKWISE);
    REQUIRE(mapCommandStreamFrontFace(CommandStreamFrontFace::Clockwise)
            == VK_FRONT_FACE_CLOCKWISE);
    REQUIRE(mapCommandStreamFrontFace(static_cast<CommandStreamFrontFace>(UINT32_MAX))
            == VK_FRONT_FACE_COUNTER_CLOCKWISE);
}
