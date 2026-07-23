#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <format>
#include <string>
#include <utility>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamDisassembler.hpp"
#include "BarriEww/CommandStream/CommandStreamModuleSectionType.hpp"
#include "BarriEww/CommandStream/CommandStreamModuleValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamOpcode.hpp"
#include "BarriEww/CommandStream/CommandStreamValidator.hpp"
#include "TestCommandStreamBuilder.hpp"
#include "TestCommandStreamModuleBuilder.hpp"

using barrieww::CommandStreamDisassembler;
using barrieww::CommandStreamModuleSectionType;
using barrieww::CommandStreamModuleValidator;
using barrieww::CommandStreamOpcode;
using barrieww::CommandStreamValidator;
using barrieww::testing::TestCommandStreamBuilder;
using barrieww::testing::TestCommandStreamModuleBuilder;

TEST_CASE("Disassembler renders a lane stream with names, sizes and payload hex",
          "[commandStream][disassembler]") {
    TestCommandStreamBuilder streamBuilder{3u, 0x00000000DEADBEEFull};
    std::vector<std::byte> drawPayload(16u, std::byte{0});
    drawPayload[0] = std::byte{0x2A}; // vertexCount = 42 little-endian
    streamBuilder.appendCommand(static_cast<std::uint16_t>(CommandStreamOpcode::Draw),
                                drawPayload);
    streamBuilder.appendCommand(static_cast<std::uint16_t>(CommandStreamOpcode::EndRendering),
                                {});
    const std::vector<std::byte> streamBytes = streamBuilder.build();

    const auto validationResult = CommandStreamValidator::validate(streamBytes);
    REQUIRE(validationResult.has_value());
    const std::string renderedText = CommandStreamDisassembler::disassemble(*validationResult);

    REQUIRE(renderedText.find("laneIndex=3") != std::string::npos);
    REQUIRE(renderedText.find("commandCount=2") != std::string::npos);
    REQUIRE(renderedText.find("graphHash=0x00000000deadbeef") != std::string::npos);
    REQUIRE(renderedText.find("[0] Draw (0x0010) byteSize=24 payload: 2a 00 00 00")
            != std::string::npos);
    REQUIRE(renderedText.find("[1] EndRendering (0x0031) byteSize=8 payload: ")
            != std::string::npos);
}

TEST_CASE("Disassembler renders every assigned opcode catalog name",
          "[commandStream][disassembler]") {
    const std::array opcodeExpectations{
        std::pair{CommandStreamOpcode::BindGraphicsPipeline, "BindGraphicsPipeline"},
        std::pair{CommandStreamOpcode::BindComputePipeline, "BindComputePipeline"},
        std::pair{CommandStreamOpcode::BindVertexBuffers, "BindVertexBuffers"},
        std::pair{CommandStreamOpcode::BindIndexBuffer, "BindIndexBuffer"},
        std::pair{CommandStreamOpcode::PushConstants, "PushConstants"},
        std::pair{CommandStreamOpcode::SetViewport, "SetViewport"},
        std::pair{CommandStreamOpcode::SetScissor, "SetScissor"},
        std::pair{CommandStreamOpcode::PushBufferDeviceAddress, "PushBufferDeviceAddress"},
        std::pair{CommandStreamOpcode::Draw, "Draw"},
        std::pair{CommandStreamOpcode::DrawIndexed, "DrawIndexed"},
        std::pair{CommandStreamOpcode::DrawIndirect, "DrawIndirect"},
        std::pair{CommandStreamOpcode::DrawIndexedIndirect, "DrawIndexedIndirect"},
        std::pair{CommandStreamOpcode::DrawIndexedIndirectCount, "DrawIndexedIndirectCount"},
        std::pair{CommandStreamOpcode::Dispatch, "Dispatch"},
        std::pair{CommandStreamOpcode::DispatchIndirect, "DispatchIndirect"},
        std::pair{CommandStreamOpcode::BeginRendering, "BeginRendering"},
        std::pair{CommandStreamOpcode::EndRendering, "EndRendering"},
        std::pair{CommandStreamOpcode::ExecuteBarrierBatch, "ExecuteBarrierBatch"},
        std::pair{CommandStreamOpcode::CopyBuffer, "CopyBuffer"},
        std::pair{CommandStreamOpcode::CopyImage, "CopyImage"},
        std::pair{CommandStreamOpcode::BlitImage, "BlitImage"},
        std::pair{CommandStreamOpcode::ClearColorImage, "ClearColorImage"},
        std::pair{CommandStreamOpcode::ClearDepthStencilImage, "ClearDepthStencilImage"},
        std::pair{CommandStreamOpcode::ResolveImage, "ResolveImage"},
        std::pair{CommandStreamOpcode::CopyBufferToImage, "CopyBufferToImage"},
        std::pair{CommandStreamOpcode::CopyImageToBuffer, "CopyImageToBuffer"},
        std::pair{CommandStreamOpcode::PushDescriptorSet, "PushDescriptorSet"},
        std::pair{CommandStreamOpcode::DebugLabelBegin, "DebugLabelBegin"},
        std::pair{CommandStreamOpcode::DebugLabelEnd, "DebugLabelEnd"},
        std::pair{CommandStreamOpcode::InvokeOpaqueExtensionNode,
                  "InvokeOpaqueExtensionNode"},
    };

    TestCommandStreamBuilder streamBuilder;
    for (const auto& [opcode, opcodeName] : opcodeExpectations) {
        static_cast<void>(opcodeName);
        streamBuilder.appendCommand(static_cast<std::uint16_t>(opcode), {});
    }
    const std::vector<std::byte> streamBytes = streamBuilder.build();
    const auto validationResult = CommandStreamValidator::validate(streamBytes);
    REQUIRE(validationResult.has_value());
    REQUIRE(validationResult->commandCount() == opcodeExpectations.size());

    const std::string renderedText = CommandStreamDisassembler::disassemble(*validationResult);
    for (const auto& [opcode, opcodeName] : opcodeExpectations) {
        const std::string expectedText = std::format(
            "{} (0x{:04x})", opcodeName, static_cast<std::uint16_t>(opcode));
        REQUIRE(renderedText.find(expectedText) != std::string::npos);
    }
}

