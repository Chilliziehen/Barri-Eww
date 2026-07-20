#include "BarriEww/Vulkan/CommandBufferRecorder.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamBufferBarrierRecord.hpp"
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

/** Whether a sync2 mask only uses bits the legacy sync1 calls can express (bit 0..31). */
bool isLegacyMappableMask(std::uint64_t synchronizationMask) {
    return (synchronizationMask >> 32u) == 0u;
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
 *   record(commandBuffer, streamView, bufferTable, barrierBatchTableView):
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
 *         case ExecuteBarrierBatch:
 *           batchSlot <- decode payload
 *           if no barrier table provided                   -> MissingBarrierBatchTable
 *           if batchSlot >= table.batchCount               -> BarrierBatchSlotOutOfRange
 *           // Legacy sync1 mapping (sync2 bit values below bit 32 equal the sync1
 *           // bits by design; per-call stage masks are the OR of the per-barrier
 *           // sync2 stage masks — coarser but correct):
 *           for each global/buffer record of the batch:
 *             if any mask uses bits above bit 31           -> UnmappableSynchronizationScope
 *             OR stages into call-level source/destination masks
 *             buffer records: resolve slot (bounds/bound/range checks as above,
 *                             range error -> BarrierRangeOutOfBounds)
 *           vkCmdPipelineBarrier(commandBuffer, orSourceStages, orDestinationStages,
 *                                0, globals, bufferBarriers, no image barriers)
 *         default                                          -> UnsupportedOpcode
 *     vkEndCommandBuffer(commandBuffer)
 */
std::expected<void, CommandBufferRecordingFailure>
CommandBufferRecorder::record(VkCommandBuffer commandBuffer,
                              const CommandStreamView& streamView,
                              const VulkanBufferTable& bufferTable,
                              const CommandStreamBarrierBatchTableView* barrierBatchTableView) {
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
            case CommandStreamOpcode::ExecuteBarrierBatch: {
                const auto barrierBatchSlot =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 0u);
                if (barrierBatchTableView == nullptr) {
                    return fail(MissingBarrierBatchTable, commandIndex, VK_SUCCESS);
                }
                if (barrierBatchSlot >= barrierBatchTableView->batchCount()) {
                    return fail(BarrierBatchSlotOutOfRange, commandIndex, VK_SUCCESS);
                }
                const CommandStreamBarrierBatchRecord batchRecord =
                    barrierBatchTableView->batch(barrierBatchSlot);

                VkPipelineStageFlags callSourceStageMask = 0;
                VkPipelineStageFlags callDestinationStageMask = 0;
                std::vector<VkMemoryBarrier> globalBarriers;
                globalBarriers.reserve(batchRecord.globalBarrierCount);
                std::vector<VkBufferMemoryBarrier> bufferBarriers;
                bufferBarriers.reserve(batchRecord.bufferBarrierCount);

                for (std::uint32_t barrierIndex = 0;
                     barrierIndex < batchRecord.globalBarrierCount; ++barrierIndex) {
                    const CommandStreamGlobalBarrierRecord globalRecord =
                        barrierBatchTableView->globalBarrier(barrierBatchSlot, barrierIndex);
                    if (!isLegacyMappableMask(globalRecord.sourceStageMask)
                        || !isLegacyMappableMask(globalRecord.sourceAccessMask)
                        || !isLegacyMappableMask(globalRecord.destinationStageMask)
                        || !isLegacyMappableMask(globalRecord.destinationAccessMask)) {
                        return fail(UnmappableSynchronizationScope, commandIndex, VK_SUCCESS);
                    }
                    callSourceStageMask |=
                        static_cast<VkPipelineStageFlags>(globalRecord.sourceStageMask);
                    callDestinationStageMask |=
                        static_cast<VkPipelineStageFlags>(globalRecord.destinationStageMask);
                    VkMemoryBarrier globalBarrier{};
                    globalBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                    globalBarrier.srcAccessMask =
                        static_cast<VkAccessFlags>(globalRecord.sourceAccessMask);
                    globalBarrier.dstAccessMask =
                        static_cast<VkAccessFlags>(globalRecord.destinationAccessMask);
                    globalBarriers.push_back(globalBarrier);
                }

                for (std::uint32_t barrierIndex = 0;
                     barrierIndex < batchRecord.bufferBarrierCount; ++barrierIndex) {
                    const CommandStreamBufferBarrierRecord bufferRecord =
                        barrierBatchTableView->bufferBarrier(barrierBatchSlot, barrierIndex);
                    if (!isLegacyMappableMask(bufferRecord.sourceStageMask)
                        || !isLegacyMappableMask(bufferRecord.sourceAccessMask)
                        || !isLegacyMappableMask(bufferRecord.destinationStageMask)
                        || !isLegacyMappableMask(bufferRecord.destinationAccessMask)) {
                        return fail(UnmappableSynchronizationScope, commandIndex, VK_SUCCESS);
                    }
                    if (bufferRecord.bufferSlot >= bufferTable.slotCount()) {
                        return fail(BufferSlotOutOfRange, commandIndex, VK_SUCCESS);
                    }
                    const VulkanBufferTable::BufferSlot& barrierTargetSlot =
                        bufferTable.slot(bufferRecord.bufferSlot);
                    if (!barrierTargetSlot.isBound) {
                        return fail(UnboundImportedBuffer, commandIndex, VK_SUCCESS);
                    }
                    const bool isWholeSize = bufferRecord.byteCount
                                             == CommandStreamBufferBarrierRecord::s_wholeByteCount;
                    if (isWholeSize
                            ? bufferRecord.byteOffset >= barrierTargetSlot.byteSize
                            : bufferRecord.byteOffset + bufferRecord.byteCount
                                  > barrierTargetSlot.byteSize) {
                        return fail(BarrierRangeOutOfBounds, commandIndex, VK_SUCCESS);
                    }

                    callSourceStageMask |=
                        static_cast<VkPipelineStageFlags>(bufferRecord.sourceStageMask);
                    callDestinationStageMask |=
                        static_cast<VkPipelineStageFlags>(bufferRecord.destinationStageMask);
                    VkBufferMemoryBarrier bufferBarrier{};
                    bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                    bufferBarrier.srcAccessMask =
                        static_cast<VkAccessFlags>(bufferRecord.sourceAccessMask);
                    bufferBarrier.dstAccessMask =
                        static_cast<VkAccessFlags>(bufferRecord.destinationAccessMask);
                    bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    bufferBarrier.buffer = barrierTargetSlot.buffer;
                    bufferBarrier.offset = bufferRecord.byteOffset;
                    bufferBarrier.size = isWholeSize ? VK_WHOLE_SIZE : bufferRecord.byteCount;
                    bufferBarriers.push_back(bufferBarrier);
                }

                vkCmdPipelineBarrier(
                    commandBuffer, callSourceStageMask, callDestinationStageMask, 0u,
                    static_cast<std::uint32_t>(globalBarriers.size()), globalBarriers.data(),
                    static_cast<std::uint32_t>(bufferBarriers.size()), bufferBarriers.data(),
                    0u, nullptr);
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
