#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamImageHandleTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamImageViewHandleTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamModuleSectionType.hpp"
#include "BarriEww/CommandStream/CommandStreamModuleValidator.hpp"
#include "BarriEww/Vulkan/VulkanContext.hpp"
#include "BarriEww/Vulkan/VulkanImageTable.hpp"
#include "BarriEww/Vulkan/VulkanImageViewTable.hpp"
#include "BarriEww/Vulkan/VulkanImageViewTableCreationError.hpp"
#include "TestVulkanDeviceHarness.hpp"

using barrieww::CommandStreamImageHandleTableValidator;
using barrieww::CommandStreamImageViewHandleTableValidator;
using barrieww::CommandStreamModuleSectionType;
using barrieww::CommandStreamModuleValidator;
using barrieww::VulkanContext;
using barrieww::VulkanImageTable;
using barrieww::VulkanImageViewTable;
using barrieww::VulkanImageViewTableCreationError;
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

/** Appends one little-endian value to a byte vector. */
template <typename ValueType>
void appendValue(std::vector<std::byte>& targetBytes, ValueType value) {
    const std::size_t writeOffset = targetBytes.size();
    targetBytes.resize(writeOffset + sizeof value);
    std::memcpy(targetBytes.data() + writeOffset, &value, sizeof value);
}

struct RawImageEntry {
    std::uint32_t imageKindValue = 2u;
    std::uint32_t formatValue = 1u;
    std::uint32_t width = 8u;
    std::uint32_t height = 8u;
    std::uint32_t depth = 1u;
    std::uint32_t mipLevelCount = 2u;
    std::uint32_t arrayLayerCount = 2u;
    std::uint32_t sampleCountValue = 1u;
    std::uint32_t usageFlags = 0x4u;
    std::uint32_t importIdentifier = 0u;
};

struct RawImageViewEntry {
    std::uint32_t imageSlot = 0u;
    std::uint32_t imageViewKindValue = 2u;
    std::uint32_t formatValue = 1u;
    std::uint32_t aspectMaskValue = 0x1u;
    std::uint32_t baseMipLevel = 0u;
    std::uint32_t mipLevelCount = 1u;
    std::uint32_t baseArrayLayer = 0u;
    std::uint32_t arrayLayerCount = 1u;
};

std::vector<std::byte> buildImageTable(const std::vector<RawImageEntry>& rawEntries) {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, static_cast<std::uint32_t>(rawEntries.size()));
    appendValue(tableBytes, std::uint32_t{0});
    for (const RawImageEntry& rawEntry : rawEntries) {
        appendValue(tableBytes, rawEntry.imageKindValue);
        appendValue(tableBytes, rawEntry.formatValue);
        appendValue(tableBytes, rawEntry.width);
        appendValue(tableBytes, rawEntry.height);
        appendValue(tableBytes, rawEntry.depth);
        appendValue(tableBytes, rawEntry.mipLevelCount);
        appendValue(tableBytes, rawEntry.arrayLayerCount);
        appendValue(tableBytes, rawEntry.sampleCountValue);
        appendValue(tableBytes, rawEntry.usageFlags);
        appendValue(tableBytes, rawEntry.importIdentifier);
    }
    return tableBytes;
}

std::vector<std::byte> buildImageViewTable(
    const std::vector<RawImageViewEntry>& rawEntries) {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, static_cast<std::uint32_t>(rawEntries.size()));
    appendValue(tableBytes, std::uint32_t{0});
    for (const RawImageViewEntry& rawEntry : rawEntries) {
        appendValue(tableBytes, rawEntry.imageSlot);
        appendValue(tableBytes, rawEntry.imageViewKindValue);
        appendValue(tableBytes, rawEntry.formatValue);
        appendValue(tableBytes, rawEntry.aspectMaskValue);
        appendValue(tableBytes, rawEntry.baseMipLevel);
        appendValue(tableBytes, rawEntry.mipLevelCount);
        appendValue(tableBytes, rawEntry.baseArrayLayer);
        appendValue(tableBytes, rawEntry.arrayLayerCount);
    }
    return tableBytes;
}

} // namespace

