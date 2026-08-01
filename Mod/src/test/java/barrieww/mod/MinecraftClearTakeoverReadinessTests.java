package barrieww.mod;

import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.mockStatic;
import static org.mockito.Mockito.when;

import com.mojang.blaze3d.GpuFormat;
import com.mojang.blaze3d.pipeline.RenderTarget;
import com.mojang.blaze3d.textures.GpuTexture;
import com.mojang.blaze3d.textures.GpuTextureView;
import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.GameRenderer;
import org.junit.jupiter.api.Test;
import org.mockito.MockedStatic;

/**
 * @note ThreadSafety: Tests use isolated mocks and scoped static Minecraft replacement serially.
 * Verifies D9 step 1 static host color-texture readiness without Native resources.
 */
final class MinecraftClearTakeoverReadinessTests {
    private static final int s_hostTextureWidth = 854;
    private static final int s_hostTextureHeight = 480;
    private static final int s_requiredUsage = GpuTexture.USAGE_COPY_SRC
        | GpuTexture.USAGE_TEXTURE_BINDING;

    /**
     * @note ThreadSafety: Test-local mocks are not shared.
     * Accepts one live single-mip, single-layer color texture and matching full view.
     */
    @Test
    void acceptsCompleteColorTextureReadiness() {
        TextureState textureState = readyTextureState();

        assertTrue(MinecraftClearTakeoverReadiness.isReady(
            textureState.m_texture,
            textureState.m_textureView));
    }

    /**
     * @note ThreadSafety: Test-local mocks are not shared.
     * Rejects absent texture state and a view that does not belong to the supplied texture.
     */
    @Test
    void rejectsAbsentOrMismatchedTextureState() {
        TextureState textureState = readyTextureState();
        GpuTexture differentTexture = mock(GpuTexture.class);

        assertFalse(MinecraftClearTakeoverReadiness.isReady(
            null, textureState.m_textureView));
        assertFalse(MinecraftClearTakeoverReadiness.isReady(
            textureState.m_texture, null));
        when(textureState.m_textureView.texture()).thenReturn(differentTexture);
        assertFalse(MinecraftClearTakeoverReadiness.isReady(
            textureState.m_texture,
            textureState.m_textureView));
    }

    /**
     * @note ThreadSafety: Test-local mocks are not shared.
     * Rejects a closed texture or closed view.
     */
    @Test
    void rejectsClosedTextureState() {
        TextureState textureState = readyTextureState();

        when(textureState.m_texture.isClosed()).thenReturn(true);
        assertFalse(MinecraftClearTakeoverReadiness.isReady(
            textureState.m_texture,
            textureState.m_textureView));
        when(textureState.m_texture.isClosed()).thenReturn(false);
        when(textureState.m_textureView.isClosed()).thenReturn(true);
        assertFalse(MinecraftClearTakeoverReadiness.isReady(
            textureState.m_texture,
            textureState.m_textureView));
    }

    /**
     * @note ThreadSafety: Test-local mocks are not shared.
     * Rejects non-positive texture or view dimensions without comparing a requested extent.
     */
    @Test
    void rejectsNonPositiveTextureOrViewDimensions() {
        TextureState textureState = readyTextureState();

        when(textureState.m_texture.getWidth(0)).thenReturn(0);
        assertFalse(MinecraftClearTakeoverReadiness.isReady(
            textureState.m_texture,
            textureState.m_textureView));
        when(textureState.m_texture.getWidth(0)).thenReturn(s_hostTextureWidth);
        when(textureState.m_texture.getHeight(0)).thenReturn(0);
        assertFalse(MinecraftClearTakeoverReadiness.isReady(
            textureState.m_texture,
            textureState.m_textureView));
        when(textureState.m_texture.getHeight(0)).thenReturn(s_hostTextureHeight);
        when(textureState.m_textureView.getWidth(0)).thenReturn(0);
        assertFalse(MinecraftClearTakeoverReadiness.isReady(
            textureState.m_texture,
            textureState.m_textureView));
        when(textureState.m_textureView.getWidth(0)).thenReturn(s_hostTextureWidth);
        when(textureState.m_textureView.getHeight(0)).thenReturn(0);
        assertFalse(MinecraftClearTakeoverReadiness.isReady(
            textureState.m_texture,
            textureState.m_textureView));
    }

