#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamBarrierBatchTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamBufferHandleTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamGraphicsPipelineTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamImageHandleTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamImageViewHandleTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamModuleSectionType.hpp"
#include "BarriEww/CommandStream/CommandStreamModuleValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamRenderingTemplateTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamShaderModuleTableValidator.hpp"
#include "BarriEww/Vulkan/CommandBufferRecorder.hpp"
#include "BarriEww/Vulkan/VulkanBufferTable.hpp"
#include "BarriEww/Vulkan/VulkanContext.hpp"
#include "BarriEww/Vulkan/VulkanGraphicsPipelineTable.hpp"
#include "BarriEww/Vulkan/VulkanImageTable.hpp"
#include "BarriEww/Vulkan/VulkanImageViewTable.hpp"
#include "TestVulkanDeviceHarness.hpp"

using barrieww::CommandBufferRecorder;
using barrieww::CommandStreamBarrierBatchTableValidator;
using barrieww::CommandStreamBufferHandleTableValidator;
using barrieww::CommandStreamGraphicsPipelineTableValidator;
using barrieww::CommandStreamImageHandleTableValidator;
using barrieww::CommandStreamImageViewHandleTableValidator;
using barrieww::CommandStreamModuleSectionType;
using barrieww::CommandStreamModuleValidator;
using barrieww::CommandStreamRenderingTemplateTableValidator;
using barrieww::CommandStreamShaderModuleTableValidator;
using barrieww::VulkanBufferTable;
using barrieww::VulkanContext;
using barrieww::VulkanGraphicsPipelineTable;
using barrieww::VulkanImageTable;
using barrieww::VulkanImageViewTable;
using barrieww::testing::TestVulkanDeviceHarness;

namespace {

/** Reads a whole binary file from the shared repository fixture directory. */
std::vector<std::byte> readTestDataFile(const char* relativeFilePath) {
    const std::string absoluteFilePath =
        std::string(BARRIEWW_TEST_DATA_DIRECTORY) + "/" + relativeFilePath;
    std::ifstream fileStream(absoluteFilePath, std::ios::binary | std::ios::ate);
    REQUIRE(fileStream.is_open());
    const std::streamsize fileByteCount = fileStream.tellg();
    fileStream.seekg(0);
    std::vector<std::byte> fileBytes(static_cast<std::size_t>(fileByteCount));
    fileStream.read(reinterpret_cast<char*>(fileBytes.data()), fileByteCount);
    return fileBytes;
}

} // namespace

