#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumeration; trivially thread-safe.
 * @brief Failure categories of load-time image materialization (ImageHandleTable →
 *        native images). Values are stable so the Java side can translate them into
 *        checked exceptions (§6.4).
 */
enum class VulkanImageTableCreationError : std::uint32_t {
    /** vkCreateImage failed for a created entry. */
    ImageCreationFailed = 1,
    /** No device memory type satisfies the device-local requirement. */
    NoSuitableMemoryType = 2,
    /** vkAllocateMemory failed. */
    MemoryAllocationFailed = 3,
    /** vkBindImageMemory failed. */
    MemoryBindingFailed = 4,
};

} // namespace barrieww
