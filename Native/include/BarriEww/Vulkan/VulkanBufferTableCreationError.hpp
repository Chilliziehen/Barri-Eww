#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumeration; trivially thread-safe.
 * @brief Failure categories of load-time buffer materialization (BufferHandleTable →
 *        native buffers). Values are stable so the Java side can translate them into
 *        checked exceptions (§6.4).
 */
enum class VulkanBufferTableCreationError : std::uint32_t {
    /** vkCreateBuffer failed for a created entry. */
    BufferCreationFailed = 1,
    /** No device memory type satisfies the entry's memory kind requirements. */
    NoSuitableMemoryType = 2,
    /** vkAllocateMemory failed. */
    MemoryAllocationFailed = 3,
    /** vkBindBufferMemory failed. */
    MemoryBindingFailed = 4,
    /** vkMapMemory failed for a host-visible memory kind. */
    MemoryMappingFailed = 5,
};

} // namespace barrieww
