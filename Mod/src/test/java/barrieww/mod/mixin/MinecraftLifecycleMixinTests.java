package barrieww.mod.mixin;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.mockito.Mockito.mockStatic;

import barrieww.mod.MainRenderTargetGenerationTracker;
import barrieww.mod.MainRenderTargetResizeListener;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import net.minecraft.client.Minecraft;
import org.junit.jupiter.api.Test;
import org.mockito.MockedStatic;
import org.spongepowered.asm.mixin.Final;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * @note ThreadSafety: Tests serialize static tracker and Minecraft singleton replacement access.
 * Pins Minecraft lifecycle injection structure and behavior at the complete-resize boundary.
 * @warning MemoryOwnership: Test callbacks and mocks own no Minecraft or Native resources.
 */
final class MinecraftLifecycleMixinTests {
    /** Pins the exact cancellable complete GameRenderer resize HEAD injection. */
    @Test
    void gameRendererResizeInjectionTargetsExactCancellableHead() throws Exception {
        Class<?> mixinClass = Class.forName("barrieww.mod.mixin.GameRendererMixin");
        Method callbackMethod = mixinClass.getDeclaredMethod(
            "barrieww$beforeMainRenderTargetResize",
            int.class,
            int.class,
            CallbackInfo.class);
        Inject injection = callbackMethod.getAnnotation(Inject.class);

        assertEquals("resize(II)V", injection.method()[0]);
        assertEquals(At.Shift.NONE, injection.at()[0].shift());
        assertEquals("HEAD", injection.at()[0].value());
        assertTrue(injection.cancellable());
    }

    /** Verifies detach veto cancels the complete GameRenderer resize callback. */
    @Test
    void gameRendererResizeVetoCancelsCompleteMethod() throws Exception {
        MainRenderTargetResizeListener resizeListener = () -> false;
        MainRenderTargetGenerationTracker.registerResizeListener(resizeListener);
        try {
            Class<?> mixinClass = Class.forName("barrieww.mod.mixin.GameRendererMixin");
            Object mixin = mixinClass.getDeclaredConstructor().newInstance();
            CallbackInfo callbackInformation = new CallbackInfo("resize", true);

            invokeCallback(
                mixinClass.getDeclaredMethod(
                    "barrieww$beforeMainRenderTargetResize",
                    int.class,
                    int.class,
                    CallbackInfo.class),
                mixin,
                1280,
                720,
                callbackInformation);

            assertTrue(callbackInformation.isCancelled());
        } finally {
            MainRenderTargetGenerationTracker.unregisterResizeListener(resizeListener);
        }
    }

    /** Verifies absent lifecycle ownership permits vanilla complete resize unchanged. */
    @Test
    void gameRendererResizeWithoutListenerDoesNotCancel() throws Exception {
        Class<?> mixinClass = Class.forName("barrieww.mod.mixin.GameRendererMixin");
        Object mixin = mixinClass.getDeclaredConstructor().newInstance();
        CallbackInfo callbackInformation = new CallbackInfo("resize", true);

        invokeCallback(
            mixinClass.getDeclaredMethod(
                "barrieww$beforeMainRenderTargetResize",
                int.class,
                int.class,
                CallbackInfo.class),
            mixin,
            1280,
            720,
            callbackInformation);

        assertFalse(callbackInformation.isCancelled());
    }

    /** Pins the exact successful RenderTarget resize TAIL injection. */
    @Test
    void renderTargetResizeInjectionTargetsExactTail() throws Exception {
        Class<?> mixinClass = Class.forName("barrieww.mod.mixin.RenderTargetMixin");
        Method callbackMethod = mixinClass.getDeclaredMethod(
            "barrieww$publishMainRenderTargetResize",
            int.class,
            int.class,
            CallbackInfo.class);
        Inject injection = callbackMethod.getAnnotation(Inject.class);

        assertEquals("resize(II)V", injection.method()[0]);
        assertEquals("TAIL", injection.at()[0].value());
        assertFalse(injection.cancellable());
    }

