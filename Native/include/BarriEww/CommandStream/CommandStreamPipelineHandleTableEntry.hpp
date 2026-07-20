#pragma once

#include <cstddef>
#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Plain value type; no internal synchronization.
 * @brief One 16-byte PipelineHandleTable entry (schema v0.1, compute pipelines). The
 *        entry point is the fixed convention "main" (our compiler emits it), and the
 *        v0.1 pipeline layout is push constants only — resources are reached through
 *        buffer device addresses pushed at record time (PushBufferDeviceAddress) or,
 *        later, the bindless heap; classic per-pipeline descriptor set layouts are
 *        deliberately absent. Layout is the wire format, pinned below (ADR-0002 D1).
 */
struct CommandStreamPipelineHandleTableEntry {
    std::uint32_t pipelineKindValue;
    std::uint32_t shaderModuleSlot;
    std::uint32_t pushConstantByteSize;
    std::uint32_t reservedFlags;
};

static_assert(sizeof(CommandStreamPipelineHandleTableEntry) == 16u,
              "PipelineHandleTable entries must be exactly 16 bytes (schema v0.1)");
static_assert(offsetof(CommandStreamPipelineHandleTableEntry, shaderModuleSlot) == 4u);
static_assert(offsetof(CommandStreamPipelineHandleTableEntry, pushConstantByteSize) == 8u);
static_assert(offsetof(CommandStreamPipelineHandleTableEntry, reservedFlags) == 12u);

} // namespace barrieww
