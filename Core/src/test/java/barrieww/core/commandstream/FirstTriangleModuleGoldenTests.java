package barrieww.core.commandstream;

import static org.junit.jupiter.api.Assertions.assertArrayEquals;
import static org.junit.jupiter.api.Assertions.assertEquals;

import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.Optional;
import org.junit.jupiter.api.Test;

/**
 * @note ThreadSafety: JUnit creates a fresh instance per test method; no shared state.
 * The FIRST FULL-FRAME module arbiter: assembles the complete first-triangle scenario
 * (BDA vertex + readback buffers, color target image + view, both shader stages, the
 * graphics pipeline, both barrier batches, the rendering template and the lane-0
 * command stream) through the real write-side composition path and asserts byte-exact
 * agreement with the committed cross-language golden. The C++ side validates the same
 * fixture, materializes every section and replays it on the GPU
 * (GraphicsModuleExecutionTests), closing the bake -> load -> pixels loop over one
 * module blob.
 */
class FirstTriangleModuleGoldenTests {

    private static final long s_goldenGraphHash = 0x1122334455667788L;

    /** Reads one committed SPIR-V fixture as a heap segment. */
    private static MemorySegment readShaderBlob(String fileName) throws Exception {
        Path blobFilePath = Path.of(System.getProperty("barrieww.testDataDirectory"),
                "Shaders", fileName);
        return MemorySegment.ofArray(Files.readAllBytes(blobFilePath));
    }

    /** Emits one table writer's bytes into a fresh native segment. */
    private static MemorySegment emitSection(Arena testArena, long requiredByteSize,
                                             java.util.function.Function<MemorySegment,
                                                     Long> writeFunction) {
        MemorySegment sectionSegment = testArena.allocate(requiredByteSize, 8);
        assertEquals(requiredByteSize, writeFunction.apply(sectionSegment));
        return sectionSegment;
    }

    /** Records the lane-0 first-triangle stream into a fresh native segment. */
    private static MemorySegment recordFirstTriangleLaneStream(Arena testArena) {
        MemorySegment streamSegment = testArena.allocate(264, 8);
        CommandStreamWriter streamWriter =
                new CommandStreamWriter(streamSegment, 0, s_goldenGraphHash);
        streamWriter.appendExecuteBarrierBatch(0); // Undefined -> ColorAttachment
        streamWriter.appendBeginRendering(0);
        streamWriter.appendBindGraphicsPipeline(0);
        streamWriter.appendPushBufferDeviceAddress(0, 0); // vertex buffer address
        streamWriter.appendSetViewport(0.0f, 0.0f, 8.0f, 8.0f, 0.0f, 1.0f);
        streamWriter.appendSetScissor(0, 0, 8, 8);
        streamWriter.appendDraw(3, 1, 0, 0);
        streamWriter.appendEndRendering();
        streamWriter.appendExecuteBarrierBatch(1); // ColorAttachment -> TransferSource
        streamWriter.appendCopyImageToBuffer(0, 1, CommandStreamImageLayout.TRANSFER_SOURCE,
                0x1, 0, 0, 1, 0, 8, 8, 1);
        assertEquals(264, streamWriter.finish());
        return streamSegment;
    }

