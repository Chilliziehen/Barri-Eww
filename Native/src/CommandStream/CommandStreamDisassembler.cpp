#include "BarriEww/CommandStream/CommandStreamDisassembler.hpp"

#include <cstdint>
#include <format>

#include "BarriEww/CommandStream/CommandStreamModuleSectionType.hpp"
#include "BarriEww/CommandStream/CommandStreamOpcode.hpp"

namespace barrieww {

namespace {

/** The catalog name of an opcode (validated streams only contain assigned opcodes). */
const char* opcodeName(CommandStreamOpcode opcode) {
    switch (opcode) {
        case CommandStreamOpcode::BindGraphicsPipeline: return "BindGraphicsPipeline";
        case CommandStreamOpcode::BindComputePipeline: return "BindComputePipeline";
        case CommandStreamOpcode::BindVertexBuffers: return "BindVertexBuffers";
        case CommandStreamOpcode::BindIndexBuffer: return "BindIndexBuffer";
        case CommandStreamOpcode::PushConstants: return "PushConstants";
        case CommandStreamOpcode::SetViewport: return "SetViewport";
        case CommandStreamOpcode::SetScissor: return "SetScissor";
        case CommandStreamOpcode::PushBufferDeviceAddress: return "PushBufferDeviceAddress";
        case CommandStreamOpcode::Draw: return "Draw";
        case CommandStreamOpcode::DrawIndexed: return "DrawIndexed";
        case CommandStreamOpcode::DrawIndirect: return "DrawIndirect";
        case CommandStreamOpcode::DrawIndexedIndirect: return "DrawIndexedIndirect";
        case CommandStreamOpcode::DrawIndexedIndirectCount: return "DrawIndexedIndirectCount";
        case CommandStreamOpcode::Dispatch: return "Dispatch";
        case CommandStreamOpcode::DispatchIndirect: return "DispatchIndirect";
        case CommandStreamOpcode::BeginRendering: return "BeginRendering";
        case CommandStreamOpcode::EndRendering: return "EndRendering";
        case CommandStreamOpcode::ExecuteBarrierBatch: return "ExecuteBarrierBatch";
        case CommandStreamOpcode::CopyBuffer: return "CopyBuffer";
        case CommandStreamOpcode::CopyImage: return "CopyImage";
        case CommandStreamOpcode::BlitImage: return "BlitImage";
        case CommandStreamOpcode::ClearColorImage: return "ClearColorImage";
        case CommandStreamOpcode::ClearDepthStencilImage: return "ClearDepthStencilImage";
        case CommandStreamOpcode::ResolveImage: return "ResolveImage";
        case CommandStreamOpcode::CopyBufferToImage: return "CopyBufferToImage";
        case CommandStreamOpcode::CopyImageToBuffer: return "CopyImageToBuffer";
        case CommandStreamOpcode::PushDescriptorSet: return "PushDescriptorSet";
        case CommandStreamOpcode::DebugLabelBegin: return "DebugLabelBegin";
        case CommandStreamOpcode::DebugLabelEnd: return "DebugLabelEnd";
        case CommandStreamOpcode::InvokeOpaqueExtensionNode: return "InvokeOpaqueExtensionNode";
    }
    return "Unassigned";
}

/** The catalog name of a section type (validated modules only contain assigned types). */
const char* sectionTypeName(std::uint16_t sectionTypeValue) {
    switch (static_cast<CommandStreamModuleSectionType>(sectionTypeValue)) {
        case CommandStreamModuleSectionType::PipelineHandleTable: return "PipelineHandleTable";
        case CommandStreamModuleSectionType::BufferHandleTable: return "BufferHandleTable";
        case CommandStreamModuleSectionType::ImageHandleTable: return "ImageHandleTable";
        case CommandStreamModuleSectionType::ImageViewHandleTable: return "ImageViewHandleTable";
        case CommandStreamModuleSectionType::SamplerHandleTable: return "SamplerHandleTable";
        case CommandStreamModuleSectionType::ShaderModuleTable: return "ShaderModuleTable";
        case CommandStreamModuleSectionType::BarrierBatchTable: return "BarrierBatchTable";
        case CommandStreamModuleSectionType::RenderingTemplateTable: return "RenderingTemplateTable";
        case CommandStreamModuleSectionType::PushDescriptorTemplateTable: return "PushDescriptorTemplateTable";
        case CommandStreamModuleSectionType::LaneStream: return "LaneStream";
    }
    return "Unassigned";
}

/** Space-separated lowercase hex rendering of a byte span. */
std::string renderHexBytes(std::span<const std::byte> payloadBytes) {
    std::string hexText;
    hexText.reserve(payloadBytes.size() * 3u);
    for (std::size_t byteIndex = 0; byteIndex < payloadBytes.size(); ++byteIndex) {
        hexText += std::format("{}{:02x}", byteIndex == 0 ? "" : " ",
                               static_cast<unsigned>(payloadBytes[byteIndex]));
    }
    return hexText;
}

} // namespace

std::string CommandStreamDisassembler::disassemble(const CommandStreamView& streamView) {
    std::string renderedText = std::format(
        "BECS lane stream: laneIndex={} version={}.{} commandCount={} totalByteSize={} "
        "graphHash=0x{:016x}\n",
        streamView.laneIndex(), streamView.versionMajor(), streamView.versionMinor(),
        streamView.commandCount(), streamView.totalByteSize(), streamView.graphHash());

    std::uint32_t commandIndex = 0;
    for (const CommandStreamView::CommandRecord commandRecord : streamView) {
        renderedText += std::format(
            "  [{}] {} (0x{:04x}) byteSize={} payload: {}\n", commandIndex,
            opcodeName(commandRecord.opcode),
            static_cast<std::uint16_t>(commandRecord.opcode),
            commandRecord.payloadBytes.size() + 8u,
            renderHexBytes(commandRecord.payloadBytes));
        ++commandIndex;
    }
    return renderedText;
}

std::string CommandStreamDisassembler::disassemble(const CommandStreamModuleView& moduleView) {
    std::string renderedText = std::format(
        "BECS module: sectionCount={} laneStreamCount={} graphHash=0x{:016x}\n",
        moduleView.sectionEntries().size(), moduleView.laneStreamCount(),
        moduleView.graphHash());

    for (const CommandStreamModuleSectionEntry& sectionEntry : moduleView.sectionEntries()) {
        renderedText += std::format(
            "  section {} (0x{:04x}) index={} byteOffset={} byteSize={}\n",
            sectionTypeName(sectionEntry.sectionTypeValue), sectionEntry.sectionTypeValue,
            sectionEntry.sectionIndex, sectionEntry.byteOffset, sectionEntry.byteSize);
    }

    for (std::uint32_t laneIndex = 0; laneIndex < moduleView.laneStreamCount(); ++laneIndex) {
        renderedText += disassemble(moduleView.laneStream(laneIndex));
    }
    return renderedText;
}

} // namespace barrieww
