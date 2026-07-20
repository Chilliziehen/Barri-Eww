#include "BarriEww/Vulkan/CommandBufferRecorder.hpp"

#include <cstdint>
#include <cstring>

#include "BarriEww/CommandStream/CommandStreamOpcode.hpp"

namespace barrieww {

namespace {

/** Reads one little-endian value out of a payload span (offsets are payload-relative). */
template <typename ValueType>
ValueType readPayloadValue(std::span<const std::byte> payloadBytes, std::size_t byteOffset) {
    ValueType value{};
    std::memcpy(&value, payloadBytes.data() + byteOffset, sizeof value);
    return value;
}

} // namespace

/*
 * Recording walk (ADR-0003 D2). Algorithm principle: the stream is already flat,
 * barrier-resolved and validated, so recording is a single linear translation pass —
 * one opcode dispatch per command, emitting the corresponding vkCmd* into the target
 * buffer. The only checks performed are the ones that REQUIRE the materialized table
 * (slot bounds, bound-ness, copy ranges), because this pass is the first time payloads
 * are decoded next to the table (ADR-0002 D5: validation stays load-time; the recorded
 * buffer is then trusted every frame).
 *
 * Pseudocode (complete semantics):
 *   record(commandBuffer, streamView, bufferTable):
 *     vkBeginCommandBuffer(commandBuffer, no one-time flags)   // reusable (ADR-0003 D1)
 *     for commandIndex, command in enumerate(streamView):
 *       switch command.opcode:
 *         case CopyBuffer:
 *           (sourceSlot, destinationSlot, sourceOffset,
 *            destinationOffset, byteCount) <- decode pinned payload layout
 *           if either slot >= bufferTable.slotCount        -> BufferSlotOutOfRange
 *           if either slot is unbound (imported, v0.1)     -> UnboundImportedBuffer
 *           if offset + byteCount > slot.byteSize (either) -> CopyRangeOutOfBounds
 *           vkCmdCopyBuffer(commandBuffer, source, destination, one region)
 *         default                                          -> UnsupportedOpcode
 *     vkEndCommandBuffer(commandBuffer)
 */
std::expected<void, CommandBufferRecordingFailure>
CommandBufferRecorder::record(VkCommandBuffer commandBuffer,
                              const CommandStreamView& streamView,
                              const VulkanBufferTable& bufferTable) {
    using enum CommandBufferRecordingError;

    constexpr std::uint32_t noCommandIndex = UINT32_MAX;
    const auto fail = [](CommandBufferRecordingError error, std::uint32_t commandIndex,
                         VkResult vulkanResult) {
        return std::unexpected(CommandBufferRecordingFailure{
            error, commandIndex, static_cast<std::int32_t>(vulkanResult)});
    };

    VkCommandBufferBeginInfo commandBufferBeginInfo{};
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VkResult vulkanResult = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
    if (vulkanResult != VK_SUCCESS) {
        return fail(CommandBufferBeginFailed, noCommandIndex, vulkanResult);
    }

    std::uint32_t commandIndex = 0;
    for (const CommandStreamView::CommandRecord commandRecord : streamView) {
        switch (commandRecord.opcode) {
            case CommandStreamOpcode::CopyBuffer: {
                // Pinned CopyBuffer payload layout (mirrored by the Java
                // CommandStreamWriter.appendCopyBuffer).
                const auto sourceBufferSlot =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 0u);
                const auto destinationBufferSlot =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 4u);
                const auto sourceByteOffset =
                    readPayloadValue<std::uint64_t>(commandRecord.payloadBytes, 8u);
                const auto destinationByteOffset =
                    readPayloadValue<std::uint64_t>(commandRecord.payloadBytes, 16u);
                const auto copyByteCount =
                    readPayloadValue<std::uint64_t>(commandRecord.payloadBytes, 24u);

                if (sourceBufferSlot >= bufferTable.slotCount()
                    || destinationBufferSlot >= bufferTable.slotCount()) {
                    return fail(BufferSlotOutOfRange, commandIndex, VK_SUCCESS);
                }
                const VulkanBufferTable::BufferSlot& sourceSlot =
                    bufferTable.slot(sourceBufferSlot);
                const VulkanBufferTable::BufferSlot& destinationSlot =
                    bufferTable.slot(destinationBufferSlot);
                if (!sourceSlot.isBound || !destinationSlot.isBound) {
                    return fail(UnboundImportedBuffer, commandIndex, VK_SUCCESS);
                }
                if (sourceByteOffset + copyByteCount > sourceSlot.byteSize
                    || destinationByteOffset + copyByteCount > destinationSlot.byteSize) {
                    return fail(CopyRangeOutOfBounds, commandIndex, VK_SUCCESS);
                }

                VkBufferCopy bufferCopyRegion{};
                bufferCopyRegion.srcOffset = sourceByteOffset;
                bufferCopyRegion.dstOffset = destinationByteOffset;
                bufferCopyRegion.size = copyByteCount;
                vkCmdCopyBuffer(commandBuffer, sourceSlot.buffer, destinationSlot.buffer,
                                1u, &bufferCopyRegion);
                break;
            }
            default:
                return fail(UnsupportedOpcode, commandIndex, VK_SUCCESS);
        }
        ++commandIndex;
    }

    vulkanResult = vkEndCommandBuffer(commandBuffer);
    if (vulkanResult != VK_SUCCESS) {
        return fail(CommandBufferEndFailed, noCommandIndex, vulkanResult);
    }
    return {};
}

} // namespace barrieww