TEST_CASE("Java-produced image and image-view module materializes on Vulkan",
          "[vulkan][imageViewHandleTable][golden]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};
    const std::vector<std::byte> moduleBytes =
        readTestDataFile("CommandStream/ImageAndImageViewModule.becs");
    const auto moduleView = CommandStreamModuleValidator::validate(moduleBytes);
    REQUIRE(moduleView.has_value());

    const auto imageSection =
        moduleView->findSection(CommandStreamModuleSectionType::ImageHandleTable);
    const auto imageViewSection =
        moduleView->findSection(CommandStreamModuleSectionType::ImageViewHandleTable);
    REQUIRE(imageSection.has_value());
    REQUIRE(imageViewSection.has_value());
    const auto imageTableView =
        CommandStreamImageHandleTableValidator::validate(*imageSection);
    const auto imageViewTableView =
        CommandStreamImageViewHandleTableValidator::validate(*imageViewSection);
    REQUIRE(imageTableView.has_value());
    REQUIRE(imageViewTableView.has_value());

    auto materializedSourceImageTable =
        VulkanImageTable::createFromTable(vulkanContext, *imageTableView);
    REQUIRE(materializedSourceImageTable.has_value());
    auto sourceImageTable = std::make_shared<VulkanImageTable>(
        std::move(*materializedSourceImageTable));
    const auto imageViewTable =
        VulkanImageViewTable::createFromTable(sourceImageTable, *imageViewTableView);
    REQUIRE(imageViewTable.has_value());
    REQUIRE(imageViewTable->slotCount() == 1u);
    REQUIRE(imageViewTable->imageView(0u) != VK_NULL_HANDLE);
}

TEST_CASE("Vulkan image-view table materializes validated compatible views",
          "[vulkan][imageViewHandleTable]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};
    const std::vector<std::byte> imageTableBytes = buildImageTable({RawImageEntry{}});
    const auto imageTableView =
        CommandStreamImageHandleTableValidator::validate(imageTableBytes);
    REQUIRE(imageTableView.has_value());
    auto materializedSourceImageTable =
        VulkanImageTable::createFromTable(vulkanContext, *imageTableView);
    REQUIRE(materializedSourceImageTable.has_value());
    auto sourceImageTable = std::make_shared<VulkanImageTable>(
        std::move(*materializedSourceImageTable));
    REQUIRE(sourceImageTable->slotCount() == 1u);
    REQUIRE(sourceImageTable->slot(0u).description.importIdentifier == 0u);
    REQUIRE(sourceImageTable->slot(0u).description.mipLevelCount == 2u);
    REQUIRE(sourceImageTable->slot(0u).isBound);

    RawImageViewEntry secondEntry{};
    secondEntry.baseMipLevel = 1u;
    secondEntry.baseArrayLayer = 1u;
    const std::vector<std::byte> imageViewTableBytes =
        buildImageViewTable({RawImageViewEntry{}, secondEntry});
    const auto imageViewTableView =
        CommandStreamImageViewHandleTableValidator::validate(imageViewTableBytes);
    REQUIRE(imageViewTableView.has_value());
    std::weak_ptr<const VulkanImageTable> sourceImageTableObserver = sourceImageTable;
    const auto imageViewTable =
        VulkanImageViewTable::createFromTable(sourceImageTable, *imageViewTableView);
    REQUIRE(imageViewTable.has_value());
    sourceImageTable.reset();
    REQUIRE_FALSE(sourceImageTableObserver.expired());
    REQUIRE(imageViewTable->slotCount() == 2u);
    REQUIRE(imageViewTable->imageView(0u) != VK_NULL_HANDLE);
    REQUIRE(imageViewTable->imageView(1u) != VK_NULL_HANDLE);
    REQUIRE(imageViewTable->description(1u).baseMipLevel == 1u);
    REQUIRE(imageViewTable->description(1u).baseArrayLayer == 1u);
}

