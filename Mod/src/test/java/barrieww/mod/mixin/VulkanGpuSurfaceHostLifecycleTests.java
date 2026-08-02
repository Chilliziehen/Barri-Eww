package barrieww.mod.mixin;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertNull;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.mockStatic;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import barrieww.core.interoperability.HostImagePresentationBinding;
import barrieww.core.interoperability.PresentationBootstrapHandles;
import barrieww.core.interoperability.PresentationImageFormat;
import barrieww.mod.BarriEwwClientInitializer;
import barrieww.mod.MainRenderTargetGenerationTracker;
import barrieww.mod.MainRenderTargetResizeListener;
import barrieww.mod.MinecraftHostImagePresentationBindingExtractor;
import barrieww.mod.MinecraftVulkanBootstrapHandles;
import barrieww.mod.PresentationGenerationInputs;
import barrieww.mod.PresentationGenerationPreparation;
import barrieww.mod.PresentationTakeoverCoordinator;
import com.mojang.blaze3d.pipeline.RenderTarget;
import com.mojang.blaze3d.systems.GpuSurface;
import com.mojang.blaze3d.vulkan.VulkanDevice;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.file.Path;
import java.util.Optional;
import java.util.concurrent.atomic.AtomicReference;
import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.GameRenderer;
import org.junit.jupiter.api.Test;
import org.mockito.MockedStatic;
import org.slf4j.Logger;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * @note ThreadSafety: Tests serialize static Minecraft adapters and the render-target tracker.
 * Verifies atomic host generation preparation and surface-owned listener lifecycle wiring.
 * @warning MemoryOwnership: Every Vulkan address is a borrowed scalar and every collaborator is mocked.
 */
final class VulkanGpuSurfaceHostLifecycleTests {
    private static final int s_framebufferWidth = 1280;
    private static final int s_framebufferHeight = 720;
    private static final long s_firstHostImageHandle = 71L;
    private static final long s_secondHostImageHandle = 72L;

    /** Verifies an exact already-sized target is extracted without a redundant complete resize. */
    @Test
    void prepareGenerationDoesNotResizeExactTarget() throws Exception {
        HostPreparationState hostState = hostPreparationState(
            s_framebufferWidth,
            s_framebufferHeight);
        long generation = publishGeneration();

        try (MockedStatic<Minecraft> minecraftClass = mockStatic(Minecraft.class);
             MockedStatic<MinecraftVulkanBootstrapHandles> handlesAdapter =
                 mockStatic(MinecraftVulkanBootstrapHandles.class);
             MockedStatic<MinecraftHostImagePresentationBindingExtractor> bindingExtractor =
                 mockStatic(MinecraftHostImagePresentationBindingExtractor.class)) {
            prepareStaticAdapters(
                hostState,
                minecraftClass,
                handlesAdapter,
                bindingExtractor,
                binding(s_firstHostImageHandle));

            PresentationGenerationInputs inputs = invokePrepareGeneration(
                hostState.m_surfaceMixin,
                configuration());

            assertEquals(generation, inputs.hostTargetGeneration());
            assertEquals(s_firstHostImageHandle, inputs.hostImageBinding().hostImageHandle());
            verify(hostState.m_gameRenderer, never()).resize(anyInt(), anyInt());
        }
    }

    /** Verifies a mismatched positive target uses the full public renderer resize before extraction. */
    @Test
    void prepareGenerationCallsCompleteResizeForRequestedExtent() throws Exception {
        HostPreparationState hostState = hostPreparationState(800, 600);
        long initialGeneration = MainRenderTargetGenerationTracker.currentGeneration();
        doAnswer(invocation -> {
            hostState.m_mainRenderTarget.width = s_framebufferWidth;
            hostState.m_mainRenderTarget.height = s_framebufferHeight;
            MainRenderTargetGenerationTracker.mainRenderTargetResizeSucceeded(
                hostState.m_mainRenderTarget,
                hostState.m_mainRenderTarget);
            return null;
        }).when(hostState.m_gameRenderer).resize(s_framebufferWidth, s_framebufferHeight);

        try (MockedStatic<Minecraft> minecraftClass = mockStatic(Minecraft.class);
             MockedStatic<MinecraftVulkanBootstrapHandles> handlesAdapter =
                 mockStatic(MinecraftVulkanBootstrapHandles.class);
             MockedStatic<MinecraftHostImagePresentationBindingExtractor> bindingExtractor =
                 mockStatic(MinecraftHostImagePresentationBindingExtractor.class)) {
            prepareStaticAdapters(
                hostState,
                minecraftClass,
                handlesAdapter,
                bindingExtractor,
                binding(s_firstHostImageHandle));

            PresentationGenerationInputs inputs = invokePrepareGeneration(
                hostState.m_surfaceMixin,
                configuration());

            verify(hostState.m_gameRenderer).resize(s_framebufferWidth, s_framebufferHeight);
            assertEquals(initialGeneration + 1, inputs.hostTargetGeneration());
        }
    }

