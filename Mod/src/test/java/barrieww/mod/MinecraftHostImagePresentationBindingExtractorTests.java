package barrieww.mod;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import barrieww.core.interoperability.HostImagePresentationBinding;
import barrieww.core.interoperability.PresentationImageFormat;
import com.mojang.blaze3d.GpuFormat;
import com.mojang.blaze3d.pipeline.RenderTarget;
import com.mojang.blaze3d.textures.GpuTexture;
import com.mojang.blaze3d.textures.GpuTextureView;
import com.mojang.blaze3d.vulkan.VulkanGpuTexture;
import com.mojang.blaze3d.vulkan.VulkanGpuTextureView;
import java.util.Optional;
import net.minecraft.client.renderer.GameRenderer;
import org.junit.jupiter.api.Test;

/**
 * @note ThreadSafety: Tests use isolated Mockito state and do not share mutable objects.
 * Verifies strict extraction of immutable Core host-image bindings from Minecraft Vulkan state.
 * @warning MemoryOwnership: All mocked Minecraft resources remain test-owned and extraction must
 *          neither retain nor close them.
 */
final class MinecraftHostImagePresentationBindingExtractorTests {
    private static final int s_framebufferWidth = 854;
    private static final int s_framebufferHeight = 480;
    private static final int s_requiredUsage = GpuTexture.USAGE_COPY_SRC
        | GpuTexture.USAGE_TEXTURE_BINDING;
    private static final long s_hostImageHandle = 0x1234L;

    /**
     * @note ThreadSafety: Test-local mocks are not shared.
     * Maps the two supported Vulkan UNORM surface formats and returns exact immutable fields.
     */
    @Test
    void extractsExactBindingForBothSupportedSurfaceFormats() {
        HostState redFirstHostState = readyHostState();
        HostImagePresentationBinding redFirstBinding = extractRequired(redFirstHostState, 37);

        assertEquals(s_hostImageHandle, redFirstBinding.hostImageHandle());
        assertEquals(PresentationImageFormat.R8G8B8A8_UNORM, redFirstBinding.hostImageFormat());
        assertEquals(PresentationImageFormat.R8G8B8A8_UNORM,
            redFirstBinding.requestedSurfaceFormat());
        assertEquals(s_framebufferWidth, redFirstBinding.width());
        assertEquals(s_framebufferHeight, redFirstBinding.height());
        verify(redFirstHostState.m_texture, never()).close();
        verify(redFirstHostState.m_textureView, never()).close();

        HostState blueFirstHostState = readyHostState();
        HostImagePresentationBinding blueFirstBinding = extractRequired(blueFirstHostState, 44);

        assertEquals(PresentationImageFormat.R8G8B8A8_UNORM, blueFirstBinding.hostImageFormat());
        assertEquals(PresentationImageFormat.B8G8R8A8_UNORM,
            blueFirstBinding.requestedSurfaceFormat());
        verify(blueFirstHostState.m_texture, never()).close();
        verify(blueFirstHostState.m_textureView, never()).close();
    }

    /**
     * @note ThreadSafety: Test-local mocks are not shared.
     * Rejects absent renderer, target, color view, and color texture initialization.
     */
    @Test
    void rejectsAbsentHostObjects() {
        assertEmpty(null, s_framebufferWidth, s_framebufferHeight, 37);

        HostState hostState = readyHostState();
        when(hostState.m_gameRenderer.mainRenderTarget()).thenReturn(null);
        assertEmpty(hostState, 37);

        hostState = readyHostState();
        when(hostState.m_mainRenderTarget.getColorTextureView()).thenReturn(null);
        assertEmpty(hostState, 37);

        hostState = readyHostState();
        when(hostState.m_textureView.texture()).thenReturn(null);
        assertEmpty(hostState, 37);
    }

    /**
     * @note ThreadSafety: Test-local mocks are not shared.
     * Rejects generic Minecraft texture implementations instead of accepting non-Vulkan handles.
     */
    @Test
    void rejectsNonVulkanTextureOrViewClasses() {
        HostState hostState = readyHostState();
        GpuTextureView genericTextureView = mock(GpuTextureView.class);
        when(hostState.m_mainRenderTarget.getColorTextureView()).thenReturn(genericTextureView);
        assertEmpty(hostState, 37);

        hostState = readyHostState();
        GpuTexture genericTexture = mock(GpuTexture.class);
        when(hostState.m_mainRenderTarget.getColorTexture()).thenReturn(genericTexture);
        assertEmpty(hostState, 37);
    }

