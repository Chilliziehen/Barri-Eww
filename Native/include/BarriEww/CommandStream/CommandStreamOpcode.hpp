#pragma once

#include <cstdint>

namespace barrieww {

/**
 * @note ThreadSafety: Enumerations and the constexpr predicates below are stateless and
 *       fully thread-safe.
 * @brief BECS command opcodes, v0.1 catalog (ADR-0002 appendix A). The 16-bit opcode
 *        space is partitioned by backend (ADR-0002 D4): 0x0000–0x0FFF backend-neutral
 *        core, 0x1000–0x1FFF Vulkan-specific, 0x2000–0x2FFF DX12 (reserved),
 *        0x3000–0x3FFF Metal (reserved), 0xF000–0xFFFF controlled-extension callouts.
 */
enum class CommandStreamOpcode : std::uint16_t {
    // Core range: state binding.
    BindGraphicsPipeline = 0x0001,
    BindComputePipeline = 0x0002,
    BindVertexBuffers = 0x0003,
    BindIndexBuffer = 0x0004,
    PushConstants = 0x0005,
    SetViewport = 0x0006,
    SetScissor = 0x0007,
    /**
     * Pushes a buffer's device address (8 bytes) into the bound pipeline's push
     * constants. The stream carries the SLOT; the load-time recorder resolves it to
     * the actual VkDeviceAddress (addresses only exist after materialization, so this
     * keeps bake artifacts pure data — ADR-0002 D2 slot indirection).
     */
    PushBufferDeviceAddress = 0x0008,

    // Core range: draws.
    Draw = 0x0010,
    DrawIndexed = 0x0011,
    DrawIndirect = 0x0012,
    DrawIndexedIndirect = 0x0013,
    DrawIndexedIndirectCount = 0x0014,

    // Core range: compute.
    Dispatch = 0x0020,
    DispatchIndirect = 0x0021,

    // Core range: rendering scope and barriers.
    BeginRendering = 0x0030,
    EndRendering = 0x0031,
    ExecuteBarrierBatch = 0x0032,

    // Core range: transfer operations.
    CopyBuffer = 0x0040,
    CopyImage = 0x0041,
    BlitImage = 0x0042,
    ClearColorImage = 0x0043,
    ClearDepthStencilImage = 0x0044,
    ResolveImage = 0x0045,
    CopyBufferToImage = 0x0046,
    CopyImageToBuffer = 0x0047,

    // Vulkan range.
    PushDescriptorSet = 0x1000,
    DebugLabelBegin = 0x1001,
    DebugLabelEnd = 0x1002,

    // Controlled-extension range (§8.3 opaque nodes).
    InvokeOpaqueExtensionNode = 0xF000,
};

/**
 * @note ThreadSafety: Thread-safe (pure constant expression).
 * @brief Whether the raw opcode value is an assigned opcode of the v0.1 catalog. The
 *        load-time validator rejects unassigned values: with the exact-major /
 *        additive-minor version policy (ADR-0002 D6) a replayer knows every opcode of
 *        every stream it accepts, so an unknown value is always a corrupt or
 *        incompatible stream.
 * @param rawOpcodeValue The 16-bit opcode value read from a stream.
 * @return bool True when the value names an assigned opcode.
 */
[[nodiscard]] constexpr bool isAssignedCommandStreamOpcode(std::uint16_t rawOpcodeValue) noexcept {
    switch (static_cast<CommandStreamOpcode>(rawOpcodeValue)) {
        case CommandStreamOpcode::BindGraphicsPipeline:
        case CommandStreamOpcode::BindComputePipeline:
        case CommandStreamOpcode::BindVertexBuffers:
        case CommandStreamOpcode::BindIndexBuffer:
        case CommandStreamOpcode::PushConstants:
        case CommandStreamOpcode::SetViewport:
        case CommandStreamOpcode::SetScissor:
        case CommandStreamOpcode::PushBufferDeviceAddress:
        case CommandStreamOpcode::Draw:
        case CommandStreamOpcode::DrawIndexed:
        case CommandStreamOpcode::DrawIndirect:
        case CommandStreamOpcode::DrawIndexedIndirect:
        case CommandStreamOpcode::DrawIndexedIndirectCount:
        case CommandStreamOpcode::Dispatch:
        case CommandStreamOpcode::DispatchIndirect:
        case CommandStreamOpcode::BeginRendering:
        case CommandStreamOpcode::EndRendering:
        case CommandStreamOpcode::ExecuteBarrierBatch:
        case CommandStreamOpcode::CopyBuffer:
        case CommandStreamOpcode::CopyImage:
        case CommandStreamOpcode::BlitImage:
        case CommandStreamOpcode::ClearColorImage:
        case CommandStreamOpcode::ClearDepthStencilImage:
        case CommandStreamOpcode::ResolveImage:
        case CommandStreamOpcode::CopyBufferToImage:
        case CommandStreamOpcode::CopyImageToBuffer:
        case CommandStreamOpcode::PushDescriptorSet:
        case CommandStreamOpcode::DebugLabelBegin:
        case CommandStreamOpcode::DebugLabelEnd:
        case CommandStreamOpcode::InvokeOpaqueExtensionNode:
            return true;
    }
    return false;
}

} // namespace barrieww