    /** Verifies generation movement during binding extraction rejects the unstable snapshot. */
    @Test
    void prepareGenerationRejectsGenerationChangedDuringExtraction() throws Exception {
        HostPreparationState hostState = hostPreparationState(
            s_framebufferWidth,
            s_framebufferHeight);
        publishGeneration();

        try (MockedStatic<Minecraft> minecraftClass = mockStatic(Minecraft.class);
             MockedStatic<MinecraftVulkanBootstrapHandles> handlesAdapter =
                 mockStatic(MinecraftVulkanBootstrapHandles.class);
             MockedStatic<MinecraftHostImagePresentationBindingExtractor> bindingExtractor =
                 mockStatic(MinecraftHostImagePresentationBindingExtractor.class)) {
            minecraftClass.when(Minecraft::getInstance).thenReturn(hostState.m_minecraft);
            handlesAdapter.when(() -> MinecraftVulkanBootstrapHandles.extractBorrowedHandles(
                hostState.m_surfaceDevice,
                4L)).thenReturn(Optional.of(bootstrapHandles()));
            bindingExtractor.when(() -> MinecraftHostImagePresentationBindingExtractor.extract(
                hostState.m_gameRenderer,
                s_framebufferWidth,
                s_framebufferHeight,
                44)).thenAnswer(invocation -> {
                    publishGeneration();
                    return Optional.of(binding(s_firstHostImageHandle));
                });

            assertNull(invokePrepareGeneration(hostState.m_surfaceMixin, configuration()));
        }
    }

    /** Verifies absent extraction leaves generation preparation unready for vanilla configure. */
    @Test
    void prepareGenerationRejectsAbsentHostBinding() throws Exception {
        HostPreparationState hostState = hostPreparationState(
            s_framebufferWidth,
            s_framebufferHeight);
        publishGeneration();

        try (MockedStatic<Minecraft> minecraftClass = mockStatic(Minecraft.class);
             MockedStatic<MinecraftVulkanBootstrapHandles> handlesAdapter =
                 mockStatic(MinecraftVulkanBootstrapHandles.class);
             MockedStatic<MinecraftHostImagePresentationBindingExtractor> bindingExtractor =
                 mockStatic(MinecraftHostImagePresentationBindingExtractor.class)) {
            prepareStaticAdapters(
                hostState,
                minecraftClass,
                handlesAdapter,
                bindingExtractor,
                null);

            assertNull(invokePrepareGeneration(hostState.m_surfaceMixin, configuration()));
        }
    }

    /** Verifies an unchanged tracker generation cannot silently accept a different image handle. */
    @Test
    void prepareGenerationRejectsChangedHandleForCommittedGeneration() throws Exception {
        HostPreparationState hostState = hostPreparationState(
            s_framebufferWidth,
            s_framebufferHeight);
        long generation = publishGeneration();
        setField(hostState.m_surfaceMixin, "barrieww$m_lastCommittedHostTargetGeneration", generation);
        setField(hostState.m_surfaceMixin, "barrieww$m_lastCommittedHostImageHandle",
            s_firstHostImageHandle);

        try (MockedStatic<Minecraft> minecraftClass = mockStatic(Minecraft.class);
             MockedStatic<MinecraftVulkanBootstrapHandles> handlesAdapter =
                 mockStatic(MinecraftVulkanBootstrapHandles.class);
             MockedStatic<MinecraftHostImagePresentationBindingExtractor> bindingExtractor =
                 mockStatic(MinecraftHostImagePresentationBindingExtractor.class)) {
            prepareStaticAdapters(
                hostState,
                minecraftClass,
                handlesAdapter,
                bindingExtractor,
                binding(s_secondHostImageHandle));

            assertNull(invokePrepareGeneration(hostState.m_surfaceMixin, configuration()));
        }
    }