    /**
     * @note ThreadSafety: Test-local mocks are not shared.
     * Rejects a Vulkan view whose texture is not the target color texture.
     */
    @Test
    void rejectsMismatchedTextureIdentity() {
        HostState hostState = readyHostState();
        VulkanGpuTexture differentTexture = mock(VulkanGpuTexture.class);
        when(hostState.m_mainRenderTarget.getColorTexture()).thenReturn(differentTexture);

        assertEmpty(hostState, 37);
    }

    /**
     * @note ThreadSafety: Test-local mocks are not shared.
     * Rejects independently closed texture and view resources.
     */
    @Test
    void rejectsClosedTextureOrView() {
        HostState hostState = readyHostState();
        when(hostState.m_texture.isClosed()).thenReturn(true);
        assertEmpty(hostState, 37);

        hostState = readyHostState();
        when(hostState.m_textureView.isClosed()).thenReturn(true);
        assertEmpty(hostState, 37);
    }

    /**
     * @note ThreadSafety: Test-local mocks are not shared.
     * Rejects any host texture format other than exact color RGBA8 UNORM.
     */
    @Test
    void rejectsWrongTextureFormatOrAspect() {
        HostState hostState = readyHostState();
        when(hostState.m_texture.getFormat()).thenReturn(null);
        assertEmpty(hostState, 37);

        hostState = readyHostState();
        when(hostState.m_texture.getFormat()).thenReturn(GpuFormat.D32_FLOAT);
        assertEmpty(hostState, 37);

        hostState = readyHostState();
        when(hostState.m_texture.getFormat()).thenReturn(GpuFormat.RGBA8_SNORM);
        assertEmpty(hostState, 37);
    }

    /**
     * @note ThreadSafety: Test-local mocks are not shared.
     * Rejects each missing required host texture usage independently.
     */
    @Test
    void rejectsEachMissingRequiredUsage() {
        HostState hostState = readyHostState();
        when(hostState.m_texture.usage()).thenReturn(GpuTexture.USAGE_TEXTURE_BINDING);
        assertEmpty(hostState, 37);

        hostState = readyHostState();
        when(hostState.m_texture.usage()).thenReturn(GpuTexture.USAGE_COPY_SRC);
        assertEmpty(hostState, 37);
    }

    /**
     * @note ThreadSafety: Test-local mocks are not shared.
     * Rejects zero and negative requested framebuffer dimensions.
     */
    @Test
    void rejectsNonPositiveRequestedDimensions() {
        HostState hostState = readyHostState();
        assertEmpty(hostState.m_gameRenderer, 0, s_framebufferHeight, 37);
        assertEmpty(hostState.m_gameRenderer, -1, s_framebufferHeight, 37);
        assertEmpty(hostState.m_gameRenderer, s_framebufferWidth, 0, 37);
        assertEmpty(hostState.m_gameRenderer, s_framebufferWidth, -1, 37);
    }

    /**
     * @note ThreadSafety: Test-local mocks are not shared.
     * Rejects target width and height that differ from the requested framebuffer extent.
     */
    @Test
    void rejectsTargetDimensionMismatch() {
        HostState hostState = readyHostState();
        hostState.m_mainRenderTarget.width = s_framebufferWidth - 1;
        assertEmpty(hostState, 37);

        hostState = readyHostState();
        hostState.m_mainRenderTarget.height = s_framebufferHeight - 1;
        assertEmpty(hostState, 37);
    }

    /**
     * @note ThreadSafety: Test-local mocks are not shared.
     * Rejects texture width and height that differ from the requested framebuffer extent.
     */
    @Test
    void rejectsTextureDimensionMismatch() {
        HostState hostState = readyHostState();
        when(hostState.m_texture.getWidth(0)).thenReturn(s_framebufferWidth - 1);
        assertEmpty(hostState, 37);

        hostState = readyHostState();
        when(hostState.m_texture.getHeight(0)).thenReturn(s_framebufferHeight - 1);
        assertEmpty(hostState, 37);
    }

