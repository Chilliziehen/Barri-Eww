package barrieww.mod.mixin;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertNotNull;
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
import org.junit.jupiter.api.BeforeEach;
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

    /**
     * @note ThreadSafety: Runs serially before each lifecycle test on the test thread.
     * Restores the production initial target generation so startup and resize scenarios are isolated.
     *
     * @throws ReflectiveOperationException When the private tracker generation cannot be restored
     * @warning MemoryOwnership: Reflection borrows the static field and retains no tracker state.
     */
    @BeforeEach
    void resetGeneration() throws ReflectiveOperationException {
        Field generationField = MainRenderTargetGenerationTracker.class.getDeclaredField(
            "s_currentGeneration");
        generationField.setAccessible(true);
        generationField.setLong(null, 1L);
    }

    /** Verifies startup accepts the already-sized initial target without a resize notification. */
    @Test
    void startupPreparesExactInitialTargetWithoutPriorResizeNotification() throws Exception {
        HostPreparationState hostState = hostPreparationState(
            s_framebufferWidth,
            s_framebufferHeight);
        PresentationTakeoverCoordinator coordinator = mock(PresentationTakeoverCoordinator.class);
        AtomicReference<PresentationGenerationInputs> capturedInputs = new AtomicReference<>();
        setField(hostState.m_surfaceMixin,
            "barrieww$m_presentationTakeoverCoordinator",
            coordinator);
        when(coordinator.configure(any(PresentationGenerationPreparation.class)))
            .thenAnswer(invocation -> {
                PresentationGenerationPreparation preparation = invocation.getArgument(0);
                capturedInputs.set(preparation.prepare());
                return capturedInputs.get() != null;
            });

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

            invokeConfigure(
                hostState.m_surfaceMixin,
                configuration(),
                callbackInformation);

            PresentationGenerationInputs inputs = capturedInputs.get();
            assertNotNull(inputs);
            assertEquals(1L, inputs.hostTargetGeneration());
            assertEquals(s_firstHostImageHandle, inputs.hostImageBinding().hostImageHandle());
            verify(coordinator).configure(any(PresentationGenerationPreparation.class));
            assertTrue(callbackInformation.isCancelled());
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

    /**
     * @note ThreadSafety: Test-thread-confined; creates independent mocks for one invocation.
     * Creates initialized Minecraft host state and one surface mixin with the requested target extent.
     *
     * @param int targetWidth Initial mocked main-target width in pixels
     * @param int targetHeight Initial mocked main-target height in pixels
     * @return HostPreparationState Complete test-owned host preparation fixture
     * @throws Exception When reflective assignment of Minecraft or mixin state fails
     * @warning MemoryOwnership: The returned fixture owns only mocks and borrowed scalar handles.
     */
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

    /**
     * @note ThreadSafety: Test-thread-confined while scoped static mocks are active.
     * Configures Minecraft, bootstrap, and binding adapters for one stable preparation attempt.
     *
     * @param HostPreparationState hostState Test-owned Minecraft host fixture
     * @param MockedStatic<Minecraft> minecraftClass Scoped Minecraft singleton replacement
     * @param MockedStatic<MinecraftVulkanBootstrapHandles> handlesAdapter Scoped bootstrap adapter
     * @param MockedStatic<MinecraftHostImagePresentationBindingExtractor> bindingExtractor Scoped
     * host-image binding extractor
     * @param HostImagePresentationBinding hostImageBinding Extracted binding, or null for absence
     * @warning MemoryOwnership: Scoped mocks and fixture resources remain owned by the caller.
     */
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

    /**
     * @note ThreadSafety: Test-thread-confined and serialized with all tracker access.
     * Simulates one successful current-main-target resize notification for non-startup scenarios.
     *
     * @return long Monotonically advanced tracker generation
     * @warning MemoryOwnership: Temporary target identities are not retained by the tracker.
     */
    private static long publishGeneration() {
        Object target = new Object();
        MainRenderTargetGenerationTracker.mainRenderTargetResizeSucceeded(target, target);
        return MainRenderTargetGenerationTracker.currentGeneration();
    }

    /**
     * @note ThreadSafety: Test-thread-confined; invokes one isolated mixin instance reflectively.
     * Invokes the surface post-retirement generation preparation callback with exact configuration.
     *
     * @param VulkanGpuSurfaceMixin surfaceMixin Test-owned surface mixin instance
     * @param GpuSurface.Configuration configuration Requested framebuffer configuration
     * @return PresentationGenerationInputs Prepared inputs, or null when readiness validation fails
     * @throws Exception When callback lookup, access, or invocation fails
     * @warning MemoryOwnership: Returned inputs contain borrowed scalar handles from test fixtures.
     */
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

    /**
     * @note ThreadSafety: Test-thread-confined; invokes one isolated mixin instance reflectively.
     * Invokes the exact cancellable Vulkan surface configure HEAD callback.
     *
     * @param VulkanGpuSurfaceMixin surfaceMixin Test-owned surface mixin instance
     * @param GpuSurface.Configuration configuration Requested framebuffer configuration
     * @param CallbackInfo callbackInformation Cancellable callback state to inspect
     * @throws Exception When callback lookup, access, or invocation fails
     * @warning MemoryOwnership: Callback and mixin remain test-owned throughout invocation.
     */
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

    /**
     * @note ThreadSafety: Test-thread-confined; invokes one isolated mixin instance reflectively.
     * Invokes the exact Vulkan surface constructor TAIL callback.
     *
     * @param VulkanGpuSurfaceMixin surfaceMixin Test-owned surface mixin instance
     * @param VulkanDevice surfaceDevice Mocked Minecraft Vulkan device constructor argument
     * @throws Exception When callback lookup, access, or invocation fails
     * @warning MemoryOwnership: The mocked device and its scalar handles remain test-owned.
     */
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

    /**
     * @note ThreadSafety: Test-thread-confined; invokes one isolated mixin instance reflectively.
     * Invokes the exact non-cancellable Vulkan surface close HEAD callback.
     *
     * @param VulkanGpuSurfaceMixin surfaceMixin Test-owned surface mixin instance
     * @throws Exception When callback lookup, access, or invocation fails
     * @warning MemoryOwnership: The callback triggers the mixin's tested coordinator release policy.
     */
    private static void invokeClose(VulkanGpuSurfaceMixin surfaceMixin) throws Exception {
        Method closeMethod = VulkanGpuSurfaceMixin.class.getDeclaredMethod(
            "barrieww$closePresentationTakeover",
            CallbackInfo.class);
        closeMethod.setAccessible(true);
        invoke(closeMethod, surfaceMixin, new CallbackInfo("close", false));
    }

    /**
     * @note ThreadSafety: Test-thread-confined; mutates only accessibility of the supplied method.
     * Invokes one callback and unwraps an original checked exception for precise test diagnostics.
     *
     * @param Method method Accessible reflected callback method
     * @param Object receiver Test-owned callback receiver
     * @param Object[] arguments Exact callback arguments
     * @return Object Reflected callback return value, or null for void callbacks
     * @throws Exception When the callback or reflection boundary reports a checked failure
     * @warning MemoryOwnership: Arguments and return values remain owned by their test fixtures.
     */
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

    /**
     * @note ThreadSafety: Test-thread-confined; mutates one isolated receiver through reflection.
     * Assigns a named private, final, shadow, or unique field found in the receiver hierarchy.
     *
     * @param Object receiver Test-owned object whose field is assigned
     * @param String fieldName Exact declared field name
     * @param Object value Replacement field value
     * @throws Exception When field lookup, access, or assignment fails
     * @warning MemoryOwnership: Assignment transfers no ownership beyond ordinary test references.
     */
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

    /**
     * @note ThreadSafety: Test-thread-confined; reads one isolated surface mixin instance.
     * Reads a named private or unique field declared by the surface mixin.
     *
     * @param VulkanGpuSurfaceMixin surfaceMixin Test-owned surface mixin instance
     * @param String fieldName Exact declared field name
     * @return Object Current field value
     * @throws Exception When field lookup, access, or reading fails
     * @warning MemoryOwnership: The returned reference remains owned by the mixin or test fixture.
     */
    private static Object getField(VulkanGpuSurfaceMixin surfaceMixin, String fieldName)
        throws Exception {
        Field field = VulkanGpuSurfaceMixin.class.getDeclaredField(fieldName);
        field.setAccessible(true);
        return field.get(surfaceMixin);
    }

    /**
     * @note ThreadSafety: Pure immutable test-value construction is concurrency-safe.
     * Creates fixed complete borrowed bootstrap handles for one preparation scenario.
     *
     * @return PresentationBootstrapHandles Complete borrowed scalar handle fixture
     * @warning MemoryOwnership: Primitive handle values own no Native or Minecraft resources.
     */
    private static PresentationBootstrapHandles bootstrapHandles() {
        return new PresentationBootstrapHandles(1L, 2L, 3L, 4L, 5L, 5L, 6, 6);
    }

    /**
     * @note ThreadSafety: Pure immutable test-value construction is concurrency-safe.
     * Creates one exact host-image binding for the fixed configured extent.
     *
     * @param long hostImageHandle Borrowed nonzero host Vulkan image handle
     * @return HostImagePresentationBinding Exact borrowed image binding fixture
     * @warning MemoryOwnership: The primitive image handle owns no Native or Minecraft resource.
     */
    private static HostImagePresentationBinding binding(long hostImageHandle) {
        return new HostImagePresentationBinding(
            hostImageHandle,
            PresentationImageFormat.R8G8B8A8_UNORM,
            PresentationImageFormat.B8G8R8A8_UNORM,
            s_framebufferWidth,
            s_framebufferHeight);
    }

    /**
     * @note ThreadSafety: Pure immutable test-value construction is concurrency-safe.
     * Creates the fixed requested surface generation configuration.
     *
     * @return GpuSurface.Configuration Fixed positive framebuffer configuration
     * @warning MemoryOwnership: The returned immutable record is owned by the calling test.
     */
    private static GpuSurface.Configuration configuration() {
        return new GpuSurface.Configuration(
            s_framebufferWidth,
            s_framebufferHeight,
            GpuSurface.PresentMode.FIFO);
    }

    /**
     * @note ThreadSafety: Instances are confined to one test thread and never published.
     * Groups one complete test-owned Minecraft host preparation state.
     * @warning MemoryOwnership: The fixture owns only mock references and no Native resource.
     */
    private static final class HostPreparationState {
        private final VulkanGpuSurfaceMixin m_surfaceMixin;
        private final VulkanDevice m_surfaceDevice;
        private final Minecraft m_minecraft;
        private final GameRenderer m_gameRenderer;
        private final RenderTarget m_mainRenderTarget;

        /**
         * @note ThreadSafety: Construction and use are confined to one test invocation.
         * Captures one complete host preparation fixture without transferring resource ownership.
         *
         * @param VulkanGpuSurfaceMixin surfaceMixin Test-owned surface mixin instance
         * @param VulkanDevice surfaceDevice Mocked Minecraft Vulkan device
         * @param Minecraft minecraft Mocked Minecraft singleton
         * @param GameRenderer gameRenderer Mocked current game renderer
         * @param RenderTarget mainRenderTarget Mocked current main render target
         * @warning MemoryOwnership: Every supplied mock remains owned by the creating test fixture.
         */
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
