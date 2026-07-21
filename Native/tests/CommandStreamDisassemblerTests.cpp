#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <string>
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