TEST_CASE("Disassembler renders every assigned module section catalog name",
          "[commandStream][disassembler]") {
    const std::array sectionExpectations{
        std::pair{CommandStreamModuleSectionType::PipelineHandleTable,
                  "PipelineHandleTable"},
        std::pair{CommandStreamModuleSectionType::BufferHandleTable,
                  "BufferHandleTable"},
        std::pair{CommandStreamModuleSectionType::ImageHandleTable,
                  "ImageHandleTable"},
        std::pair{CommandStreamModuleSectionType::ImageViewHandleTable,
                  "ImageViewHandleTable"},
        std::pair{CommandStreamModuleSectionType::SamplerHandleTable,
                  "SamplerHandleTable"},
        std::pair{CommandStreamModuleSectionType::ShaderModuleTable,
                  "ShaderModuleTable"},
        std::pair{CommandStreamModuleSectionType::GraphicsPipelineTable,
                  "GraphicsPipelineTable"},
        std::pair{CommandStreamModuleSectionType::BarrierBatchTable,
                  "BarrierBatchTable"},
        std::pair{CommandStreamModuleSectionType::RenderingTemplateTable,
                  "RenderingTemplateTable"},
        std::pair{CommandStreamModuleSectionType::PushDescriptorTemplateTable,
                  "PushDescriptorTemplateTable"},
    };

    TestCommandStreamModuleBuilder moduleBuilder;
    for (const auto& [sectionType, sectionName] : sectionExpectations) {
        static_cast<void>(sectionName);
        moduleBuilder.addSection(static_cast<std::uint16_t>(sectionType), 0u,
                                 std::vector<std::byte>(8u, std::byte{0}));
    }
    TestCommandStreamBuilder laneStreamBuilder;
    moduleBuilder.addSection(
        static_cast<std::uint16_t>(CommandStreamModuleSectionType::LaneStream), 0u,
        laneStreamBuilder.build());
    const std::vector<std::byte> moduleBytes = moduleBuilder.build();
    const auto validationResult = CommandStreamModuleValidator::validate(moduleBytes);
    REQUIRE(validationResult.has_value());

    const std::string renderedText = CommandStreamDisassembler::disassemble(*validationResult);
    for (const auto& [sectionType, sectionName] : sectionExpectations) {
        const std::string expectedText = std::format(
            "section {} (0x{:04x})", sectionName,
            static_cast<std::uint16_t>(sectionType));
        REQUIRE(renderedText.find(expectedText) != std::string::npos);
    }
    REQUIRE(renderedText.find("section LaneStream (0x0020)") != std::string::npos);
}

TEST_CASE("Disassembler renders a module directory and its embedded lanes",
          "[commandStream][disassembler]") {
    TestCommandStreamModuleBuilder moduleBuilder{0x1122334455667788ull};
    moduleBuilder.addSection(
        static_cast<std::uint16_t>(CommandStreamModuleSectionType::BufferHandleTable), 0u,
        std::vector<std::byte>(16u, std::byte{0}));
    TestCommandStreamBuilder laneStreamBuilder{0u, 0x1122334455667788ull};
    laneStreamBuilder.appendCommand(
        static_cast<std::uint16_t>(CommandStreamOpcode::Dispatch),
        std::vector<std::byte>(16u, std::byte{0}));
    moduleBuilder.addSection(
        static_cast<std::uint16_t>(CommandStreamModuleSectionType::LaneStream), 0u,
        laneStreamBuilder.build());
    const std::vector<std::byte> moduleBytes = moduleBuilder.build();

    const auto validationResult = CommandStreamModuleValidator::validate(moduleBytes);
    REQUIRE(validationResult.has_value());
    const std::string renderedText = CommandStreamDisassembler::disassemble(*validationResult);

    REQUIRE(renderedText.find("BECS module: sectionCount=2 laneStreamCount=1")
            != std::string::npos);
    REQUIRE(renderedText.find("section BufferHandleTable (0x0002) index=0")
            != std::string::npos);
    REQUIRE(renderedText.find("section LaneStream (0x0020) index=0") != std::string::npos);
    REQUIRE(renderedText.find("BECS lane stream: laneIndex=0") != std::string::npos);
    REQUIRE(renderedText.find("Dispatch (0x0020)") != std::string::npos);
}
