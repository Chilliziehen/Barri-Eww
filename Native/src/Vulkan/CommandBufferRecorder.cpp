#include "BarriEww/Vulkan/CommandBufferRecorder.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#include "BarriEww/CommandStream/CommandStreamBufferBarrierRecord.hpp"
#include "BarriEww/CommandStream/CommandStreamBufferUsage.hpp"
#include "BarriEww/CommandStream/CommandStreamImageAspect.hpp"
#include "BarriEww/CommandStream/CommandStreamImageBarrierRecord.hpp"
#include "BarriEww/CommandStream/CommandStreamImageFormat.hpp"
#include "BarriEww/CommandStream/CommandStreamImageLayout.hpp"
#include "BarriEww/CommandStream/CommandStreamBarrierBatchTableView.hpp"
#include "BarriEww/CommandStream/CommandStreamOpcode.hpp"
#include "BarriEww/Vulkan/VulkanBufferTable.hpp"
#include "BarriEww/Vulkan/VulkanFormatMapping.hpp"
#include "BarriEww/Vulkan/VulkanImageTable.hpp"
#include "BarriEww/Vulkan/VulkanPipelineTable.hpp"

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
 *   record(commandBuffer, streamView, recordingInputs {bufferTable, ...}):
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
 *         case BindComputePipeline:
 *           slot checks against pipelineTable              -> Missing.../PipelineSlotOutOfRange
 *           vkCmdBindPipeline(COMPUTE); remember the slot's layout and push range
 *         case PushBufferDeviceAddress:
 *           requires a bound compute pipeline              -> NoBoundComputePipeline
 *           buffer slot checks; slot.deviceAddress != 0    -> MissingBufferDeviceAddress
 *           pushOffset + 8 within declared push range      -> PushConstantRangeExceeded
 *           vkCmdPushConstants(boundLayout, COMPUTE, offset, 8, &resolvedAddress)
 *           // The stream carries the SLOT; the address exists only after
 *           // materialization, so it is baked here at record time (load path).
 *         case Dispatch:
 *           requires a bound compute pipeline              -> NoBoundComputePipeline
 *           vkCmdDispatch(x, y, z)
 *         case DispatchIndirect:
 *           requires a bound compute pipeline              -> NoBoundComputePipeline
 *           buffer slot checks (bounds/bound as above)
 *           buffer must carry the Indirect usage bit       -> MissingIndirectUsage
 *           offset % 4 == 0                                -> MisalignedIndirectOffset
 *           offset + 12 within the buffer                  -> IndirectArgumentsOutOfBounds
 *           vkCmdDispatchIndirect(buffer, offset)
 *           // The GPU decides the workload at execution time; the CPU prerecorded
 *           // everything (the GPU-driven shape of ADR-0001).
 *         case ClearColorImage:
 *           image table present, slot bound; layout assigned; aspect usable;
 *           subresource range inside the image             -> ImageSubresourceOutOfRange
 *           vkCmdClearColorImage(image, mappedLayout, color, one range)
 *         case CopyImageToBuffer:
 *           image and buffer slot checks as above; layout/aspect as above;
 *           mip level and layer range inside the image, extent inside the mip
 *                                                          -> ImageSubresourceOutOfRange
 *           bufferOffset + texelSize * extent * layers within the buffer
 *                                                          -> CopyRangeOutOfBounds
 *           vkCmdCopyImageToBuffer(image, mappedLayout, buffer, one tightly packed region)
 *         default                                          -> UnsupportedOpcode
 *     vkEndCommandBuffer(commandBuffer)
 */
