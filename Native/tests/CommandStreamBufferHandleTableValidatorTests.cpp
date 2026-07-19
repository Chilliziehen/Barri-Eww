#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamBufferHandleTableValidator.hpp"
#include "BarriEww/CommandStream/CommandStreamBufferMemoryKind.hpp"
#include "BarriEww/CommandStream/CommandStreamBufferUsage.hpp"
#include "BarriEww/CommandStream/CommandStreamHandleTableValidationError.hpp"

using barrieww::CommandStreamBufferHandleTableValidator;
using barrieww::CommandStreamBufferMemoryKind;
using barrieww::CommandStreamBufferUsage;
using barrieww::CommandStreamHandleTableValidationError;

namespace {

/** Appends one little-endian value to a byte vector. */
template <typename ValueType>
void appendValue(std::vector<std::byte>& tableBytes, ValueType value) {
    const std::size_t writeOffset = tableBytes.size();
    tableBytes.resize(writeOffset + sizeof value);
    std::memcpy(tableBytes.data() + writeOffset, &value, sizeof value);
}

/** Builds a table with the given raw entries (each as five fields). */
struct RawBufferEntry {
    std::uint64_t byteSize;
    std::uint32_t usageFlags;
    std::uint32_t memoryKindValue;
    std::uint32_t importIdentifier;
    std::uint32_t reservedFlags;
};

std::vector<std::byte> buildTable(const std::vector<RawBufferEntry>& rawEntries,
                                  std::uint32_t headerReservedFlags = 0u) {
    std::vector<std::byte> tableBytes;
    appendValue(tableBytes, static_cast<std::uint32_t>(rawEntries.size()));
    appendValue(tableBytes, headerReservedFlags);
    for (const RawBufferEntry& rawEntry : rawEntries) {
        appendValue(tableBytes, rawEntry.byteSize);
        appendValue(tableBytes, rawEntry.usageFlags);
        appendValue(tableBytes, rawEntry.memoryKindValue);
        appendValue(tableBytes, rawEntry.importIdentifier);
        appendValue(tableBytes, rawEntry.reservedFlags);
    }
    return tableBytes;
}

} // namespace

TEST_CASE("Buffer table validator accepts created and imported entries",
          "[commandStream][bufferHandleTable]") {
    const std::vector<std::byte> tableBytes = buildTable({
        {256u, 0x1u, 2u, 0u, 0u},   // created staging buffer
        {256u, 0x2u, 1u, 0u, 0u},   // created device-local buffer
        {0u, 0u, 0u, 77u, 0u},      // imported buffer, identifier 77
    });

    const auto validationResult =
        CommandStreamBufferHandleTableValidator::validate(tableBytes);
    REQUIRE(validationResult.has_value());
    REQUIRE(validationResult->entryCount() == 3u);

    const auto stagingEntry = validationResult->entry(0u);
    REQUIRE(stagingEntry.byteSize == 256u);
    REQUIRE(stagingEntry.usageFlags
            == static_cast<std::uint32_t>(CommandStreamBufferUsage::TransferSource));
    REQUIRE(stagingEntry.memoryKindValue
            == static_cast<std::uint32_t>(
                   CommandStreamBufferMemoryKind::HostVisiblePersistentMapped));
    REQUIRE_FALSE(stagingEntry.isImported());

    const auto importedEntry = validationResult->entry(2u);
    REQUIRE(importedEntry.isImported());
    REQUIRE(importedEntry.importIdentifier == 77u);
}

TEST_CASE("Buffer table validator rejects malformed tables with the precise failure",
          "[commandStream][bufferHandleTable]") {
    SECTION("table smaller than the header") {
        const std::vector<std::byte> tinyBytes(4u, std::byte{0});
        const auto validationResult =
            CommandStreamBufferHandleTableValidator::validate(tinyBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::TableTooSmall);
    }

    SECTION("size not matching the entry count") {
        std::vector<std::byte> tableBytes = buildTable({{64u, 0x1u, 1u, 0u, 0u}});
        tableBytes.resize(tableBytes.size() + 8u);
        const auto validationResult =
            CommandStreamBufferHandleTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::TableSizeMismatch);
    }

    SECTION("non-zero header reserved flags") {
        const std::vector<std::byte> tableBytes = buildTable({}, 5u);
        const auto validationResult =
            CommandStreamBufferHandleTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::NonZeroReservedFlags);
    }

    SECTION("unassigned usage bit") {
        const std::vector<std::byte> tableBytes =
            buildTable({{64u, 0x8000u, 1u, 0u, 0u}});
        const auto validationResult =
            CommandStreamBufferHandleTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::UnknownUsageFlags);
        REQUIRE(validationResult.error().byteOffset == 8u);
    }

    SECTION("unassigned memory kind") {
        const std::vector<std::byte> tableBytes = buildTable({{64u, 0x1u, 9u, 0u, 0u}});
        const auto validationResult =
            CommandStreamBufferHandleTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::UnknownMemoryKind);
    }

    SECTION("created entry with zero byte size") {
        const std::vector<std::byte> tableBytes = buildTable({{0u, 0x1u, 1u, 0u, 0u}});
        const auto validationResult =
            CommandStreamBufferHandleTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::InvalidCreatedBufferDescription);
    }

    SECTION("created entry with memory kind None") {
        const std::vector<std::byte> tableBytes = buildTable({{64u, 0x1u, 0u, 0u, 0u}});
        const auto validationResult =
            CommandStreamBufferHandleTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::InvalidCreatedBufferDescription);
    }

    SECTION("imported entry carrying a concrete memory kind") {
        const std::vector<std::byte> tableBytes = buildTable({{0u, 0u, 1u, 42u, 0u}});
        const auto validationResult =
            CommandStreamBufferHandleTableValidator::validate(tableBytes);
        REQUIRE_FALSE(validationResult.has_value());
        REQUIRE(validationResult.error().error
                == CommandStreamHandleTableValidationError::InvalidImportedBufferDescription);
    }
}