    /**
     * @note ThreadSafety: Test-local mocks are not shared.
     * Rejects depth-only texture format without inspecting alpha semantics.
     */
    @Test
    void rejectsTextureWithoutColorAspect() {
        TextureState textureState = readyTextureState();

        when(textureState.m_texture.getFormat()).thenReturn(null);
        assertFalse(MinecraftClearTakeoverReadiness.isReady(
            textureState.m_texture,
            textureState.m_textureView));
        when(textureState.m_texture.getFormat()).thenReturn(GpuFormat.D32_FLOAT);
        assertFalse(MinecraftClearTakeoverReadiness.isReady(
            textureState.m_texture,
            textureState.m_textureView));
    }

    /**
     * @note ThreadSafety: Test-local mocks are not shared.
     * Rejects each missing required texture usage independently.
     */
    @Test
    void rejectsMissingRequiredUsage() {
        TextureState textureState = readyTextureState();

        when(textureState.m_texture.usage()).thenReturn(GpuTexture.USAGE_TEXTURE_BINDING);
        assertFalse(MinecraftClearTakeoverReadiness.isReady(
            textureState.m_texture,
            textureState.m_textureView));
        when(textureState.m_texture.usage()).thenReturn(GpuTexture.USAGE_COPY_SRC);
        assertFalse(MinecraftClearTakeoverReadiness.isReady(
            textureState.m_texture,
            textureState.m_textureView));
    }

    /**
     * @note ThreadSafety: Test-local mocks are not shared.
     * Rejects zero or multiple texture depth layers.
     */
    @Test
    void rejectsTextureWithoutExactlyOneDepthLayer() {
        TextureState textureState = readyTextureState();

        when(textureState.m_texture.getDepthOrLayers()).thenReturn(0);
        assertFalse(MinecraftClearTakeoverReadiness.isReady(
            textureState.m_texture,
            textureState.m_textureView));
        when(textureState.m_texture.getDepthOrLayers()).thenReturn(2);
        assertFalse(MinecraftClearTakeoverReadiness.isReady(
            textureState.m_texture,
            textureState.m_textureView));
    }

    /**
     * @note ThreadSafety: Test-local mocks are not shared.
     * Rejects textures and views that are not one full base mip level.
     */
    @Test
    void rejectsNonSingleMipTextureOrView() {
        TextureState textureState = readyTextureState();

        when(textureState.m_texture.getMipLevels()).thenReturn(0);
        assertFalse(MinecraftClearTakeoverReadiness.isReady(
            textureState.m_texture,
            textureState.m_textureView));
        when(textureState.m_texture.getMipLevels()).thenReturn(2);
        assertFalse(MinecraftClearTakeoverReadiness.isReady(
            textureState.m_texture,
            textureState.m_textureView));
        when(textureState.m_texture.getMipLevels()).thenReturn(1);
        when(textureState.m_textureView.baseMipLevel()).thenReturn(1);
        assertFalse(MinecraftClearTakeoverReadiness.isReady(
            textureState.m_texture,
            textureState.m_textureView));
        when(textureState.m_textureView.baseMipLevel()).thenReturn(0);
        when(textureState.m_textureView.mipLevels()).thenReturn(0);
        assertFalse(MinecraftClearTakeoverReadiness.isReady(
            textureState.m_texture,
            textureState.m_textureView));
        when(textureState.m_textureView.mipLevels()).thenReturn(2);
        assertFalse(MinecraftClearTakeoverReadiness.isReady(
            textureState.m_texture,
            textureState.m_textureView));
    }

    /**
     * @note ThreadSafety: Not concurrency-safe because Minecraft static behavior is scoped-replaced.
     * Treats an unavailable Minecraft singleton as ordinary unready state without throwing.
     */
    @Test
    void unavailableMinecraftIsNotReady() {
        try (MockedStatic<Minecraft> minecraftClass = mockStatic(Minecraft.class)) {
            minecraftClass.when(Minecraft::getInstance).thenReturn(null);

            assertFalse(MinecraftClearTakeoverReadiness.isMinecraftColorTextureReady());
        }
    }