std::expected<void, CommandBufferRecordingFailure>
CommandBufferRecorder::record(VkCommandBuffer commandBuffer,
                              const CommandStreamView& streamView,
                              const CommandBufferRecordingInputs& recordingInputs) {
    using enum CommandBufferRecordingError;

    constexpr std::uint32_t noCommandIndex = UINT32_MAX;
    const auto fail = [](CommandBufferRecordingError error, std::uint32_t commandIndex,
                         VkResult vulkanResult) {
        return std::unexpected(CommandBufferRecordingFailure{
            error, commandIndex, static_cast<std::int32_t>(vulkanResult)});
    };

    // Unpack the bundled inputs into the names the walk below uses. bufferTable is
    // required (almost every command may reference a buffer); the rest stay optional
    // pointers checked at their use sites (the matching Missing...Table errors).
    if (recordingInputs.bufferTable == nullptr) {
        return fail(MissingBufferTable, noCommandIndex, VK_SUCCESS);
    }
    const VulkanBufferTable& bufferTable = *recordingInputs.bufferTable;
    const CommandStreamBarrierBatchTableView* barrierBatchTableView =
        recordingInputs.barrierBatchTableView;
    const VulkanPipelineTable* pipelineTable = recordingInputs.pipelineTable;
    const VulkanImageTable* imageTable = recordingInputs.imageTable;

    VkCommandBufferBeginInfo commandBufferBeginInfo{};
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VkResult vulkanResult = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
    if (vulkanResult != VK_SUCCESS) {
        return fail(CommandBufferBeginFailed, noCommandIndex, vulkanResult);
    }

    // Recording-walk pipeline state: which compute pipeline is currently bound and the
    // push range it declared (needed by push constant emission).
    bool hasBoundComputePipeline = false;
    VkPipelineLayout boundComputePipelineLayout = VK_NULL_HANDLE;
    std::uint32_t boundPushConstantByteSize = 0;

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

                std::vector<VkImageMemoryBarrier> imageBarriers;
                imageBarriers.reserve(batchRecord.imageBarrierCount);
                for (std::uint32_t barrierIndex = 0;
                     barrierIndex < batchRecord.imageBarrierCount; ++barrierIndex) {
                    const CommandStreamImageBarrierRecord imageRecord =
                        barrierBatchTableView->imageBarrier(barrierBatchSlot, barrierIndex);
                    if (!isLegacyMappableMask(imageRecord.sourceStageMask)
                        || !isLegacyMappableMask(imageRecord.sourceAccessMask)
                        || !isLegacyMappableMask(imageRecord.destinationStageMask)
                        || !isLegacyMappableMask(imageRecord.destinationAccessMask)) {
                        return fail(UnmappableSynchronizationScope, commandIndex, VK_SUCCESS);
                    }
                    if (imageTable == nullptr) {
                        return fail(MissingImageTable, commandIndex, VK_SUCCESS);
                    }
                    if (imageRecord.imageSlot >= imageTable->slotCount()) {
                        return fail(ImageSlotOutOfRange, commandIndex, VK_SUCCESS);
                    }
                    const VulkanImageTable::ImageSlot& barrierImageSlot =
                        imageTable->slot(imageRecord.imageSlot);
                    if (!barrierImageSlot.isBound) {
                        return fail(UnboundImportedImage, commandIndex, VK_SUCCESS);
                    }

                    callSourceStageMask |=
                        static_cast<VkPipelineStageFlags>(imageRecord.sourceStageMask);
                    callDestinationStageMask |=
                        static_cast<VkPipelineStageFlags>(imageRecord.destinationStageMask);
                    VkImageMemoryBarrier imageBarrier{};
                    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    imageBarrier.srcAccessMask =
                        static_cast<VkAccessFlags>(imageRecord.sourceAccessMask);
                    imageBarrier.dstAccessMask =
                        static_cast<VkAccessFlags>(imageRecord.destinationAccessMask);
                    imageBarrier.oldLayout = mapCommandStreamImageLayout(
                        static_cast<CommandStreamImageLayout>(imageRecord.oldLayoutValue));
                    imageBarrier.newLayout = mapCommandStreamImageLayout(
                        static_cast<CommandStreamImageLayout>(imageRecord.newLayoutValue));
                    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    imageBarrier.image = barrierImageSlot.image;
                    imageBarrier.subresourceRange.aspectMask =
                        mapCommandStreamImageAspectMask(imageRecord.aspectMaskValue);
                    imageBarrier.subresourceRange.baseMipLevel = imageRecord.baseMipLevel;
                    imageBarrier.subresourceRange.levelCount =
                        imageRecord.mipLevelCount
                                == CommandStreamImageBarrierRecord::s_remainingCount
                            ? VK_REMAINING_MIP_LEVELS
                            : imageRecord.mipLevelCount;
                    imageBarrier.subresourceRange.baseArrayLayer = imageRecord.baseArrayLayer;
                    imageBarrier.subresourceRange.layerCount =
                        imageRecord.arrayLayerCount
                                == CommandStreamImageBarrierRecord::s_remainingCount
                            ? VK_REMAINING_ARRAY_LAYERS
                            : imageRecord.arrayLayerCount;
                    imageBarriers.push_back(imageBarrier);
                }

                vkCmdPipelineBarrier(
                    commandBuffer, callSourceStageMask, callDestinationStageMask, 0u,
                    static_cast<std::uint32_t>(globalBarriers.size()), globalBarriers.data(),
                    static_cast<std::uint32_t>(bufferBarriers.size()), bufferBarriers.data(),
                    static_cast<std::uint32_t>(imageBarriers.size()), imageBarriers.data());
                break;
            }
            case CommandStreamOpcode::BindComputePipeline: {
                const auto pipelineSlot =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 0u);
                if (pipelineTable == nullptr) {
                    return fail(MissingPipelineTable, commandIndex, VK_SUCCESS);
                }
                if (pipelineSlot >= pipelineTable->slotCount()) {
                    return fail(PipelineSlotOutOfRange, commandIndex, VK_SUCCESS);
                }
                const VulkanPipelineTable::PipelineSlot& boundSlot =
                    pipelineTable->slot(pipelineSlot);
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                  boundSlot.pipeline);
                hasBoundComputePipeline = true;
                boundComputePipelineLayout = boundSlot.pipelineLayout;
                boundPushConstantByteSize = boundSlot.pushConstantByteSize;
                break;
            }
            case CommandStreamOpcode::PushBufferDeviceAddress: {
                // Pinned payload: +0 bufferSlot u32, +4 pushConstantByteOffset u32.
                const auto bufferSlot =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 0u);
                const auto pushConstantByteOffset =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 4u);
                if (!hasBoundComputePipeline) {
                    return fail(NoBoundComputePipeline, commandIndex, VK_SUCCESS);
                }
                if (bufferSlot >= bufferTable.slotCount()) {
                    return fail(BufferSlotOutOfRange, commandIndex, VK_SUCCESS);
                }
                const VulkanBufferTable::BufferSlot& addressedSlot =
                    bufferTable.slot(bufferSlot);
                if (!addressedSlot.isBound) {
                    return fail(UnboundImportedBuffer, commandIndex, VK_SUCCESS);
                }
                if (addressedSlot.deviceAddress == 0u) {
                    return fail(MissingBufferDeviceAddress, commandIndex, VK_SUCCESS);
                }
                if (pushConstantByteOffset + sizeof(VkDeviceAddress)
                    > boundPushConstantByteSize) {
                    return fail(PushConstantRangeExceeded, commandIndex, VK_SUCCESS);
                }
                // The address exists only after materialization; baking it here keeps
                // the stream pure data (slot indirection) while the prerecorded buffer
                // carries the resolved value (ADR-0003: decided at load, not per frame).
                vkCmdPushConstants(commandBuffer, boundComputePipelineLayout,
                                   VK_SHADER_STAGE_COMPUTE_BIT, pushConstantByteOffset,
                                   sizeof(VkDeviceAddress), &addressedSlot.deviceAddress);
                break;
            }
            case CommandStreamOpcode::Dispatch: {
                const auto groupCountX =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 0u);
                const auto groupCountY =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 4u);
                const auto groupCountZ =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 8u);
                if (!hasBoundComputePipeline) {
                    return fail(NoBoundComputePipeline, commandIndex, VK_SUCCESS);
                }
                vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
                break;
            }
            case CommandStreamOpcode::DispatchIndirect: {
                // Pinned payload: +0 bufferSlot u32, +4 reserved, +8 bufferOffset u64.
                const auto bufferSlot =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 0u);
                const auto bufferOffset =
                    readPayloadValue<std::uint64_t>(commandRecord.payloadBytes, 8u);
                if (!hasBoundComputePipeline) {
                    return fail(NoBoundComputePipeline, commandIndex, VK_SUCCESS);
                }
                if (bufferSlot >= bufferTable.slotCount()) {
                    return fail(BufferSlotOutOfRange, commandIndex, VK_SUCCESS);
                }
                const VulkanBufferTable::BufferSlot& argumentsSlot =
                    bufferTable.slot(bufferSlot);
                if (!argumentsSlot.isBound) {
                    return fail(UnboundImportedBuffer, commandIndex, VK_SUCCESS);
                }
                if ((argumentsSlot.neutralUsageFlags
                     & static_cast<std::uint32_t>(CommandStreamBufferUsage::Indirect))
                    == 0u) {
                    return fail(MissingIndirectUsage, commandIndex, VK_SUCCESS);
                }
                if (bufferOffset % 4u != 0u) {
                    return fail(MisalignedIndirectOffset, commandIndex, VK_SUCCESS);
                }
                // VkDispatchIndirectCommand is three u32 workgroup counts (12 bytes).
                if (bufferOffset + 12u > argumentsSlot.byteSize) {
                    return fail(IndirectArgumentsOutOfBounds, commandIndex, VK_SUCCESS);
                }
                vkCmdDispatchIndirect(commandBuffer, argumentsSlot.buffer, bufferOffset);
                break;
            }
            case CommandStreamOpcode::ClearColorImage: {
                // Pinned payload: imageSlot, layout, RGBA floats, aspect, subresource.
                const auto imageSlot =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 0u);
                const auto imageLayoutValue =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 4u);
                const auto aspectMaskValue =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 24u);
                const auto baseMipLevel =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 28u);
                const auto mipLevelCount =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 32u);
                const auto baseArrayLayer =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 36u);
                const auto arrayLayerCount =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 40u);

                if (imageTable == nullptr) {
                    return fail(MissingImageTable, commandIndex, VK_SUCCESS);
                }
                if (imageSlot >= imageTable->slotCount()) {
                    return fail(ImageSlotOutOfRange, commandIndex, VK_SUCCESS);
                }
                const VulkanImageTable::ImageSlot& clearTargetSlot = imageTable->slot(imageSlot);
                if (!clearTargetSlot.isBound) {
                    return fail(UnboundImportedImage, commandIndex, VK_SUCCESS);
                }
                if (!isAssignedCommandStreamImageLayout(imageLayoutValue)) {
                    return fail(UnknownImageLayoutValue, commandIndex, VK_SUCCESS);
                }
                if (!isUsableCommandStreamImageAspectMask(aspectMaskValue)) {
                    return fail(UnknownImageAspectMask, commandIndex, VK_SUCCESS);
                }
                constexpr std::uint32_t remainingCount =
                    CommandStreamImageBarrierRecord::s_remainingCount;
                const auto& imageDescription = clearTargetSlot.description;
                if (mipLevelCount == 0u || arrayLayerCount == 0u
                    || baseMipLevel >= imageDescription.mipLevelCount
                    || baseArrayLayer >= imageDescription.arrayLayerCount
                    || (mipLevelCount != remainingCount
                        && baseMipLevel + mipLevelCount > imageDescription.mipLevelCount)
                    || (arrayLayerCount != remainingCount
                        && baseArrayLayer + arrayLayerCount
                               > imageDescription.arrayLayerCount)) {
                    return fail(ImageSubresourceOutOfRange, commandIndex, VK_SUCCESS);
                }

                VkClearColorValue clearColorValue{};
                clearColorValue.float32[0] =
                    readPayloadValue<float>(commandRecord.payloadBytes, 8u);
                clearColorValue.float32[1] =
                    readPayloadValue<float>(commandRecord.payloadBytes, 12u);
                clearColorValue.float32[2] =
                    readPayloadValue<float>(commandRecord.payloadBytes, 16u);
                clearColorValue.float32[3] =
                    readPayloadValue<float>(commandRecord.payloadBytes, 20u);
                VkImageSubresourceRange subresourceRange{};
                subresourceRange.aspectMask =
                    mapCommandStreamImageAspectMask(aspectMaskValue);
                subresourceRange.baseMipLevel = baseMipLevel;
                subresourceRange.levelCount = mipLevelCount == remainingCount
                                                  ? VK_REMAINING_MIP_LEVELS
                                                  : mipLevelCount;
                subresourceRange.baseArrayLayer = baseArrayLayer;
                subresourceRange.layerCount = arrayLayerCount == remainingCount
                                                  ? VK_REMAINING_ARRAY_LAYERS
                                                  : arrayLayerCount;
                vkCmdClearColorImage(
                    commandBuffer, clearTargetSlot.image,
                    mapCommandStreamImageLayout(
                        static_cast<CommandStreamImageLayout>(imageLayoutValue)),
                    &clearColorValue, 1u, &subresourceRange);
                break;
            }
            case CommandStreamOpcode::CopyImageToBuffer: {
                // Pinned payload: imageSlot, bufferSlot, layout, aspect, mip, layers,
                // bufferOffset, tightly packed extent.
                const auto imageSlot =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 0u);
                const auto bufferSlot =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 4u);
                const auto imageLayoutValue =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 8u);
                const auto aspectMaskValue =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 12u);
                const auto mipLevel =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 16u);
                const auto baseArrayLayer =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 20u);
                const auto arrayLayerCount =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 24u);
                const auto bufferByteOffset =
                    readPayloadValue<std::uint64_t>(commandRecord.payloadBytes, 32u);
                const auto copyWidth =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 40u);
                const auto copyHeight =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 44u);
                const auto copyDepth =
                    readPayloadValue<std::uint32_t>(commandRecord.payloadBytes, 48u);

                if (imageTable == nullptr) {
                    return fail(MissingImageTable, commandIndex, VK_SUCCESS);
                }
                if (imageSlot >= imageTable->slotCount()) {
                    return fail(ImageSlotOutOfRange, commandIndex, VK_SUCCESS);
                }
                const VulkanImageTable::ImageSlot& copySourceSlot = imageTable->slot(imageSlot);
                if (!copySourceSlot.isBound) {
                    return fail(UnboundImportedImage, commandIndex, VK_SUCCESS);
                }
                if (bufferSlot >= bufferTable.slotCount()) {
                    return fail(BufferSlotOutOfRange, commandIndex, VK_SUCCESS);
                }
                const VulkanBufferTable::BufferSlot& copyTargetSlot =
                    bufferTable.slot(bufferSlot);
                if (!copyTargetSlot.isBound) {
                    return fail(UnboundImportedBuffer, commandIndex, VK_SUCCESS);
                }
                if (!isAssignedCommandStreamImageLayout(imageLayoutValue)) {
                    return fail(UnknownImageLayoutValue, commandIndex, VK_SUCCESS);
                }
                if (!isUsableCommandStreamImageAspectMask(aspectMaskValue)) {
                    return fail(UnknownImageAspectMask, commandIndex, VK_SUCCESS);
                }

                const auto& imageDescription = copySourceSlot.description;
                const std::uint32_t mipWidth =
                    std::max(1u, imageDescription.width >> mipLevel);
                const std::uint32_t mipHeight =
                    std::max(1u, imageDescription.height >> mipLevel);
                const std::uint32_t mipDepth =
                    std::max(1u, imageDescription.depth >> mipLevel);
                if (mipLevel >= imageDescription.mipLevelCount || arrayLayerCount == 0u
                    || baseArrayLayer + arrayLayerCount > imageDescription.arrayLayerCount
                    || copyWidth == 0u || copyHeight == 0u || copyDepth == 0u
                    || copyWidth > mipWidth || copyHeight > mipHeight
                    || copyDepth > mipDepth) {
                    return fail(ImageSubresourceOutOfRange, commandIndex, VK_SUCCESS);
                }
                const std::uint64_t copyByteCount =
                    static_cast<std::uint64_t>(commandStreamImageFormatTexelByteSize(
                        static_cast<CommandStreamImageFormat>(imageDescription.formatValue)))
                    * copyWidth * copyHeight * copyDepth * arrayLayerCount;
                if (bufferByteOffset + copyByteCount > copyTargetSlot.byteSize) {
                    return fail(CopyRangeOutOfBounds, commandIndex, VK_SUCCESS);
                }

                VkBufferImageCopy copyRegion{};
                copyRegion.bufferOffset = bufferByteOffset;
                copyRegion.bufferRowLength = 0u;   // tightly packed
                copyRegion.bufferImageHeight = 0u; // tightly packed
                copyRegion.imageSubresource.aspectMask =
                    mapCommandStreamImageAspectMask(aspectMaskValue);
                copyRegion.imageSubresource.mipLevel = mipLevel;
                copyRegion.imageSubresource.baseArrayLayer = baseArrayLayer;
                copyRegion.imageSubresource.layerCount = arrayLayerCount;
                copyRegion.imageExtent = VkExtent3D{copyWidth, copyHeight, copyDepth};
                vkCmdCopyImageToBuffer(
                    commandBuffer, copySourceSlot.image,
                    mapCommandStreamImageLayout(
                        static_cast<CommandStreamImageLayout>(imageLayoutValue)),
                    copyTargetSlot.buffer, 1u, &copyRegion);
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