    /** Assembles the golden module through the real write-side composition path. */
    static MemorySegment writeGoldenModule(Arena testArena) throws Exception {
        CommandStreamBufferHandleTableWriter bufferTableWriter =
                new CommandStreamBufferHandleTableWriter();
        bufferTableWriter.addCreatedBuffer(24, CommandStreamBufferUsage.s_deviceAddress,
                CommandStreamBufferMemoryKind.HOST_VISIBLE_PERSISTENT_MAPPED);
        bufferTableWriter.addCreatedBuffer(256, CommandStreamBufferUsage.s_transferDestination,
                CommandStreamBufferMemoryKind.HOST_VISIBLE_READBACK);

        CommandStreamImageHandleTableWriter imageTableWriter =
                new CommandStreamImageHandleTableWriter();
        imageTableWriter.addCreatedImage(CommandStreamImageKind.TWO_DIMENSIONAL,
                CommandStreamImageFormat.R8G8B8A8_UNORM, 8, 8, 1, 1, 1, 1,
                CommandStreamImageUsage.s_transferSource
                        | CommandStreamImageUsage.s_colorAttachment);

        CommandStreamImageViewHandleTableWriter imageViewTableWriter =
                new CommandStreamImageViewHandleTableWriter();
        imageViewTableWriter.addImageView(0, CommandStreamImageViewKind.TWO_DIMENSIONAL,
                CommandStreamImageFormat.R8G8B8A8_UNORM, 0x1, 0, 1, 0, 1);

        CommandStreamShaderModuleTableWriter shaderTableWriter =
                new CommandStreamShaderModuleTableWriter();
        shaderTableWriter.addShaderBlob(readShaderBlob("VertexPullByDeviceAddress.vert.spv"));
        shaderTableWriter.addShaderBlob(readShaderBlob("SolidGreen.frag.spv"));

        CommandStreamGraphicsPipelineTableWriter graphicsPipelineTableWriter =
                new CommandStreamGraphicsPipelineTableWriter();
        graphicsPipelineTableWriter.addPipeline(0, 1, 8,
                CommandStreamPrimitiveTopology.TRIANGLE_LIST,
                List.of(CommandStreamImageFormat.R8G8B8A8_UNORM), Optional.empty(),
                CommandStreamCullMode.NONE, CommandStreamFrontFace.COUNTER_CLOCKWISE);

        CommandStreamBarrierBatchTableWriter barrierTableWriter =
                new CommandStreamBarrierBatchTableWriter();
        barrierTableWriter.addBatch(List.of(), List.of(),
                List.of(new CommandStreamBarrierBatchTableWriter.ImageBarrierDescription(
                        CommandStreamPipelineStage.s_topOfPipe, 0,
                        CommandStreamPipelineStage.s_colorAttachmentOutput,
                        CommandStreamMemoryAccess.s_colorAttachmentWrite,
                        0, 0x1, 0, 2, 0,
                        CommandStreamBarrierBatchTableWriter.s_remainingCount, 0,
                        CommandStreamBarrierBatchTableWriter.s_remainingCount)));
        barrierTableWriter.addBatch(List.of(), List.of(),
                List.of(new CommandStreamBarrierBatchTableWriter.ImageBarrierDescription(
                        CommandStreamPipelineStage.s_colorAttachmentOutput,
                        CommandStreamMemoryAccess.s_colorAttachmentWrite,
                        CommandStreamPipelineStage.s_transfer,
                        CommandStreamMemoryAccess.s_transferRead,
                        0, 0x1, 2, 5, 0,
                        CommandStreamBarrierBatchTableWriter.s_remainingCount, 0,
                        CommandStreamBarrierBatchTableWriter.s_remainingCount)));

        CommandStreamRenderingTemplateTableWriter renderingTemplateTableWriter =
                new CommandStreamRenderingTemplateTableWriter();
        renderingTemplateTableWriter.addTemplate(
                List.of(new CommandStreamRenderingTemplateTableWriter.AttachmentDescription(
                        0, CommandStreamImageLayout.COLOR_ATTACHMENT,
                        CommandStreamAttachmentLoadOp.CLEAR,
                        CommandStreamAttachmentStoreOp.STORE, 0.0f, 0.0f, 0.0f, 1.0f)),
                Optional.empty(), 0, 0, 8, 8, 1, 0);

        CommandStreamModuleWriter moduleWriter = new CommandStreamModuleWriter(s_goldenGraphHash);
        moduleWriter.addSection(CommandStreamModuleSectionType.BUFFER_HANDLE_TABLE, 0,
                emitSection(testArena, bufferTableWriter.requiredByteSize(),
                        bufferTableWriter::writeTo));
        moduleWriter.addSection(CommandStreamModuleSectionType.IMAGE_HANDLE_TABLE, 0,
                emitSection(testArena, imageTableWriter.requiredByteSize(),
                        imageTableWriter::writeTo));
        moduleWriter.addSection(CommandStreamModuleSectionType.IMAGE_VIEW_HANDLE_TABLE, 0,
                emitSection(testArena, imageViewTableWriter.requiredByteSize(),
                        imageViewTableWriter::writeTo));
        moduleWriter.addSection(CommandStreamModuleSectionType.SHADER_MODULE_TABLE, 0,
                emitSection(testArena, shaderTableWriter.requiredByteSize(),
                        shaderTableWriter::writeTo));
        moduleWriter.addSection(CommandStreamModuleSectionType.GRAPHICS_PIPELINE_TABLE, 0,
                emitSection(testArena, graphicsPipelineTableWriter.requiredByteSize(),
                        graphicsPipelineTableWriter::writeTo));
        moduleWriter.addSection(CommandStreamModuleSectionType.BARRIER_BATCH_TABLE, 0,
                emitSection(testArena, barrierTableWriter.requiredByteSize(),
                        barrierTableWriter::writeTo));
        moduleWriter.addSection(CommandStreamModuleSectionType.RENDERING_TEMPLATE_TABLE, 0,
                emitSection(testArena, renderingTemplateTableWriter.requiredByteSize(),
                        renderingTemplateTableWriter::writeTo));
        moduleWriter.addSection(CommandStreamModuleSectionType.LANE_STREAM, 0,
                recordFirstTriangleLaneStream(testArena));

        assertEquals(2280, moduleWriter.requiredByteSize());
        MemorySegment moduleSegment = testArena.allocate(2280, 8);
        assertEquals(2280, moduleWriter.writeTo(moduleSegment));
        return moduleSegment;
    }

    @Test
    void firstTriangleModuleMatchesTheCommittedCrossLanguageGolden() throws Exception {
        Path goldenFilePath = Path.of(System.getProperty("barrieww.testDataDirectory"),
                "CommandStream", "FirstTriangleModule.becs");
        byte[] goldenBytes = Files.readAllBytes(goldenFilePath);
        assertEquals(2280, goldenBytes.length);

        try (Arena testArena = Arena.ofConfined()) {
            assertArrayEquals(goldenBytes,
                    writeGoldenModule(testArena).toArray(ValueLayout.JAVA_BYTE));
        }
    }
}
