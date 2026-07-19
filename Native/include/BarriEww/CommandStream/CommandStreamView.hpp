#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <span>

#include "BarriEww/CommandStream/CommandStreamHeader.hpp"
#include "BarriEww/CommandStream/CommandStreamOpcode.hpp"

namespace barrieww {

class CommandStreamValidator;

/**
 * @note ThreadSafety: Read-only view; safe to iterate concurrently from multiple threads.
 *       This blanket statement covers all accessors below (spec §2.6).
 * @brief Non-owning, read-only view over a VALIDATED BECS lane stream. Instances are
 *        created exclusively by CommandStreamValidator (D5: a view existing implies the
 *        stream passed full structural validation), which is why iteration performs no
 *        checks — the replay hot path stays zero-check in release (T0[1]).
 * @warning MemoryOwnership: The view BORROWS the underlying bytes; in production these
 *          live in a Java-owned MemorySegment (Arena-managed, §6.3). The provider must
 *          keep the memory alive and unmodified for the lifetime of the view and of any
 *          iterator obtained from it.
 */
class CommandStreamView {
    friend class CommandStreamValidator;

public:
    /**
     * @note ThreadSafety: Plain value type; trivially thread-safe.
     * @brief One decoded command record: opcode plus a borrowed span of its payload
     *        bytes (everything after the 8-byte command header).
     */
    struct CommandRecord {
        CommandStreamOpcode opcode;
        std::span<const std::byte> payloadBytes;
    };

    /**
     * @note ThreadSafety: Independent iterator instances are safe on separate threads;
     *       a single instance must not be shared without external synchronization.
     * @brief Forward iterator decoding one command per step. Decoding is two aligned
     *        loads plus a span subview; no allocation, no branching beyond loop control.
     */
    class ConstIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = CommandRecord;
        using difference_type = std::ptrdiff_t;

        ConstIterator() noexcept = default;

        /** Decodes the command at the current offset (see class notes). */
        [[nodiscard]] CommandRecord operator*() const noexcept {
            std::uint16_t rawOpcodeValue = 0;
            std::uint32_t commandByteSize = 0;
            std::memcpy(&rawOpcodeValue, m_streamBytes.data() + m_byteOffset, sizeof rawOpcodeValue);
            std::memcpy(&commandByteSize, m_streamBytes.data() + m_byteOffset + 4u, sizeof commandByteSize);
            return CommandRecord{
                static_cast<CommandStreamOpcode>(rawOpcodeValue),
                m_streamBytes.subspan(m_byteOffset + 8u, commandByteSize - 8u)};
        }

        ConstIterator& operator++() noexcept {
            std::uint32_t commandByteSize = 0;
            std::memcpy(&commandByteSize, m_streamBytes.data() + m_byteOffset + 4u, sizeof commandByteSize);
            m_byteOffset += commandByteSize;
            return *this;
        }

        ConstIterator operator++(int) noexcept {
            ConstIterator previousState = *this;
            ++(*this);
            return previousState;
        }

        [[nodiscard]] bool operator==(const ConstIterator& other) const noexcept {
            return m_byteOffset == other.m_byteOffset;
        }

    private:
        friend class CommandStreamView;

        ConstIterator(std::span<const std::byte> streamBytes, std::size_t byteOffset) noexcept
            : m_streamBytes(streamBytes), m_byteOffset(byteOffset) {}

        std::span<const std::byte> m_streamBytes;
        std::size_t m_byteOffset = 0;
    };

    /** The lane index this stream was compiled for (see class notes). */
    [[nodiscard]] std::uint32_t laneIndex() const noexcept { return m_header.laneIndex; }

    /** The number of commands in the stream (see class notes). */
    [[nodiscard]] std::uint32_t commandCount() const noexcept { return m_header.commandCount; }

    /** The graph hash binding this stream to its §8 artifact set (see class notes). */
    [[nodiscard]] std::uint64_t graphHash() const noexcept { return m_header.graphHash; }

    /** The stream format version pair (see class notes). */
    [[nodiscard]] std::uint16_t versionMajor() const noexcept { return m_header.versionMajor; }

    /** The stream format minor version (see class notes). */
    [[nodiscard]] std::uint16_t versionMinor() const noexcept { return m_header.versionMinor; }

    /** Iterator at the first command, immediately after the 32-byte header. */
    [[nodiscard]] ConstIterator begin() const noexcept {
        return ConstIterator{m_streamBytes, sizeof(CommandStreamHeader)};
    }

    /** Past-the-end iterator at totalByteSize. */
    [[nodiscard]] ConstIterator end() const noexcept {
        return ConstIterator{m_streamBytes, static_cast<std::size_t>(m_header.totalByteSize)};
    }

private:
    CommandStreamView(std::span<const std::byte> streamBytes,
                      const CommandStreamHeader& header) noexcept
        : m_streamBytes(streamBytes), m_header(header) {}

    std::span<const std::byte> m_streamBytes;
    CommandStreamHeader m_header;
};

} // namespace barrieww
