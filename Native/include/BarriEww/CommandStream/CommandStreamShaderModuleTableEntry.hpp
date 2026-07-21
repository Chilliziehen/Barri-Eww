#pragma once

#include <cstddef>
#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief One 16-byte ShaderModuleTable directory entry: the byte range of one SPIR-V
 *        blob within the table section (offset from the table base, 8-byte aligned).
 *        Blob regions may be shared between entries (deduplication is legitimate).
 *        Layout is the wire format, pinned by the static_asserts below (ADR-0002 D1).
 */
struct CommandStreamShaderModuleTableEntry {
    std::uint64_t blobByteOffset;
    std::uint64_t blobByteSize;
};

static_assert(sizeof(CommandStreamShaderModuleTableEntry) == 16u,
              "ShaderModuleTable entries must be exactly 16 bytes (schema v0.1)");
static_assert(offsetof(CommandStreamShaderModuleTableEntry, blobByteSize) == 8u);

} // namespace barrieww