    /** Verifies configure publishes its pending generation-handle pair only after coordinator commit. */
    @Test
    void configureRecordsPairOnlyAfterCoordinatorCommit() throws Exception {
        HostPreparationState hostState = hostPreparationState(
            s_framebufferWidth,
            s_framebufferHeight);
        PresentationTakeoverCoordinator coordinator = mock(PresentationTakeoverCoordinator.class);
        AtomicReference<PresentationGenerationInputs> capturedInputs = new AtomicReference<>();
        setField(hostState.m_surfaceMixin,
            "barrieww$m_presentationTakeoverCoordinator",
            coordinator);
        publishGeneration();
        when(coordinator.configure(any(PresentationGenerationPreparation.class)))
            .thenAnswer(invocation -> {
                PresentationGenerationPreparation preparation = invocation.getArgument(0);
                capturedInputs.set(preparation.prepare());
                return true;
            });
        when(coordinator.hasCommittedHostImagePresentationGeneration()).thenReturn(true);

        try (MockedStatic<Minecraft> minecraftClass = mockStatic(Minecraft.class);
             MockedStatic<MinecraftVulkanBootstrapHandles> handlesAdapter =
                 mockStatic(MinecraftVulkanBootstrapHandles.class);
             MockedStatic<MinecraftHostImagePresentationBindingExtractor> bindingExtractor =
                 mockStatic(MinecraftHostImagePresentationBindingExtractor.class)) {
            prepareStaticAdapters(
                hostState,
                minecraftClass,
                handlesAdapter,
                bindingExtractor,
                binding(s_firstHostImageHandle));
            CallbackInfo callbackInformation = new CallbackInfo("configure", true);

            invokeConfigure(hostState.m_surfaceMixin, configuration(), callbackInformation);

            assertTrue(callbackInformation.isCancelled());
            assertEquals(capturedInputs.get().hostTargetGeneration(),
                getField(hostState.m_surfaceMixin,
                    "barrieww$m_lastCommittedHostTargetGeneration"));
            assertEquals(s_firstHostImageHandle,
                getField(hostState.m_surfaceMixin, "barrieww$m_lastCommittedHostImageHandle"));
        }
    }

    /** Verifies listener removal completes before the coordinator's sole close attempt. */
    @Test
    void closeUnregistersExactListenerBeforeCoordinatorClose() throws Exception {
        VulkanGpuSurfaceMixin surfaceMixin = new VulkanGpuSurfaceMixin() { };
        PresentationTakeoverCoordinator coordinator = mock(PresentationTakeoverCoordinator.class);
        MainRenderTargetResizeListener resizeListener = coordinator::detachHostImagePresentationResources;
        MainRenderTargetGenerationTracker.registerResizeListener(resizeListener);
        setField(surfaceMixin, "barrieww$m_presentationTakeoverCoordinator", coordinator);
        setField(surfaceMixin, "barrieww$m_mainRenderTargetResizeListener", resizeListener);
        doAnswer(invocation -> {
            assertFalse(MainRenderTargetGenerationTracker.unregisterResizeListener(resizeListener));
            return null;
        }).when(coordinator).close();

        invokeClose(surfaceMixin);

        verify(coordinator).close();
        assertTrue(MainRenderTargetGenerationTracker.beforeMainRenderTargetResize());
        assertNull(getField(surfaceMixin, "barrieww$m_mainRenderTargetResizeListener"));
    }

