#include "BarriEww/Vulkan/VulkanImageViewTable.hpp"

#include <cstdint>
#include <utility>

#include "BarriEww/CommandStream/CommandStreamImageFormat.hpp"
#include "BarriEww/CommandStream/CommandStreamImageUsage.hpp"
#include "BarriEww/CommandStream/CommandStreamImageViewKind.hpp"
#include "BarriEww/Vulkan/VulkanFormatMapping.hpp"

namespace barrieww {

namespace {

/**
 * @note ThreadSafety: Thread-safe (pure constant expression).
 * @brief Whether the source image usage contains a v0.1 purpose that permits image views.
 * @param usageFlags The assigned neutral source image usage mask.
 * @return bool True when the mask contains Sampled, Storage, ColorAttachment or
 *         DepthStencilAttachment.
 */
[[nodiscard]] constexpr bool
hasImageViewCompatibleUsage(std::uint32_t usageFlags) noexcept {
    constexpr std::uint32_t imageViewCompatibleUsageMask =
        static_cast<std::uint32_t>(CommandStreamImageUsage::Sampled)
        | static_cast<std::uint32_t>(CommandStreamImageUsage::Storage)
        | static_cast<std::uint32_t>(CommandStreamImageUsage::ColorAttachment)
        | static_cast<std::uint32_t>(CommandStreamImageUsage::DepthStencilAttachment);
    return (usageFlags & imageViewCompatibleUsageMask) != 0u;
}

/**
 * @note ThreadSafety: Thread-safe (pure constant expression).
 * @brief Maps an assigned neutral image-view kind onto a matching non-array Vulkan type.
 * @param imageViewKindValue The assigned raw ImageViewHandleTable kind value.
 * @return VkImageViewType The corresponding Vulkan non-array view type.
 */
[[nodiscard]] constexpr VkImageViewType
mapImageViewKind(std::uint32_t imageViewKindValue) noexcept {
    switch (static_cast<CommandStreamImageViewKind>(imageViewKindValue)) {
        case CommandStreamImageViewKind::OneDimensional: return VK_IMAGE_VIEW_TYPE_1D;
        case CommandStreamImageViewKind::TwoDimensional: return VK_IMAGE_VIEW_TYPE_2D;
        case CommandStreamImageViewKind::ThreeDimensional: return VK_IMAGE_VIEW_TYPE_3D;
    }
    return VK_IMAGE_VIEW_TYPE_2D;
}

/**
 * @note ThreadSafety: Thread-safe (pure constant expression).
 * @brief Determines whether an explicit positive range fits a limit without overflow.
 * @param baseIndex The first index in the requested range.
 * @param count The explicit number of requested elements.
 * @param limit The exclusive source-resource element limit.
 * @return bool True when [baseIndex, baseIndex + count) lies inside [0, limit).
 */
[[nodiscard]] constexpr bool
isRangeWithinLimit(std::uint32_t baseIndex, std::uint32_t count,
                   std::uint32_t limit) noexcept {
    return baseIndex < limit && count <= limit - baseIndex;
}

} // namespace

/*
 * Image-view materialization (load path). Algorithm principle: a first linear preflight
 * joins every validated view entry with source metadata and rejects all cross-table
 * errors before allocating output storage or calling the Vulkan driver. A second linear
 * pass creates VkImageView objects into a partial RAII table. This separates cheap
 * validation from expensive side effects and prevents a late malformed entry from
 * forcing creation and rollback of every earlier view.
 *
 * Pseudocode (complete semantics):
 *   createFromTable(sourceImageTableOwner, imageViewTable):
 *     if sourceImageTableOwner is null                    -> MissingSourceImageTable
 *     for each imageViewSlot:                             // preflight only
 *       entry <- imageViewTable.entry(imageViewSlot)
 *       if entry.imageSlot not in sourceImageTable        -> SourceImageSlotOutOfRange
 *       sourceSlot <- sourceImageTable.slot(entry.imageSlot)
 *       if sourceSlot not bound                           -> SourceImageSlotUnbound
 *       if entry format/aspect/kind mismatch source       -> matching stable error
 *       if mip or layer range exceeds source              -> SourceImageSubresourceRangeOutOfBounds
 *       if source usage cannot create a view              -> SourceImageUsageDoesNotSupportImageViews
 *     allocate dense handle and description arrays
 *     for each imageViewSlot:                             // Vulkan side effects
 *       view <- vkCreateImageView(sourceSlot.image, entry) -> ImageViewCreationFailed
 *       append view and entry to partial table
 *     return partial table retaining sourceImageTableOwner
 */
std::expected<VulkanImageViewTable, VulkanImageViewTableCreationFailure>
VulkanImageViewTable::createFromTable(
    std::shared_ptr<const VulkanImageTable> sourceImageTable,
    const CommandStreamImageViewHandleTableView& tableView) {
    using enum VulkanImageViewTableCreationError;

    const auto fail = [](VulkanImageViewTableCreationError error,
                         std::uint32_t imageViewSlotIndex,
                         std::uint32_t sourceImageSlotIndex,
                         VkResult vulkanResult) {
        return std::unexpected(VulkanImageViewTableCreationFailure{
            error, imageViewSlotIndex, sourceImageSlotIndex,
            static_cast<std::int32_t>(vulkanResult)});
    };

    if (sourceImageTable == nullptr) {
        return fail(MissingSourceImageTable, 0u, 0u, VK_SUCCESS);
    }

    for (std::uint32_t imageViewSlotIndex = 0u;
         imageViewSlotIndex < tableView.entryCount(); ++imageViewSlotIndex) {
        const CommandStreamImageViewHandleTableEntry entry =
            tableView.entry(imageViewSlotIndex);
        if (entry.imageSlot >= sourceImageTable->slotCount()) {
            return fail(SourceImageSlotOutOfRange, imageViewSlotIndex, entry.imageSlot,
                        VK_SUCCESS);
        }

        const VulkanImageTable::ImageSlot& sourceImageSlot =
            sourceImageTable->slot(entry.imageSlot);
        const CommandStreamImageHandleTableEntry& sourceDescription =
            sourceImageSlot.description;
        if (!sourceImageSlot.isBound) {
            return fail(SourceImageSlotUnbound, imageViewSlotIndex, entry.imageSlot,
                        VK_SUCCESS);
        }
        if (entry.formatValue != sourceDescription.formatValue) {
            return fail(SourceImageFormatMismatch, imageViewSlotIndex, entry.imageSlot,
                        VK_SUCCESS);
        }
        const auto sourceImageFormat =
            static_cast<CommandStreamImageFormat>(sourceDescription.formatValue);
        if (!isCompatibleCommandStreamImageAspectMask(sourceImageFormat,
                                                       entry.aspectMaskValue)) {
            return fail(SourceImageAspectMismatch, imageViewSlotIndex, entry.imageSlot,
                        VK_SUCCESS);
        }
        if (entry.imageViewKindValue != sourceDescription.imageKindValue) {
            return fail(SourceImageKindMismatch, imageViewSlotIndex, entry.imageSlot,
                        VK_SUCCESS);
        }
        if (!isRangeWithinLimit(entry.baseMipLevel, entry.mipLevelCount,
                                sourceDescription.mipLevelCount)
            || !isRangeWithinLimit(entry.baseArrayLayer, entry.arrayLayerCount,
                                   sourceDescription.arrayLayerCount)) {
            return fail(SourceImageSubresourceRangeOutOfBounds, imageViewSlotIndex,
                        entry.imageSlot, VK_SUCCESS);
        }
        if (!hasImageViewCompatibleUsage(sourceDescription.usageFlags)) {
            return fail(SourceImageUsageDoesNotSupportImageViews, imageViewSlotIndex,
                        entry.imageSlot, VK_SUCCESS);
        }
    }

    const VkDevice logicalDevice = sourceImageTable->logicalDevice();
    VulkanImageViewTable partialTable{std::move(sourceImageTable), logicalDevice};
    partialTable.m_imageViews.reserve(tableView.entryCount());
    partialTable.m_descriptions.reserve(tableView.entryCount());

    for (std::uint32_t imageViewSlotIndex = 0u;
         imageViewSlotIndex < tableView.entryCount(); ++imageViewSlotIndex) {
        const CommandStreamImageViewHandleTableEntry entry =
            tableView.entry(imageViewSlotIndex);
        const VulkanImageTable::ImageSlot& sourceImageSlot =
            partialTable.m_sourceImageTable->slot(entry.imageSlot);
        const auto sourceImageFormat =
            static_cast<CommandStreamImageFormat>(sourceImageSlot.description.formatValue);

        VkImageViewCreateInfo imageViewCreateInfo{};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.image = sourceImageSlot.image;
        imageViewCreateInfo.viewType = mapImageViewKind(entry.imageViewKindValue);
        imageViewCreateInfo.format = mapCommandStreamImageFormat(sourceImageFormat);
        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.subresourceRange.aspectMask =
            mapCommandStreamImageAspectMask(entry.aspectMaskValue);
        imageViewCreateInfo.subresourceRange.baseMipLevel = entry.baseMipLevel;
        imageViewCreateInfo.subresourceRange.levelCount = entry.mipLevelCount;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = entry.baseArrayLayer;
        imageViewCreateInfo.subresourceRange.layerCount = entry.arrayLayerCount;

        VkImageView imageView = VK_NULL_HANDLE;
        const VkResult vulkanResult = vkCreateImageView(logicalDevice, &imageViewCreateInfo,
                                                        nullptr, &imageView);
        if (vulkanResult != VK_SUCCESS) {
            return fail(ImageViewCreationFailed, imageViewSlotIndex, entry.imageSlot,
                        vulkanResult);
        }
        partialTable.m_imageViews.push_back(imageView);
        partialTable.m_descriptions.push_back(entry);
    }

    return partialTable;
}

VulkanImageViewTable::~VulkanImageViewTable() {
    for (VkImageView imageView : m_imageViews) {
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_logicalDevice, imageView, nullptr);
        }
    }
}

} // namespace barrieww
