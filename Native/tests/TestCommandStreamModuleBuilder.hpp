#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamModuleHeader.hpp"
#include "BarriEww/CommandStream/CommandStreamModuleSectionEntry.hpp"

namespace barrieww::testing {

/**
 * @note ThreadSafety: Not thread-safe; each test builds its own instance.
 * @brief Test-only BECS module container builder: lays out header, directory and
 *        8-byte-aligned section blobs. Production modules are written by the Java bake
 *        (ADR-0001); this exists to exercise the C++ module validator without a JVM.
 */
class TestCommandStreamModuleBuilder {
public:
    explicit TestCommandStreamModuleBuilder(std::uint64_t graphHash = 0xABCDEF0123456789ull)
        : m_graphHash(graphHash) {}

    /** Queues one section; contentBytes are placed in call order, padded to 8 bytes. */
    void addSection(std::uint16_t sectionTypeValue,
                    std::uint32_t sectionIndex,
                    std::vector<std::byte> contentBytes,
                    std::uint16_t reservedFlags = 0u) {
        m_pendingSections.push_back(PendingSection{sectionTypeValue, sectionIndex,
                                                   reservedFlags, std::move(contentBytes)});
    }

    /** Finalizes header + directory + blobs and returns the module bytes. */
    [[nodiscard]] std::vector<std::byte> build(bool overrideLaneStreamCount = false,
                                               std::uint32_t forcedLaneStreamCount = 0u) {
        constexpr std::size_t headerByteSize = sizeof(CommandStreamModuleHeader);
        constexpr std::size_t entryByteSize = sizeof(CommandStreamModuleSectionEntry);
        const std::size_t directoryEndOffset =
            headerByteSize + m_pendingSections.size() * entryByteSize;

        // Assign aligned offsets in declaration order.
        std::vector<CommandStreamModuleSectionEntry> sectionEntries;
        std::size_t nextSectionOffset = directoryEndOffset;
        for (const PendingSection& pendingSection : m_pendingSections) {
            sectionEntries.push_back(CommandStreamModuleSectionEntry{
                pendingSection.sectionTypeValue, pendingSection.reservedFlags,
                pendingSection.sectionIndex, nextSectionOffset,
                pendingSection.contentBytes.size()});
            nextSectionOffset += alignUp(pendingSection.contentBytes.size());
        }

        std::uint32_t laneStreamCount = 0;
        for (const CommandStreamModuleSectionEntry& sectionEntry : sectionEntries) {
            if (sectionEntry.sectionTypeValue == 0x0020u) {
                ++laneStreamCount;
            }
        }

        CommandStreamModuleHeader header{};
        std::memcpy(header.magicBytes, CommandStreamModuleHeader::s_expectedMagicBytes, 4u);
        header.versionMajor = CommandStreamModuleHeader::s_currentVersionMajor;
        header.versionMinor = CommandStreamModuleHeader::s_currentVersionMinor;
        header.sectionCount = static_cast<std::uint32_t>(sectionEntries.size());
        header.laneStreamCount =
            overrideLaneStreamCount ? forcedLaneStreamCount : laneStreamCount;
        header.totalByteSize = nextSectionOffset;
        header.graphHash = m_graphHash;

        std::vector<std::byte> moduleBytes(nextSectionOffset, std::byte{0});
        std::memcpy(moduleBytes.data(), &header, headerByteSize);
        std::memcpy(moduleBytes.data() + headerByteSize, sectionEntries.data(),
                    sectionEntries.size() * entryByteSize);
        for (std::size_t sectionPosition = 0; sectionPosition < m_pendingSections.size();
             ++sectionPosition) {
            const PendingSection& pendingSection = m_pendingSections[sectionPosition];
            std::memcpy(moduleBytes.data() + sectionEntries[sectionPosition].byteOffset,
                        pendingSection.contentBytes.data(),
                        pendingSection.contentBytes.size());
        }
        return moduleBytes;
    }

    /** Writes a little-endian value at an absolute byte offset (corruption hook). */
    template <typename ValueType>
    static void overwriteValueAt(std::vector<std::byte>& moduleBytes,
                                 std::size_t byteOffset,
                                 ValueType newValue) {
        std::memcpy(moduleBytes.data() + byteOffset, &newValue, sizeof newValue);
    }

private:
    struct PendingSection {
        std::uint16_t sectionTypeValue;
        std::uint32_t sectionIndex;
        std::uint16_t reservedFlags;
        std::vector<std::byte> contentBytes;
    };

    static std::size_t alignUp(std::size_t byteCount) {
        return (byteCount + 7u) & ~static_cast<std::size_t>(7u);
    }

    std::vector<PendingSection> m_pendingSections;
    std::uint64_t m_graphHash;
};

} // namespace barrieww::testing
