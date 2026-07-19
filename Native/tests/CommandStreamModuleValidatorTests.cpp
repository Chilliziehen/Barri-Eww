#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamModuleSectionType.hpp"
#include "BarriEww/CommandStream/CommandStreamModuleValidationError.hpp"
#include "BarriEww/CommandStream/CommandStreamModuleValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamOpcode.hpp"
#include "BarriEww/CommandStream/CommandStreamValidationError.hpp"
#include "TestCommandStreamBuilder.hpp"
#include "TestCommandStreamModuleBuilder.hpp"

using barrieww::CommandStreamModuleSectionType;
using barrieww::CommandStreamModuleValidationError;
using barrieww::CommandStreamModuleValidator;
using barrieww::CommandStreamOpcode;
using barrieww::CommandStreamValidationError;
using barrieww::testing::TestCommandStreamBuilder;
using barrieww::testing::TestCommandStreamModuleBuilder;

namespace {

constexpr std::uint64_t sharedGraphHash = 0xFEDCBA9876543210ull;

std::uint16_t rawValue(CommandStreamModuleSectionType sectionType) {
    return static_cast<std::uint16_t>(sectionType);
}

/** A minimal valid lane stream containing a single Draw command. */
std::vector<std::byte> makeSingleDrawLaneStream(std::uint32_t laneIndex) {
    TestCommandStreamBuilder streamBuilder{laneIndex, sharedGraphHash};
    streamBuilder.appendCommand(static_cast<std::uint16_t>(CommandStreamOpcode::Draw),
                                std::vector<std::byte>(16u, std::byte{0}));
    return streamBuilder.build();
}

} // namespace

TEST_CASE("Module validator accepts an empty module", "[commandStreamModule]") {
    TestCommandStreamModuleBuilder moduleBuilder{sharedGraphHash};
    const std::vector<std::byte> moduleBytes = moduleBuilder.build();

    const auto validationResult = CommandStreamModuleValidator::validate(moduleBytes);
    REQUIRE(validationResult.has_value());
    REQUIRE(validationResult->graphHash() == sharedGraphHash);
    REQUIRE(validationResult->laneStreamCount() == 0u);
    REQUIRE(validationResult->sectionEntries().empty());
}

TEST_CASE("Module validator accepts tables plus two lanes and exposes them",
          "[commandStreamModule]") {
    TestCommandStreamModuleBuilder moduleBuilder{sharedGraphHash};
    const std::vector<std::byte> bufferTableBytes(24u, std::byte{0x42});
    moduleBuilder.addSection(rawValue(CommandStreamModuleSectionType::BufferHandleTable), 0u,
                             bufferTableBytes);
    moduleBuilder.addSection(rawValue(CommandStreamModuleSectionType::LaneStream), 0u,
                             makeSingleDrawLaneStream(0u));
    moduleBuilder.addSection(rawValue(CommandStreamModuleSectionType::LaneStream), 1u,
                             makeSingleDrawLaneStream(1u));
    const std::vector<std::byte> moduleBytes = moduleBuilder.build();

    const auto validationResult = CommandStreamModuleValidator::validate(moduleBytes);
    REQUIRE(validationResult.has_value());
    REQUIRE(validationResult->laneStreamCount() == 2u);
    REQUIRE(validationResult->sectionEntries().size() == 3u);

    const auto bufferTableSection =
        validationResult->findSection(CommandStreamModuleSectionType::BufferHandleTable);
    REQUIRE(bufferTableSection.has_value());
    REQUIRE(bufferTableSection->size() == 24u);
    REQUIRE((*bufferTableSection)[0] == std::byte{0x42});

    REQUIRE_FALSE(
        validationResult->findSection(CommandStreamModuleSectionType::SamplerHandleTable)
            .has_value());

    for (std::uint32_t laneIndex = 0; laneIndex < 2u; ++laneIndex) {
        const auto& laneStreamView = validationResult->laneStream(laneIndex);
        REQUIRE(laneStreamView.laneIndex() == laneIndex);
        REQUIRE(laneStreamView.commandCount() == 1u);
        REQUIRE((*laneStreamView.begin()).opcode == CommandStreamOpcode::Draw);
    }
}