    /** Verifies listener registration failure closes and drops the unregistered coordinator. */
    @Test
    void constructorRegistrationFailureLeavesTakeoverDisabledWithoutRetention() throws Exception {
        VulkanGpuSurfaceMixin surfaceMixin = new VulkanGpuSurfaceMixin() { };
        VulkanDevice surfaceDevice = mock(VulkanDevice.class);
        MainRenderTargetResizeListener occupiedListener = () -> true;
        MainRenderTargetGenerationTracker.registerResizeListener(occupiedListener);
        setField(surfaceMixin, "m_device", surfaceDevice);
        setField(surfaceMixin, "m_surface", 4L);

        try (MockedStatic<MinecraftVulkanBootstrapHandles> handlesAdapter =
                 mockStatic(MinecraftVulkanBootstrapHandles.class);
             MockedStatic<BarriEwwClientInitializer> initializer =
                 mockStatic(BarriEwwClientInitializer.class)) {
            handlesAdapter.when(() -> MinecraftVulkanBootstrapHandles.extractBorrowedHandles(
                surfaceDevice,
                4L)).thenReturn(Optional.of(bootstrapHandles()));
            initializer.when(BarriEwwClientInitializer::nativeLibraryPath)
                .thenReturn(Optional.of(Path.of("C:/runtime/BarriEwwNativeFfm.dll")));

            invokeConstructorTail(surfaceMixin, surfaceDevice);

            assertNull(getField(surfaceMixin, "barrieww$m_presentationTakeoverCoordinator"));
            assertNull(getField(surfaceMixin, "barrieww$m_mainRenderTargetResizeListener"));
        } finally {
            MainRenderTargetGenerationTracker.unregisterResizeListener(occupiedListener);
        }
    }

    /** Creates initialized mocked host state and one surface mixin. */
    private static HostPreparationState hostPreparationState(int targetWidth, int targetHeight)
        throws Exception {
        VulkanGpuSurfaceMixin surfaceMixin = new VulkanGpuSurfaceMixin() { };
        VulkanDevice surfaceDevice = mock(VulkanDevice.class);
        Minecraft minecraft = mock(Minecraft.class);
        GameRenderer gameRenderer = mock(GameRenderer.class);
        RenderTarget mainRenderTarget = mock(RenderTarget.class);
        mainRenderTarget.width = targetWidth;
        mainRenderTarget.height = targetHeight;
        setField(minecraft, "gameRenderer", gameRenderer);
        when(gameRenderer.mainRenderTarget()).thenReturn(mainRenderTarget);
        setField(surfaceMixin, "m_device", surfaceDevice);
        setField(surfaceMixin, "m_surface", 4L);
        setField(surfaceMixin, "m_swapchainImageFormat", 44);
        return new HostPreparationState(
            surfaceMixin,
            surfaceDevice,
            minecraft,
            gameRenderer,
            mainRenderTarget);
    }

    /** Configures all static host adapters for one stable preparation. */
    private static void prepareStaticAdapters(
        HostPreparationState hostState,
        MockedStatic<Minecraft> minecraftClass,
        MockedStatic<MinecraftVulkanBootstrapHandles> handlesAdapter,
        MockedStatic<MinecraftHostImagePresentationBindingExtractor> bindingExtractor,
        HostImagePresentationBinding hostImageBinding) {
        minecraftClass.when(Minecraft::getInstance).thenReturn(hostState.m_minecraft);
        handlesAdapter.when(() -> MinecraftVulkanBootstrapHandles.extractBorrowedHandles(
            hostState.m_surfaceDevice,
            4L)).thenReturn(Optional.of(bootstrapHandles()));
        bindingExtractor.when(() -> MinecraftHostImagePresentationBindingExtractor.extract(
            hostState.m_gameRenderer,
            s_framebufferWidth,
            s_framebufferHeight,
            44)).thenReturn(Optional.ofNullable(hostImageBinding));
    }

    /** Publishes one positive current-target generation and returns it. */
    private static long publishGeneration() {
        Object target = new Object();
        MainRenderTargetGenerationTracker.mainRenderTargetResizeSucceeded(target, target);
        return MainRenderTargetGenerationTracker.currentGeneration();
    }

    /** Invokes the surface's post-retirement generation preparation callback. */
    private static PresentationGenerationInputs invokePrepareGeneration(
        VulkanGpuSurfaceMixin surfaceMixin,
        GpuSurface.Configuration configuration) throws Exception {
        Method preparationMethod = VulkanGpuSurfaceMixin.class.getDeclaredMethod(
            "barrieww$prepareGeneration",
            GpuSurface.Configuration.class);
        preparationMethod.setAccessible(true);
        return (PresentationGenerationInputs) invoke(
            preparationMethod,
            surfaceMixin,
            configuration);
    }

    /** Invokes the exact configure HEAD callback. */
    private static void invokeConfigure(
        VulkanGpuSurfaceMixin surfaceMixin,
        GpuSurface.Configuration configuration,
        CallbackInfo callbackInformation) throws Exception {
        Method configureMethod = VulkanGpuSurfaceMixin.class.getDeclaredMethod(
            "barrieww$configurePresentationTakeover",
            GpuSurface.Configuration.class,
            CallbackInfo.class);
        configureMethod.setAccessible(true);
        invoke(configureMethod, surfaceMixin, configuration, callbackInformation);
    }

