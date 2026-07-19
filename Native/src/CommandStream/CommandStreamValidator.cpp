#include "BarriEww/CommandStream/CommandStreamValidator.hpp"

#include <bit>
#include <cstdint>
#include <cstring>

#include "BarriEww/CommandStream/CommandStreamHeader.hpp"
#include "BarriEww/CommandStream/CommandStreamOpcode.hpp"

namespace barrieww {

// ADR-0002 D1 fixes the wire format to little-endian with zero byte swapping on either
// side, so a big-endian build of this replayer would silently misread every field.
static_assert(std::endian::native == std::endian::little,
              "BECS replay requires a little-endian target (ADR-0002 D1)");

/*
 * Validation walk (ADR-0002 D5). Algorithm principle: a single linear pass over the
 * command chain proves structural integrity because every command carries its own size;
 * validity is equivalent to (a) a well-formed header, (b) every link of the size chain
 * staying inside the stream and aligned, and (c) the walk ending exactly at
 * totalByteSize after exactly commandCount steps.
 *
 * Pseudocode (complete semantics):
 *   validate(streamBytes):
 *     if length(streamBytes) < headerSize            -> StreamTooSmall
 *     if baseAddress(streamBytes) % 8 != 0           -> MisalignedStreamBase
 *     header <- decode 32-byte header
 *     if header.magicBytes != "BECS"                 -> InvalidMagic
 *     if header.versionMajor != supportedMajor
 *        or header.versionMinor > supportedMinor     -> UnsupportedVersion
 *     if header.totalByteSize != length(streamBytes) -> StreamSizeMismatch
 *     walkOffset <- headerSize
 *     for commandIndex in 0 .. header.commandCount - 1:
 *       if walkOffset + commandHeaderSize > totalByteSize      -> TruncatedCommand
 *       (opcodeValue, reservedFlags, commandByteSize) <- decode command header
 *       if commandByteSize < commandHeaderSize
 *          or commandByteSize % 8 != 0                         -> MisalignedCommandSize
 *       if reservedFlags != 0                                  -> NonZeroReservedFlags
 *       if not isAssignedCommandStreamOpcode(opcodeValue)      -> UnknownOpcode
 *       if walkOffset + commandByteSize > totalByteSize        -> TruncatedCommand
 *       walkOffset <- walkOffset + commandByteSize
 *     if walkOffset != totalByteSize                           -> CommandChainMismatch
 *     return view over (streamBytes, header)
 */
std::expected<CommandStreamView, CommandStreamValidationFailure>
CommandStreamValidator::validate(std::span<const std::byte> streamBytes) noexcept {
    using enum CommandStreamValidationError;

    constexpr std::size_t headerByteSize = sizeof(CommandStreamHeader);
    constexpr std::size_t commandHeaderByteSize = 8u;

    const auto fail = [](CommandStreamValidationError error, std::uint64_t byteOffset) {
        return std::unexpected(CommandStreamValidationFailure{error, byteOffset});
    };

    if (streamBytes.size() < headerByteSize) {
        return fail(StreamTooSmall, 0u);
    }
    if (reinterpret_cast<std::uintptr_t>(streamBytes.data())
            % CommandStreamHeader::s_commandAlignment != 0u) {
        return fail(MisalignedStreamBase, 0u);
    }

    CommandStreamHeader header{};
    std::memcpy(&header, streamBytes.data(), headerByteSize);

    if (std::memcmp(header.magicBytes, CommandStreamHeader::s_expectedMagicBytes,
                    sizeof header.magicBytes) != 0) {
        return fail(InvalidMagic, 0u);
    }
    if (header.versionMajor != CommandStreamHeader::s_currentVersionMajor
        || header.versionMinor > CommandStreamHeader::s_currentVersionMinor) {
        return fail(UnsupportedVersion, 4u);
    }
    if (header.totalByteSize != streamBytes.size()) {
        return fail(StreamSizeMismatch, 16u);
    }

    std::uint64_t walkOffset = headerByteSize;
    for (std::uint32_t commandIndex = 0; commandIndex < header.commandCount; ++commandIndex) {
        if (walkOffset + commandHeaderByteSize > header.totalByteSize) {
            return fail(TruncatedCommand, walkOffset);
        }

        std::uint16_t rawOpcodeValue = 0;
        std::uint16_t reservedFlags = 0;
        std::uint32_t commandByteSize = 0;
        std::memcpy(&rawOpcodeValue, streamBytes.data() + walkOffset, sizeof rawOpcodeValue);
        std::memcpy(&reservedFlags, streamBytes.data() + walkOffset + 2u, sizeof reservedFlags);
        std::memcpy(&commandByteSize, streamBytes.data() + walkOffset + 4u, sizeof commandByteSize);

        if (commandByteSize < commandHeaderByteSize
            || commandByteSize % CommandStreamHeader::s_commandAlignment != 0u) {
            return fail(MisalignedCommandSize, walkOffset);
        }
        if (reservedFlags != 0u) {
            return fail(NonZeroReservedFlags, walkOffset);
        }
        if (!isAssignedCommandStreamOpcode(rawOpcodeValue)) {
            return fail(UnknownOpcode, walkOffset);
        }
        if (walkOffset + commandByteSize > header.totalByteSize) {
            return fail(TruncatedCommand, walkOffset);
        }

        walkOffset += commandByteSize;
    }

    if (walkOffset != header.totalByteSize) {
        return fail(CommandChainMismatch, walkOffset);
    }

    return CommandStreamView{streamBytes, header};
}

} // namespace barrieww