TEST_CASE("Module validator rejects malformed containers with the precise failure",
          "[commandStreamModule]") {
    SECTION("module smaller than the header") {
        const std::vector<std::byte> tinyBytes(16u, std::byte{0});
        const auto validationResult = CommandStreamModuleValidator::validate(tinyBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamModuleValidationError::ModuleTooSmall);
    }

    SECTION("corrupted magic") {
        TestCommandStreamModuleBuilder moduleBuilder{sharedGraphHash};
        auto moduleBytes = moduleBuilder.build();
        moduleBytes[3] = std::byte{0x00};
        const auto validationResult = CommandStreamModuleValidator::validate(moduleBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamModuleValidationError::InvalidModuleMagic);
    }

    SECTION("future minor version is rejected") {
        TestCommandStreamModuleBuilder moduleBuilder{sharedGraphHash};
        auto moduleBytes = moduleBuilder.build();
        TestCommandStreamModuleBuilder::overwriteValueAt<std::uint16_t>(moduleBytes, 6u, 0x0063u);
        const auto validationResult = CommandStreamModuleValidator::validate(moduleBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamModuleValidationError::UnsupportedModuleVersion);
    }

    SECTION("directory that does not fit") {
        TestCommandStreamModuleBuilder moduleBuilder{sharedGraphHash};
        auto moduleBytes = moduleBuilder.build();
        // Claim three sections in a 32-byte module.
        TestCommandStreamModuleBuilder::overwriteValueAt<std::uint32_t>(moduleBytes, 8u, 3u);
        const auto validationResult = CommandStreamModuleValidator::validate(moduleBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamModuleValidationError::DirectoryOutOfBounds);
    }

    SECTION("unknown section type") {
        TestCommandStreamModuleBuilder moduleBuilder{sharedGraphHash};
        moduleBuilder.addSection(0x7777u, 0u, std::vector<std::byte>(8u, std::byte{0}));
        const auto moduleBytes = moduleBuilder.build();
        const auto validationResult = CommandStreamModuleValidator::validate(moduleBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamModuleValidationError::UnknownSectionType);
        REQUIRE(validationResult.error().byteOffset == 32u);
    }

    SECTION("non-zero reserved directory flags") {
        TestCommandStreamModuleBuilder moduleBuilder{sharedGraphHash};
        moduleBuilder.addSection(rawValue(CommandStreamModuleSectionType::BufferHandleTable),
                                 0u, std::vector<std::byte>(8u, std::byte{0}),
                                 /*reservedFlags=*/1u);
        const auto moduleBytes = moduleBuilder.build();
        const auto validationResult = CommandStreamModuleValidator::validate(moduleBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamModuleValidationError::NonZeroReservedFlags);
    }

    SECTION("section out of bounds") {
        TestCommandStreamModuleBuilder moduleBuilder{sharedGraphHash};
        moduleBuilder.addSection(rawValue(CommandStreamModuleSectionType::BufferHandleTable),
                                 0u, std::vector<std::byte>(8u, std::byte{0}));
        auto moduleBytes = moduleBuilder.build();
        // Entry 0's byteSize lives at 32 + 16; inflate it past the module end.
        TestCommandStreamModuleBuilder::overwriteValueAt<std::uint64_t>(moduleBytes, 48u, 4096u);
        const auto validationResult = CommandStreamModuleValidator::validate(moduleBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamModuleValidationError::SectionOutOfBounds);
    }

    SECTION("misaligned section offset") {
        TestCommandStreamModuleBuilder moduleBuilder{sharedGraphHash};
        moduleBuilder.addSection(rawValue(CommandStreamModuleSectionType::BufferHandleTable),
                                 0u, std::vector<std::byte>(16u, std::byte{0}));
        auto moduleBytes = moduleBuilder.build();
        // Entry 0's byteOffset lives at 32 + 8; make it odd.
        TestCommandStreamModuleBuilder::overwriteValueAt<std::uint64_t>(moduleBytes, 40u, 57u);
        const auto validationResult = CommandStreamModuleValidator::validate(moduleBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamModuleValidationError::MisalignedSectionOffset);
    }

    SECTION("overlapping sections") {
        TestCommandStreamModuleBuilder moduleBuilder{sharedGraphHash};
        moduleBuilder.addSection(rawValue(CommandStreamModuleSectionType::BufferHandleTable),
                                 0u, std::vector<std::byte>(16u, std::byte{0}));
        moduleBuilder.addSection(rawValue(CommandStreamModuleSectionType::ImageHandleTable),
                                 0u, std::vector<std::byte>(16u, std::byte{0}));
        auto moduleBytes = moduleBuilder.build();
        // Point entry 1 (byteOffset at 32 + 24 + 8 = 64) back into entry 0's range.
        TestCommandStreamModuleBuilder::overwriteValueAt<std::uint64_t>(moduleBytes, 64u, 88u);
        const auto validationResult = CommandStreamModuleValidator::validate(moduleBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamModuleValidationError::SectionOverlap);
    }

    SECTION("duplicate section identity") {
        TestCommandStreamModuleBuilder moduleBuilder{sharedGraphHash};
        moduleBuilder.addSection(rawValue(CommandStreamModuleSectionType::BufferHandleTable),
                                 0u, std::vector<std::byte>(8u, std::byte{0}));
        moduleBuilder.addSection(rawValue(CommandStreamModuleSectionType::BufferHandleTable),
                                 0u, std::vector<std::byte>(8u, std::byte{0}));
        const auto moduleBytes = moduleBuilder.build();
        const auto validationResult = CommandStreamModuleValidator::validate(moduleBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamModuleValidationError::DuplicateSection);
    }

    SECTION("laneStreamCount disagreeing with the directory") {
        TestCommandStreamModuleBuilder moduleBuilder{sharedGraphHash};
        moduleBuilder.addSection(rawValue(CommandStreamModuleSectionType::LaneStream), 0u,
                                 makeSingleDrawLaneStream(0u));
        const auto moduleBytes =
            moduleBuilder.build(/*overrideLaneStreamCount=*/true, /*forced=*/2u);
        const auto validationResult = CommandStreamModuleValidator::validate(moduleBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamModuleValidationError::LaneStreamCountMismatch);
    }

    SECTION("non-contiguous lane indices") {
        TestCommandStreamModuleBuilder moduleBuilder{sharedGraphHash};
        moduleBuilder.addSection(rawValue(CommandStreamModuleSectionType::LaneStream), 1u,
                                 makeSingleDrawLaneStream(1u));
        const auto moduleBytes = moduleBuilder.build();
        const auto validationResult = CommandStreamModuleValidator::validate(moduleBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamModuleValidationError::LaneIndexMismatch);
    }

    SECTION("embedded lane stream with its own defect surfaces the nested failure") {
        TestCommandStreamModuleBuilder moduleBuilder{sharedGraphHash};
        std::vector<std::byte> corruptedLaneStream = makeSingleDrawLaneStream(0u);
        corruptedLaneStream[0] = std::byte{0x00}; // break the lane magic
        moduleBuilder.addSection(rawValue(CommandStreamModuleSectionType::LaneStream), 0u,
                                 corruptedLaneStream);
        const auto moduleBytes = moduleBuilder.build();
        const auto validationResult = CommandStreamModuleValidator::validate(moduleBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamModuleValidationError::LaneStreamInvalid);
        REQUIRE(validationResult.error().laneStreamFailure.has_value());
        REQUIRE(validationResult.error().laneStreamFailure->error
                == CommandStreamValidationError::InvalidMagic);
    }

    SECTION("lane stream whose graphHash disagrees with the module") {
        TestCommandStreamModuleBuilder moduleBuilder{sharedGraphHash};
        TestCommandStreamBuilder foreignStreamBuilder{0u, /*graphHash=*/0x1111u};
        foreignStreamBuilder.appendCommand(
            static_cast<std::uint16_t>(CommandStreamOpcode::Draw),
            std::vector<std::byte>(16u, std::byte{0}));
        moduleBuilder.addSection(rawValue(CommandStreamModuleSectionType::LaneStream), 0u,
                                 foreignStreamBuilder.build());
        const auto moduleBytes = moduleBuilder.build();
        const auto validationResult = CommandStreamModuleValidator::validate(moduleBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamModuleValidationError::GraphHashMismatch);
    }
}
