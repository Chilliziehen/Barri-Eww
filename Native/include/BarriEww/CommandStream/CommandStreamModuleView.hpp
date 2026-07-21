#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamModuleHeader.hpp"
#include "BarriEww/CommandStream/CommandStreamModuleSectionEntry.hpp"
#include "BarriEww/CommandStream/CommandStreamModuleSectionType.hpp"
#include "BarriEww/CommandStream/CommandStreamView.hpp"

namespace barrieww {

class CommandStreamModuleValidator;

/**
 * @note ThreadSafety: Read-only view; safe to query and iterate concurrently from
 *       multiple threads (each lane's stream view is independent, matching the
 *       THREADED_RECORDING lane model). This blanket statement covers all accessors
 *       below (spec §2.6).
 * @brief Non-owning view over a VALIDATED BECS module container. Instances are created
 *        exclusively by CommandStreamModuleValidator; existence of a view implies the
 *        directory is structurally sound and every embedded lane stream passed stream
 *        validation (ADR-0002 D5), so all accessors are zero-check. The decoded
 *        directory and per-lane views are materialized once at load (slow path); the
 *        replay hot path only performs indexed lookups here (T0[1]).
 * @warning MemoryOwnership: The view BORROWS the underlying module bytes; in production
 *          these live in a Java-owned MemorySegment (Arena-managed, §6.3). The provider
 *          must keep the memory alive and unmodified for the lifetime of this view and
 *          of every CommandStreamView obtained from it.
 */
class CommandStreamModuleView {
    friend class CommandStreamModuleValidator;

public:
    /** The graph hash binding this module to its §8 artifact set (see class notes). */
    [[nodiscard]] std::uint64_t graphHash() const noexcept { return m_header.graphHash; }

    /** The number of embedded lane streams (see class notes). */
    [[nodiscard]] std::uint32_t laneStreamCount() const noexcept {
        return m_header.laneStreamCount;
    }

    /** The decoded directory entries in module order (see class notes). */
    [[nodiscard]] std::span<const CommandStreamModuleSectionEntry> sectionEntries() const noexcept {
        return m_sectionEntries;
    }

    /**
     * @note ThreadSafety: Thread-safe (read-only lookup, see class note).
     * @brief Finds one section's byte range by type and instance index.
     * @param sectionType The section type to look up.
     * @param sectionIndex The instance index (lane index for LaneStream sections;
     *        0 for singleton sections).
     * @return std::optional<std::span<const std::byte>> The borrowed byte range, or
     *         std::nullopt when the module has no such section.
     */
    [[nodiscard]] std::optional<std::span<const std::byte>>
    findSection(CommandStreamModuleSectionType sectionType,
                std::uint32_t sectionIndex = 0u) const noexcept {
        for (const CommandStreamModuleSectionEntry& sectionEntry : m_sectionEntries) {
            if (sectionEntry.sectionTypeValue == static_cast<std::uint16_t>(sectionType)
                && sectionEntry.sectionIndex == sectionIndex) {
                return m_moduleBytes.subspan(static_cast<std::size_t>(sectionEntry.byteOffset),
                                             static_cast<std::size_t>(sectionEntry.byteSize));
            }
        }
        return std::nullopt;
    }

    /**
     * @note ThreadSafety: Thread-safe (read-only, see class note).
     * @brief The validated stream view of one lane. Precondition: laneIndex <
     *        laneStreamCount() — the validator guarantees views exist exactly for lanes
     *        0..N-1, so the access is unchecked (T0[1]).
     * @param laneIndex The lane whose stream to access.
     * @return const CommandStreamView& The validated lane stream view.
     */
    [[nodiscard]] const CommandStreamView& laneStream(std::uint32_t laneIndex) const noexcept {
        return m_laneStreamViews[laneIndex];
    }

private:
    CommandStreamModuleView(std::span<const std::byte> moduleBytes,
                            const CommandStreamModuleHeader& header,
                            std::vector<CommandStreamModuleSectionEntry> sectionEntries,
                            std::vector<CommandStreamView> laneStreamViews) noexcept
        : m_moduleBytes(moduleBytes)
        , m_header(header)
        , m_sectionEntries(std::move(sectionEntries))
        , m_laneStreamViews(std::move(laneStreamViews)) {}

    std::span<const std::byte> m_moduleBytes;
    CommandStreamModuleHeader m_header;
    std::vector<CommandStreamModuleSectionEntry> m_sectionEntries;
    std::vector<CommandStreamView> m_laneStreamViews;
};

} // namespace barrieww