    /**
     * @note ThreadSafety: Test-local renderer and texture mocks are not shared.
     * Resolves and validates the initialized Minecraft main render target.
     */
    @Test
    void acceptsInitializedMainRenderTarget() {
        TextureState textureState = readyTextureState();
        GameRenderer gameRenderer = mock(GameRenderer.class);
        RenderTarget mainRenderTarget = mock(RenderTarget.class);
        when(gameRenderer.mainRenderTarget()).thenReturn(mainRenderTarget);
        when(mainRenderTarget.getColorTextureView()).thenReturn(textureState.m_textureView);

        assertTrue(MinecraftClearTakeoverReadiness.isGameRendererColorTextureReady(
            gameRenderer));
    }

    /**
     * @note ThreadSafety: Test-local renderer and target mocks are not shared.
     * Treats absent renderer, target and color view initialization as ordinary unready state.
     */
    @Test
    void rejectsIncompleteMainRenderTargetInitialization() {
        GameRenderer gameRenderer = mock(GameRenderer.class);
        RenderTarget mainRenderTarget = mock(RenderTarget.class);

        assertFalse(MinecraftClearTakeoverReadiness.isGameRendererColorTextureReady(null));
        assertFalse(MinecraftClearTakeoverReadiness.isGameRendererColorTextureReady(
            gameRenderer));
        when(gameRenderer.mainRenderTarget()).thenReturn(mainRenderTarget);
        assertFalse(MinecraftClearTakeoverReadiness.isGameRendererColorTextureReady(
            gameRenderer));
    }

    /**
     * @note ThreadSafety: Not concurrency-safe because Minecraft static behavior is scoped-replaced.
     * Contains an initialization exception as ordinary unready state.
     */
    @Test
    void minecraftInitializationFailureIsNotReady() {
        try (MockedStatic<Minecraft> minecraftClass = mockStatic(Minecraft.class)) {
            minecraftClass.when(Minecraft::getInstance)
                .thenThrow(new IllegalStateException("not initialized"));

            assertFalse(MinecraftClearTakeoverReadiness.isMinecraftColorTextureReady());
        }
    }

    /**
     * @note ThreadSafety: Creates test-local mocks without shared state.
     * Creates complete texture and view state suitable for one positive readiness decision.
     *
     * @return TextureState Complete mocked host texture state
     */
    private static TextureState readyTextureState() {
        GpuTexture texture = mock(GpuTexture.class);
        GpuTextureView textureView = mock(GpuTextureView.class);
        when(texture.getFormat()).thenReturn(GpuFormat.RGBA8_UNORM);
        when(texture.getWidth(0)).thenReturn(s_hostTextureWidth);
        when(texture.getHeight(0)).thenReturn(s_hostTextureHeight);
        when(texture.getDepthOrLayers()).thenReturn(1);
        when(texture.getMipLevels()).thenReturn(1);
        when(texture.usage()).thenReturn(s_requiredUsage);
        when(textureView.texture()).thenReturn(texture);
        when(textureView.baseMipLevel()).thenReturn(0);
        when(textureView.mipLevels()).thenReturn(1);
        when(textureView.getWidth(0)).thenReturn(s_hostTextureWidth);
        when(textureView.getHeight(0)).thenReturn(s_hostTextureHeight);
        return new TextureState(texture, textureView);
    }

    /**
     * @note ThreadSafety: Immutable test value confined to one test invocation.
     * Groups one mocked texture with its matching view.
     */
    private static final class TextureState {
        private final GpuTexture m_texture;
        private final GpuTextureView m_textureView;

        /**
         * @note ThreadSafety: Construction is confined to one test invocation.
         * Captures one mocked texture pair.
         *
         * @param GpuTexture texture Mocked host texture
         * @param GpuTextureView textureView Mocked full texture view
         */
        private TextureState(GpuTexture texture, GpuTextureView textureView) {
            m_texture = texture;
            m_textureView = textureView;
        }
    }
}