// The full production shape on one committed module blob (ADR-0001/0003 spine): the
// SAME FirstTriangleModule.becs the Java write side produced byte-exactly is loaded
// here as an opaque byte range and drives the whole frame — module validation, every
// table section's schema validation, materialization of buffers/image/view/shaders/
// graphics pipeline, recording of the embedded lane-0 stream, one submit — and the
// readback asserts all 64 texels rasterized green over the cleared black target.
// Nothing about the frame exists outside the module except the CPU-seeded vertex
// bytes (whose GPU visibility flows through the pushed device address).
TEST_CASE("Committed first triangle module replays end-to-end on the GPU",
          "[graphicsModule][gpu]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    if (!harness->supportsDynamicRendering()) {
        SKIP("driver does not support dynamic rendering (core Vulkan 1.3)");
    }
    if (!harness->supportsBufferDeviceAddress()) {
        SKIP("driver does not support bufferDeviceAddress (core Vulkan 1.2)");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};

    // Load path step 1: one opaque byte range in, full structural validation.
    const std::vector<std::byte> moduleBytes =
        readTestDataFile("CommandStream/FirstTriangleModule.becs");
    const auto moduleView = CommandStreamModuleValidator::validate(moduleBytes);
    REQUIRE(moduleView.has_value());

    // Load path step 2: per-section schema validation, all sections found by type.
    const auto bufferSection =
        moduleView->findSection(CommandStreamModuleSectionType::BufferHandleTable);
    const auto imageSection =
        moduleView->findSection(CommandStreamModuleSectionType::ImageHandleTable);
    const auto imageViewSection =
        moduleView->findSection(CommandStreamModuleSectionType::ImageViewHandleTable);
    const auto shaderSection =
        moduleView->findSection(CommandStreamModuleSectionType::ShaderModuleTable);
    const auto graphicsPipelineSection =
        moduleView->findSection(CommandStreamModuleSectionType::GraphicsPipelineTable);
    const auto barrierSection =
        moduleView->findSection(CommandStreamModuleSectionType::BarrierBatchTable);
    const auto renderingTemplateSection =
        moduleView->findSection(CommandStreamModuleSectionType::RenderingTemplateTable);
    REQUIRE(bufferSection.has_value());
    REQUIRE(imageSection.has_value());
    REQUIRE(imageViewSection.has_value());
    REQUIRE(shaderSection.has_value());
    REQUIRE(graphicsPipelineSection.has_value());
    REQUIRE(barrierSection.has_value());
    REQUIRE(renderingTemplateSection.has_value());

    const auto bufferTableView =
        CommandStreamBufferHandleTableValidator::validate(*bufferSection);
    const auto imageTableView =
        CommandStreamImageHandleTableValidator::validate(*imageSection);
    const auto imageViewTableView =
        CommandStreamImageViewHandleTableValidator::validate(*imageViewSection);
    const auto shaderTableView =
        CommandStreamShaderModuleTableValidator::validate(*shaderSection);
    const auto graphicsPipelineTableView =
        CommandStreamGraphicsPipelineTableValidator::validate(*graphicsPipelineSection);
    const auto barrierTableView =
        CommandStreamBarrierBatchTableValidator::validate(*barrierSection);
    const auto renderingTemplateTableView =
        CommandStreamRenderingTemplateTableValidator::validate(*renderingTemplateSection);
    REQUIRE(bufferTableView.has_value());
    REQUIRE(imageTableView.has_value());
    REQUIRE(imageViewTableView.has_value());
    REQUIRE(shaderTableView.has_value());
    REQUIRE(graphicsPipelineTableView.has_value());
    REQUIRE(barrierTableView.has_value());
    REQUIRE(renderingTemplateTableView.has_value());

    // Load path step 3: materialization (dense native-handle arrays, ADR-0002 D2).
    auto bufferTable = VulkanBufferTable::createFromTable(vulkanContext, *bufferTableView);
    REQUIRE(bufferTable.has_value());
    auto imageTable = VulkanImageTable::createFromTable(vulkanContext, *imageTableView);
    REQUIRE(imageTable.has_value());
    const auto sharedImageTable =
        std::make_shared<const VulkanImageTable>(std::move(*imageTable));
    auto imageViewTable =
        VulkanImageViewTable::createFromTable(sharedImageTable, *imageViewTableView);
    REQUIRE(imageViewTable.has_value());
    auto graphicsPipelineTable = VulkanGraphicsPipelineTable::createFromTables(
        vulkanContext, *graphicsPipelineTableView, *shaderTableView);
    REQUIRE(graphicsPipelineTable.has_value());

    // The only out-of-module input: the CPU seeds the fullscreen triangle into the
    // persistently mapped vertex buffer (slot 0) the pushed address points at.
    const float trianglePositions[6] = {-1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f};
    std::byte* vertexBufferBytes = bufferTable->slot(0u).mappedPointer;
    REQUIRE(vertexBufferBytes != nullptr);
    std::memcpy(vertexBufferBytes, trianglePositions, sizeof trianglePositions);

    // Load path step 4: record the module's own lane-0 stream and submit once.
    const VkCommandBuffer commandBuffer = harness->allocateCommandBuffer();
    REQUIRE(CommandBufferRecorder::record(
                commandBuffer, moduleView->laneStream(0u),
                {.bufferTable = &*bufferTable,
                 .barrierBatchTableView = &*barrierTableView,
                 .imageTable = sharedImageTable.get(),
                 .imageViewTable = &*imageViewTable,
                 .renderingTemplateTableView = &*renderingTemplateTableView,
                 .graphicsPipelineTable = &*graphicsPipelineTable})
                .has_value());
    REQUIRE(harness->submitAndWait(commandBuffer));

    const std::byte* readbackBytes = bufferTable->slot(1u).mappedPointer;
    REQUIRE(readbackBytes != nullptr);
    for (std::uint32_t texelIndex = 0; texelIndex < 64u; ++texelIndex) {
        REQUIRE(readbackBytes[texelIndex * 4u + 0u] == std::byte{0x00}); // red
        REQUIRE(readbackBytes[texelIndex * 4u + 1u] == std::byte{0xFF}); // green
        REQUIRE(readbackBytes[texelIndex * 4u + 2u] == std::byte{0x00}); // blue
        REQUIRE(readbackBytes[texelIndex * 4u + 3u] == std::byte{0xFF}); // alpha
    }
}