    /** Verifies a non-current or unavailable target cannot publish a successful generation. */
    @Test
    void renderTargetTailDoesNotPublishWithoutCurrentMainTargetIdentity() throws Exception {
        Class<?> mixinClass = Class.forName("barrieww.mod.mixin.RenderTargetMixin");
        Object mixin = mixinClass.getDeclaredConstructor().newInstance();
        Method callbackMethod = mixinClass.getDeclaredMethod(
            "barrieww$publishMainRenderTargetResize",
            int.class,
            int.class,
            CallbackInfo.class);
        long initialGeneration = MainRenderTargetGenerationTracker.currentGeneration();

        try (MockedStatic<Minecraft> minecraftClass = mockStatic(Minecraft.class)) {
            minecraftClass.when(Minecraft::getInstance).thenReturn(null);
            invokeCallback(callbackMethod, mixin, 1280, 720, null);
        }

        assertEquals(initialGeneration, MainRenderTargetGenerationTracker.currentGeneration());
    }

    /** Verifies incomplete Minecraft initialization cannot falsely publish a generation. */
    @Test
    void renderTargetTailContainsInitializationFailure() throws Exception {
        Class<?> mixinClass = Class.forName("barrieww.mod.mixin.RenderTargetMixin");
        Object mixin = mixinClass.getDeclaredConstructor().newInstance();
        Method callbackMethod = mixinClass.getDeclaredMethod(
            "barrieww$publishMainRenderTargetResize",
            int.class,
            int.class,
            CallbackInfo.class);
        long initialGeneration = MainRenderTargetGenerationTracker.currentGeneration();

        try (MockedStatic<Minecraft> minecraftClass = mockStatic(Minecraft.class)) {
            minecraftClass.when(Minecraft::getInstance)
                .thenThrow(new IllegalStateException("not initialized"));
            invokeCallback(callbackMethod, mixin, 1280, 720, null);
        }

        assertEquals(initialGeneration, MainRenderTargetGenerationTracker.currentGeneration());
    }

    /** Pins selected-format shadow ownership and exact Minecraft field alias. */
    @Test
    void surfaceShadowsFinalSwapchainImageFormat() throws Exception {
        var formatField = VulkanGpuSurfaceMixin.class.getDeclaredField("m_swapchainImageFormat");

        assertTrue(formatField.isAnnotationPresent(Shadow.class));
        assertTrue(formatField.isAnnotationPresent(Final.class));
        assertEquals(
            "swapchainImageFormat",
            formatField.getAnnotation(Shadow.class).aliases()[0]);
    }

    /** Pins both lifecycle mixins in the client-only Mixin configuration. */
    @Test
    void mixinConfigurationRegistersMinecraftLifecycleMixins() throws Exception {
        String configuration;
        try (var inputStream = MinecraftLifecycleMixinTests.class.getClassLoader()
                 .getResourceAsStream("barrieww.mixins.json")) {
            configuration = new String(inputStream.readAllBytes(), StandardCharsets.UTF_8);
        }

        assertTrue(configuration.contains("\"GameRendererMixin\""));
        assertTrue(configuration.contains("\"RenderTargetMixin\""));
        assertTrue(configuration.contains("\"VulkanGpuSurfaceMixin\""));
    }

    /** Verifies the superseded clear-only readiness validator is no longer packaged. */
    @Test
    void obsoleteClearTakeoverReadinessIsRemoved() {
        assertThrows(
            ClassNotFoundException.class,
            () -> Class.forName("barrieww.mod.MinecraftClearTakeoverReadiness"));
    }

    /**
     * @note ThreadSafety: Test-thread-confined; mutates only accessibility of the supplied method.
     * Invokes one private injected callback and unwraps its original checked failure for diagnostics.
     *
     * @param Method callbackMethod Reflected injected callback method
     * @param Object receiver Test-owned callback receiver
     * @param Object[] arguments Exact callback arguments
     * @throws Exception When callback lookup, access, or invocation reports a checked failure
     * @warning MemoryOwnership: Receiver, arguments, and callback state remain test-owned.
     */
    private static void invokeCallback(Method callbackMethod, Object receiver, Object... arguments)
        throws Exception {
        callbackMethod.setAccessible(true);
        try {
            callbackMethod.invoke(receiver, arguments);
        } catch (InvocationTargetException invocationFailure) {
            if (invocationFailure.getCause() instanceof Exception cause) {
                throw cause;
            }
            throw invocationFailure;
        }
    }
}