TEST_CASE("Vulkan image-view table reports source relationship failures",
          "[vulkan][imageViewHandleTable]") {
    const auto harness = TestVulkanDeviceHarness::create();
    if (harness == nullptr) {
        SKIP("no usable Vulkan driver on this machine");
    }
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};

    const auto runMaterialization = [&](RawImageEntry imageEntry,
                                        RawImageViewEntry imageViewEntry) {
        const std::vector<std::byte> imageTableBytes = buildImageTable({imageEntry});
        const auto imageTableView = CommandStreamImageHandleTableValidator::validate(
            imageTableBytes);
        REQUIRE(imageTableView.has_value());
        auto materializedSourceImageTable =
            VulkanImageTable::createFromTable(vulkanContext, *imageTableView);
        REQUIRE(materializedSourceImageTable.has_value());
        auto sourceImageTable = std::make_shared<VulkanImageTable>(
            std::move(*materializedSourceImageTable));
        REQUIRE(sourceImageTable->slotCount() == 1u);
        const std::vector<std::byte> imageViewTableBytes =
            buildImageViewTable({imageViewEntry});
        const auto imageViewTableView =
            CommandStreamImageViewHandleTableValidator::validate(imageViewTableBytes);
        REQUIRE(imageViewTableView.has_value());
        return VulkanImageViewTable::createFromTable(sourceImageTable, *imageViewTableView);
    };

    SECTION("missing source owner") {
        const std::vector<std::byte> imageViewTableBytes =
            buildImageViewTable({RawImageViewEntry{}});
        const auto imageViewTableView =
            CommandStreamImageViewHandleTableValidator::validate(imageViewTableBytes);
        REQUIRE(imageViewTableView.has_value());
        const auto result = VulkanImageViewTable::createFromTable(nullptr,
                                                                  *imageViewTableView);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().error
                == VulkanImageViewTableCreationError::MissingSourceImageTable);
    }

    SECTION("source slot is outside the table") {
        RawImageViewEntry imageViewEntry{};
        imageViewEntry.imageSlot = 1u;
        const auto result = runMaterialization(RawImageEntry{}, imageViewEntry);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().error
                == VulkanImageViewTableCreationError::SourceImageSlotOutOfRange);
    }

    SECTION("source slot is an unbound imported image") {
        RawImageEntry imageEntry{};
        imageEntry.importIdentifier = 7u;
        imageEntry.usageFlags = 0u;
        const auto result = runMaterialization(imageEntry, RawImageViewEntry{});
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().error
                == VulkanImageViewTableCreationError::SourceImageSlotUnbound);
    }

    SECTION("view format differs from source format") {
        RawImageViewEntry imageViewEntry{};
        imageViewEntry.formatValue = 2u;
        const auto result = runMaterialization(RawImageEntry{}, imageViewEntry);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().error
                == VulkanImageViewTableCreationError::SourceImageFormatMismatch);
    }

    SECTION("view aspect differs from source format aspect") {
        RawImageViewEntry imageViewEntry{};
        imageViewEntry.aspectMaskValue = 0x2u;
        const auto result = runMaterialization(RawImageEntry{}, imageViewEntry);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().error
                == VulkanImageViewTableCreationError::SourceImageAspectMismatch);
    }

    SECTION("view kind differs from source kind") {
        RawImageViewEntry imageViewEntry{};
        imageViewEntry.imageViewKindValue = 1u;
        const auto result = runMaterialization(RawImageEntry{}, imageViewEntry);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().error
                == VulkanImageViewTableCreationError::SourceImageKindMismatch);
    }

    SECTION("mip range exceeds source") {
        RawImageViewEntry imageViewEntry{};
        imageViewEntry.baseMipLevel = 1u;
        imageViewEntry.mipLevelCount = 2u;
        const auto result = runMaterialization(RawImageEntry{}, imageViewEntry);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().error
                == VulkanImageViewTableCreationError::SourceImageSubresourceRangeOutOfBounds);
    }

    SECTION("array layer range exceeds source") {
        RawImageViewEntry imageViewEntry{};
        imageViewEntry.baseArrayLayer = 2u;
        const auto result = runMaterialization(RawImageEntry{}, imageViewEntry);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().error
                == VulkanImageViewTableCreationError::SourceImageSubresourceRangeOutOfBounds);
    }

    SECTION("transfer-only source cannot create a view") {
        RawImageEntry imageEntry{};
        imageEntry.usageFlags = 0x3u;
        const auto result = runMaterialization(imageEntry, RawImageViewEntry{});
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().error
                == VulkanImageViewTableCreationError::SourceImageUsageDoesNotSupportImageViews);
    }
}