    /**
     * @note ThreadSafety: Test-local mocks are not shared.
     * Rejects view width and height that differ from the requested framebuffer extent.
     */
    @Test
    void rejectsViewDimensionMismatch() {
        HostState hostState = readyHostState();
        when(hostState.m_textureView.getWidth(0)).thenReturn(s_framebufferWidth - 1);
        assertEmpty(hostState, 37);

        hostState = readyHostState();
        when(hostState.m_textureView.getHeight(0)).thenReturn(s_framebufferHeight - 1);
        assertEmpty(hostState, 37);
    }

    /**
     * @note ThreadSafety: Test-local mocks are not shared.
     * Rejects texture layer counts other than exactly one.
     */
    @Test
    void rejectsInvalidTextureLayerCount() {
        HostState hostState = readyHostState();
        when(hostState.m_texture.getDepthOrLayers()).thenReturn(0);
        assertEmpty(hostState, 37);

        hostState = readyHostState();
        when(hostState.m_texture.getDepthOrLayers()).thenReturn(2);
        assertEmpty(hostState, 37);
    }

    /**
     * @note ThreadSafety: Test-local mocks are not shared.
     * Rejects texture mip counts other than exactly one.
     */
    @Test
    void rejectsInvalidTextureMipCount() {
        HostState hostState = readyHostState();
        when(hostState.m_texture.getMipLevels()).thenReturn(0);
        assertEmpty(hostState, 37);

        hostState = readyHostState();
        when(hostState.m_texture.getMipLevels()).thenReturn(2);
        assertEmpty(hostState, 37);
    }

    /**
     * @note ThreadSafety: Test-local mocks are not shared.
     * Rejects nonzero view base mip and view mip counts other than exactly one.
     */
    @Test
    void rejectsInvalidViewMipRange() {
        HostState hostState = readyHostState();
        when(hostState.m_textureView.baseMipLevel()).thenReturn(1);
        assertEmpty(hostState, 37);

        hostState = readyHostState();
        when(hostState.m_textureView.mipLevels()).thenReturn(0);
        assertEmpty(hostState, 37);

        hostState = readyHostState();
        when(hostState.m_textureView.mipLevels()).thenReturn(2);
        assertEmpty(hostState, 37);
    }

    /**
     * @note ThreadSafety: Test-local mocks are not shared.
     * Rejects a null Vulkan image handle without constructing a Core binding.
     */
    @Test
    void rejectsZeroVulkanImageHandle() {
        HostState hostState = readyHostState();
        when(hostState.m_texture.vkImage()).thenReturn(0L);

        assertEmpty(hostState, 37);
    }

    /**
     * @note ThreadSafety: Test-local mocks are not shared.
     * Rejects Vulkan SRGB and unknown surface format values without passing raw values to Core.
     */
    @Test
    void rejectsSrgbAndUnknownSurfaceFormats() {
        HostState hostState = readyHostState();
        assertEmpty(hostState, 43);
        assertEmpty(hostState, 50);
        assertEmpty(hostState, 0);
        assertEmpty(hostState, Integer.MAX_VALUE);
    }

    /**
     * @note ThreadSafety: Test-local mocks are not shared.
     * Contains Minecraft runtime initialization failures as ordinary absent readiness.
     */
    @Test
    void runtimeGetterFailureReturnsEmpty() {
        GameRenderer gameRenderer = mock(GameRenderer.class);
        when(gameRenderer.mainRenderTarget()).thenThrow(
            new IllegalStateException("renderer not initialized"));

        assertEmpty(gameRenderer, s_framebufferWidth, s_framebufferHeight, 37);
    }