    /** Invokes the exact constructor TAIL callback. */
    private static void invokeConstructorTail(
        VulkanGpuSurfaceMixin surfaceMixin,
        VulkanDevice surfaceDevice) throws Exception {
        Method constructorMethod = VulkanGpuSurfaceMixin.class.getDeclaredMethod(
            "barrieww$probeBorrowedBootstrapHandles",
            VulkanDevice.class,
            long.class,
            CallbackInfo.class);
        constructorMethod.setAccessible(true);
        invoke(constructorMethod, surfaceMixin, surfaceDevice, 9L, null);
    }

    /** Invokes the exact close HEAD callback. */
    private static void invokeClose(VulkanGpuSurfaceMixin surfaceMixin) throws Exception {
        Method closeMethod = VulkanGpuSurfaceMixin.class.getDeclaredMethod(
            "barrieww$closePresentationTakeover",
            CallbackInfo.class);
        closeMethod.setAccessible(true);
        invoke(closeMethod, surfaceMixin, new CallbackInfo("close", false));
    }

    /** Invokes one callback and unwraps its original checked failure. */
    private static Object invoke(Method method, Object receiver, Object... arguments)
        throws Exception {
        try {
            return method.invoke(receiver, arguments);
        } catch (InvocationTargetException invocationFailure) {
            if (invocationFailure.getCause() instanceof Exception cause) {
                throw cause;
            }
            throw invocationFailure;
        }
    }

    /** Assigns one test-owned private, final, shadow, or unique field. */
    private static void setField(Object receiver, String fieldName, Object value) throws Exception {
        Class<?> declaringClass = receiver.getClass();
        Field field = null;
        while (declaringClass != null && field == null) {
            try {
                field = declaringClass.getDeclaredField(fieldName);
            } catch (NoSuchFieldException missingField) {
                declaringClass = declaringClass.getSuperclass();
            }
        }
        if (field == null) {
            throw new NoSuchFieldException(fieldName);
        }
        field.setAccessible(true);
        field.set(receiver, value);
    }

    /** Reads one test-owned private or unique surface field. */
    private static Object getField(VulkanGpuSurfaceMixin surfaceMixin, String fieldName)
        throws Exception {
        Field field = VulkanGpuSurfaceMixin.class.getDeclaredField(fieldName);
        field.setAccessible(true);
        return field.get(surfaceMixin);
    }

    /** Returns fixed complete borrowed bootstrap handles. */
    private static PresentationBootstrapHandles bootstrapHandles() {
        return new PresentationBootstrapHandles(1L, 2L, 3L, 4L, 5L, 5L, 6, 6);
    }

    /** Returns one exact host image binding for the configured extent. */
    private static HostImagePresentationBinding binding(long hostImageHandle) {
        return new HostImagePresentationBinding(
            hostImageHandle,
            PresentationImageFormat.R8G8B8A8_UNORM,
            PresentationImageFormat.B8G8R8A8_UNORM,
            s_framebufferWidth,
            s_framebufferHeight);
    }

    /** Returns the fixed requested surface generation configuration. */
    private static GpuSurface.Configuration configuration() {
        return new GpuSurface.Configuration(
            s_framebufferWidth,
            s_framebufferHeight,
            GpuSurface.PresentMode.FIFO);
    }

    /** Groups one test-owned Minecraft host preparation state. */
    private static final class HostPreparationState {
        private final VulkanGpuSurfaceMixin m_surfaceMixin;
        private final VulkanDevice m_surfaceDevice;
        private final Minecraft m_minecraft;
        private final GameRenderer m_gameRenderer;
        private final RenderTarget m_mainRenderTarget;

        /** Captures one complete host preparation fixture. */
        private HostPreparationState(
            VulkanGpuSurfaceMixin surfaceMixin,
            VulkanDevice surfaceDevice,
            Minecraft minecraft,
            GameRenderer gameRenderer,
            RenderTarget mainRenderTarget) {
            m_surfaceMixin = surfaceMixin;
            m_surfaceDevice = surfaceDevice;
            m_minecraft = minecraft;
            m_gameRenderer = gameRenderer;
            m_mainRenderTarget = mainRenderTarget;
        }
    }
}