    /**
     * @note ThreadSafety: Creates test-local mocks without shared state.
     * Creates a complete Minecraft Vulkan host state matching the requested framebuffer extent.
     *
     * @return HostState Complete mocked host state
     */
    private static HostState readyHostState() {
        GameRenderer gameRenderer = mock(GameRenderer.class);
        RenderTarget mainRenderTarget = mock(RenderTarget.class);
        VulkanGpuTexture texture = mock(VulkanGpuTexture.class);
        VulkanGpuTextureView textureView = mock(VulkanGpuTextureView.class);
        mainRenderTarget.width = s_framebufferWidth;
        mainRenderTarget.height = s_framebufferHeight;
        when(gameRenderer.mainRenderTarget()).thenReturn(mainRenderTarget);
        when(mainRenderTarget.getColorTexture()).thenReturn(texture);
        when(mainRenderTarget.getColorTextureView()).thenReturn(textureView);
        when(textureView.texture()).thenReturn(texture);
        when(texture.getFormat()).thenReturn(GpuFormat.RGBA8_UNORM);
        when(texture.usage()).thenReturn(s_requiredUsage);
        when(texture.getWidth(0)).thenReturn(s_framebufferWidth);
        when(texture.getHeight(0)).thenReturn(s_framebufferHeight);
        when(texture.getDepthOrLayers()).thenReturn(1);
        when(texture.getMipLevels()).thenReturn(1);
        when(textureView.baseMipLevel()).thenReturn(0);
        when(textureView.mipLevels()).thenReturn(1);
        when(textureView.getWidth(0)).thenReturn(s_framebufferWidth);
        when(textureView.getHeight(0)).thenReturn(s_framebufferHeight);
        when(texture.vkImage()).thenReturn(s_hostImageHandle);
        return new HostState(gameRenderer, mainRenderTarget, texture, textureView);
    }

    /**
     * @note ThreadSafety: Reads only test-local mocks.
     * Extracts one binding and requires presence for a success-path assertion.
     *
     * @param HostState hostState Complete mocked host state
     * @param int selectedSurfaceFormatValue Raw Vulkan surface format value
     * @return HostImagePresentationBinding Extracted immutable binding
     */
    private static HostImagePresentationBinding extractRequired(
        HostState hostState,
        int selectedSurfaceFormatValue) {
        Optional<HostImagePresentationBinding> binding =
            MinecraftHostImagePresentationBindingExtractor.extract(
                hostState.m_gameRenderer,
                s_framebufferWidth,
                s_framebufferHeight,
                selectedSurfaceFormatValue);
        assertTrue(binding.isPresent());
        return binding.orElseThrow();
    }

    /**
     * @note ThreadSafety: Reads only test-local mocks.
     * Requires an empty extraction result for one complete host state.
     *
     * @param HostState hostState Mocked host state
     * @param int selectedSurfaceFormatValue Raw Vulkan surface format value
     */
    private static void assertEmpty(HostState hostState, int selectedSurfaceFormatValue) {
        assertEmpty(
            hostState.m_gameRenderer,
            s_framebufferWidth,
            s_framebufferHeight,
            selectedSurfaceFormatValue);
    }

    /**
     * @note ThreadSafety: Reads only test-local mocks.
     * Requires an empty extraction result for explicit renderer, extent, and surface format input.
     *
     * @param GameRenderer gameRenderer Mocked renderer, or null
     * @param int framebufferWidth Requested framebuffer width
     * @param int framebufferHeight Requested framebuffer height
     * @param int selectedSurfaceFormatValue Raw Vulkan surface format value
     */
    private static void assertEmpty(
        GameRenderer gameRenderer,
        int framebufferWidth,
        int framebufferHeight,
        int selectedSurfaceFormatValue) {
        assertTrue(MinecraftHostImagePresentationBindingExtractor.extract(
            gameRenderer,
            framebufferWidth,
            framebufferHeight,
            selectedSurfaceFormatValue).isEmpty());
    }

    /**
     * @note ThreadSafety: Immutable test value confined to one test invocation.
     * Groups mocked Minecraft host resources used by one extraction decision.
     */
    private static final class HostState {
        private final GameRenderer m_gameRenderer;
        private final RenderTarget m_mainRenderTarget;
        private final VulkanGpuTexture m_texture;
        private final VulkanGpuTextureView m_textureView;

        /**
         * @note ThreadSafety: Construction is confined to one test invocation.
         * Captures one complete mocked Minecraft host state.
         *
         * @param GameRenderer gameRenderer Mocked game renderer
         * @param RenderTarget mainRenderTarget Mocked main render target
         * @param VulkanGpuTexture texture Mocked Vulkan host color texture
         * @param VulkanGpuTextureView textureView Mocked Vulkan host color texture view
         */
        private HostState(
            GameRenderer gameRenderer,
            RenderTarget mainRenderTarget,
            VulkanGpuTexture texture,
            VulkanGpuTextureView textureView) {
            m_gameRenderer = gameRenderer;
            m_mainRenderTarget = mainRenderTarget;
            m_texture = texture;
            m_textureView = textureView;
        }
    }
}
