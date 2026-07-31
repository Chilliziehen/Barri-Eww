package barrieww.mod.mixin;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertNotNull;
import static org.junit.jupiter.api.Assertions.assertNull;
import static org.junit.jupiter.api.Assertions.assertSame;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.mockito.ArgumentMatchers.contains;
import static org.mockito.ArgumentMatchers.same;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.mockStatic;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import barrieww.core.interoperability.NativePresentationRuntimeException;
import barrieww.core.interoperability.PresentationBootstrapHandles;
import barrieww.mod.BarriEwwClientInitializer;
import barrieww.mod.MinecraftClearTakeoverReadiness;
import barrieww.mod.MinecraftVulkanBootstrapHandles;
import barrieww.mod.PresentationGenerationInputs;
import barrieww.mod.PresentationTakeoverCoordinator;
import com.mojang.blaze3d.systems.CommandEncoderBackend;
import com.mojang.blaze3d.systems.GpuSurface;
import com.mojang.blaze3d.textures.GpuTextureView;
import com.mojang.blaze3d.vulkan.VulkanDevice;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.file.Path;
import java.util.Optional;
import org.junit.jupiter.api.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.MockedStatic;
import org.slf4j.Logger;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * @note ThreadSafety: Tests mutate isolated mixin instances and scoped static mocks serially.
 * Verifies the thin VulkanGpuSurface backend interception policy and callback state propagation.
 */
final class VulkanGpuSurfaceMixinTests {
    private static final String s_coordinatorFieldName =
        "barrieww$m_presentationTakeoverCoordinator";
    private static final String s_loggerFieldName = "barrieww$m_logger";
    private static final String s_closeEscalationFieldName = "barrieww$m_closeEscalation";

    /**
     * @note ThreadSafety: Not concurrency-safe because scoped static adapters are replaced.
     * Retains borrowed-handle probing and creates one coordinator only when Native is ready.
     *
     * @throws ReflectiveOperationException When injected callback state cannot be inspected
     * @warning MemoryOwnership: Mocks and borrowed primitive handles own no Native resource.
     */
    @Test
    void constructorTailCreatesCoordinatorWhenNativePathIsPresent()
        throws ReflectiveOperationException {
        VulkanGpuSurfaceMixin surfaceMixin = newSurfaceMixin();
        VulkanDevice surfaceDevice = mock(VulkanDevice.class);
        setField(surfaceMixin, "m_device", surfaceDevice);
        setField(surfaceMixin, "m_surface", 4L);
        PresentationBootstrapHandles bootstrapHandles = bootstrapHandles();
        Path nativeLibraryPath = Path.of("C:/runtime/BarriEwwNativeFfm.dll");

        try (MockedStatic<MinecraftVulkanBootstrapHandles> handlesAdapter =
                 mockStatic(MinecraftVulkanBootstrapHandles.class);
             MockedStatic<BarriEwwClientInitializer> initializer =
                 mockStatic(BarriEwwClientInitializer.class)) {
            handlesAdapter.when(() -> MinecraftVulkanBootstrapHandles.extractBorrowedHandles(
                surfaceDevice,
                4L)).thenReturn(Optional.of(bootstrapHandles));
            initializer.when(BarriEwwClientInitializer::nativeLibraryPath)
                .thenReturn(Optional.of(nativeLibraryPath));

            invokeConstructorTail(surfaceMixin, surfaceDevice);

            assertNotNull(getField(surfaceMixin, s_coordinatorFieldName));
            handlesAdapter.verify(() -> MinecraftVulkanBootstrapHandles.extractBorrowedHandles(
                surfaceDevice,
                4L));
        }
    }

    /**
     * @note ThreadSafety: Not concurrency-safe because scoped static adapters are replaced.
     * Leaves the coordinator absent while retaining non-destructive handle probing when Native is absent.
     *
     * @throws ReflectiveOperationException When injected callback state cannot be inspected
     * @warning MemoryOwnership: Mocks and borrowed primitive handles own no Native resource.
     */
    @Test
    void constructorTailLeavesCoordinatorAbsentWithoutNativePath()
        throws ReflectiveOperationException {
        VulkanGpuSurfaceMixin surfaceMixin = newSurfaceMixin();
        VulkanDevice surfaceDevice = mock(VulkanDevice.class);
        setField(surfaceMixin, "m_device", surfaceDevice);
        setField(surfaceMixin, "m_surface", 4L);

        try (MockedStatic<MinecraftVulkanBootstrapHandles> handlesAdapter =
                 mockStatic(MinecraftVulkanBootstrapHandles.class);
             MockedStatic<BarriEwwClientInitializer> initializer =
                 mockStatic(BarriEwwClientInitializer.class)) {
            handlesAdapter.when(() -> MinecraftVulkanBootstrapHandles.extractBorrowedHandles(
                surfaceDevice,
                4L)).thenReturn(Optional.empty());
            initializer.when(BarriEwwClientInitializer::nativeLibraryPath)
                .thenReturn(Optional.empty());

            invokeConstructorTail(surfaceMixin, surfaceDevice);

            assertNull(getField(surfaceMixin, s_coordinatorFieldName));
            handlesAdapter.verify(() -> MinecraftVulkanBootstrapHandles.extractBorrowedHandles(
                surfaceDevice,
                4L));
        }
    }

    /**
     * @note ThreadSafety: Not concurrency-safe because scoped static adapters are replaced.
     * Uses the requested Native extent and cancels after accepting static clear-takeover readiness,
     * even while Minecraft still owns an older host texture extent.
     *
     * @throws ReflectiveOperationException When injected callback state cannot be inspected
     * @warning MemoryOwnership: All handles and callback state are test-owned or borrowed mocks.
     */
    @Test
    void configureUsesRequestedExtentWhenOlderHostTextureIsReady()
        throws ReflectiveOperationException {
        VulkanGpuSurfaceMixin surfaceMixin = newSurfaceMixin();
        VulkanDevice surfaceDevice = mock(VulkanDevice.class);
        PresentationTakeoverCoordinator coordinator = mock(PresentationTakeoverCoordinator.class);
        PresentationBootstrapHandles bootstrapHandles = bootstrapHandles();
        GpuSurface.Configuration configuration = new GpuSurface.Configuration(
            1100,
            700,
            GpuSurface.PresentMode.FIFO);
        CallbackInfo callbackInformation = new CallbackInfo("configure", true);
        setField(surfaceMixin, "m_device", surfaceDevice);
        setField(surfaceMixin, "m_surface", 4L);
        setField(surfaceMixin, s_coordinatorFieldName, coordinator);
        setField(surfaceMixin, "m_swapchainSuboptimal", true);
        when(coordinator.configure(org.mockito.ArgumentMatchers.any())).thenReturn(true);
        when(coordinator.requiresReconfiguration()).thenReturn(false);

        try (MockedStatic<MinecraftVulkanBootstrapHandles> handlesAdapter =
                 mockStatic(MinecraftVulkanBootstrapHandles.class);
             MockedStatic<MinecraftClearTakeoverReadiness> textureReadiness =
                 mockStatic(MinecraftClearTakeoverReadiness.class)) {
            handlesAdapter.when(() -> MinecraftVulkanBootstrapHandles.extractBorrowedHandles(
                surfaceDevice,
                4L)).thenReturn(Optional.of(bootstrapHandles));
            textureReadiness.when(
                MinecraftClearTakeoverReadiness::isMinecraftColorTextureReady)
                .thenReturn(true);

            invokeConfigure(surfaceMixin, configuration, callbackInformation);

            ArgumentCaptor<PresentationGenerationInputs> inputsCaptor =
                ArgumentCaptor.forClass(PresentationGenerationInputs.class);
            verify(coordinator).configure(inputsCaptor.capture());
            PresentationGenerationInputs inputs = inputsCaptor.getValue();
            assertSame(bootstrapHandles, inputs.bootstrapHandles());
            assertEquals(1100, inputs.framebufferWidth());
            assertEquals(700, inputs.framebufferHeight());
            assertTrue(inputs.isNativeLibraryReady());
            assertTrue(inputs.isHostColorTextureReady());
            assertTrue(callbackInformation.isCancelled());
            assertFalse((boolean) getField(surfaceMixin, "m_swapchainSuboptimal"));
        }
    }

    /**
     * @note ThreadSafety: Not concurrency-safe because scoped static adapters are replaced.
     * Allows vanilla configure and mirrors the coordinator's safe non-suboptimal fallback state.
     *
     * @throws ReflectiveOperationException When injected callback state cannot be inspected
     * @warning MemoryOwnership: All handles and callback state are test-owned or borrowed mocks.
     */
    @Test
    void configureAllowsVanillaFallbackWithoutSchedulingReconfiguration()
        throws ReflectiveOperationException {
        VulkanGpuSurfaceMixin surfaceMixin = newSurfaceMixin();
        VulkanDevice surfaceDevice = mock(VulkanDevice.class);
        PresentationTakeoverCoordinator coordinator = mock(PresentationTakeoverCoordinator.class);
        CallbackInfo callbackInformation = new CallbackInfo("configure", true);
        setField(surfaceMixin, "m_device", surfaceDevice);
        setField(surfaceMixin, "m_surface", 4L);
        setField(surfaceMixin, s_coordinatorFieldName, coordinator);
        when(coordinator.configure(org.mockito.ArgumentMatchers.any())).thenReturn(false);
        when(coordinator.requiresReconfiguration()).thenReturn(false);

        try (MockedStatic<MinecraftVulkanBootstrapHandles> handlesAdapter =
                 mockStatic(MinecraftVulkanBootstrapHandles.class);
             MockedStatic<MinecraftClearTakeoverReadiness> textureReadiness =
                 mockStatic(MinecraftClearTakeoverReadiness.class)) {
            handlesAdapter.when(() -> MinecraftVulkanBootstrapHandles.extractBorrowedHandles(
                surfaceDevice,
                4L)).thenReturn(Optional.empty());
            textureReadiness.when(
                MinecraftClearTakeoverReadiness::isMinecraftColorTextureReady)
                .thenReturn(false);

            invokeConfigure(surfaceMixin, configuration(), callbackInformation);

            ArgumentCaptor<PresentationGenerationInputs> inputsCaptor =
                ArgumentCaptor.forClass(PresentationGenerationInputs.class);
            verify(coordinator).configure(inputsCaptor.capture());
            assertNull(inputsCaptor.getValue().bootstrapHandles());
            assertFalse(inputsCaptor.getValue().isHostColorTextureReady());
            assertFalse(callbackInformation.isCancelled());
            assertFalse((boolean) getField(surfaceMixin, "m_swapchainSuboptimal"));
        }
    }

    /**
     * @note ThreadSafety: Not concurrency-safe because scoped static adapters are replaced.
     * Cancels vanilla configure for terminal interception without scheduling a stale recreation.
     *
     * @throws ReflectiveOperationException When injected callback state cannot be inspected
     * @warning MemoryOwnership: All handles and callback state are test-owned or borrowed mocks.
     */
    @Test
    void configureTerminalInterceptionDoesNotScheduleReconfiguration()
        throws ReflectiveOperationException {
        VulkanGpuSurfaceMixin surfaceMixin = newSurfaceMixin();
        VulkanDevice surfaceDevice = mock(VulkanDevice.class);
        PresentationTakeoverCoordinator coordinator = mock(PresentationTakeoverCoordinator.class);
        CallbackInfo callbackInformation = new CallbackInfo("configure", true);
        setField(surfaceMixin, "m_device", surfaceDevice);
        setField(surfaceMixin, "m_surface", 4L);
        setField(surfaceMixin, "m_swapchainSuboptimal", true);
        setField(surfaceMixin, s_coordinatorFieldName, coordinator);
        when(coordinator.configure(org.mockito.ArgumentMatchers.any())).thenReturn(true);
        when(coordinator.requiresReconfiguration()).thenReturn(false);

        try (MockedStatic<MinecraftVulkanBootstrapHandles> handlesAdapter =
                 mockStatic(MinecraftVulkanBootstrapHandles.class);
             MockedStatic<MinecraftClearTakeoverReadiness> textureReadiness =
                 mockStatic(MinecraftClearTakeoverReadiness.class)) {
            handlesAdapter.when(() -> MinecraftVulkanBootstrapHandles.extractBorrowedHandles(
                surfaceDevice,
                4L)).thenReturn(Optional.of(bootstrapHandles()));
            textureReadiness.when(
                MinecraftClearTakeoverReadiness::isMinecraftColorTextureReady)
                .thenReturn(true);

            invokeConfigure(surfaceMixin, configuration(), callbackInformation);

            assertTrue(callbackInformation.isCancelled());
            assertFalse((boolean) getField(surfaceMixin, "m_swapchainSuboptimal"));
        }
    }

    /**
     * @note ThreadSafety: Reflection mutates one test-owned mixin instance.
     * Leaves configure untouched when no Native path created a coordinator.
     *
     * @throws ReflectiveOperationException When injected callback state cannot be inspected
     */
    @Test
    void configureWithoutCoordinatorLeavesVanillaUntouched() throws ReflectiveOperationException {
        VulkanGpuSurfaceMixin surfaceMixin = newSurfaceMixin();
        CallbackInfo callbackInformation = new CallbackInfo("configure", true);

        invokeConfigure(surfaceMixin, configuration(), callbackInformation);

        assertFalse(callbackInformation.isCancelled());
    }

    /**
     * @note ThreadSafety: Reflection mutates one test-owned mixin instance.
     * Begins Native acquisition before cancellation and propagates reconfiguration state.
     *
     * @throws ReflectiveOperationException When injected callback state cannot be inspected
     */
    @Test
    void acquireBeginsFrameBeforeCancellingVanilla() throws ReflectiveOperationException {
        VulkanGpuSurfaceMixin surfaceMixin = newSurfaceMixin();
        PresentationTakeoverCoordinator coordinator = mock(PresentationTakeoverCoordinator.class);
        CallbackInfo callbackInformation = mock(CallbackInfo.class);
        setField(surfaceMixin, s_coordinatorFieldName, coordinator);
        when(coordinator.isTakenOver()).thenReturn(true);
        when(coordinator.requiresReconfiguration()).thenReturn(true);

        invokeNoArgumentCallback(
            surfaceMixin, "barrieww$acquirePresentationTexture", callbackInformation);

        InOrder callOrder = inOrder(coordinator, callbackInformation);
        callOrder.verify(coordinator).isTakenOver();
        callOrder.verify(coordinator).beginFrame();
        callOrder.verify(coordinator).requiresReconfiguration();
        callOrder.verify(callbackInformation).cancel();
        assertTrue((boolean) getField(surfaceMixin, "m_swapchainSuboptimal"));
    }

    /**
     * @note ThreadSafety: Reflection mutates one test-owned mixin instance.
     * Allows vanilla acquisition when the current generation is not taken over.
     *
     * @throws ReflectiveOperationException When injected callback state cannot be inspected
     */
    @Test
    void acquireAllowsVanillaWhenNotTakenOver() throws ReflectiveOperationException {
        VulkanGpuSurfaceMixin surfaceMixin = newSurfaceMixin();
        PresentationTakeoverCoordinator coordinator = mock(PresentationTakeoverCoordinator.class);
        CallbackInfo callbackInformation = mock(CallbackInfo.class);
        setField(surfaceMixin, s_coordinatorFieldName, coordinator);

        invokeNoArgumentCallback(
            surfaceMixin, "barrieww$acquirePresentationTexture", callbackInformation);

        verify(coordinator, never()).beginFrame();
        verify(callbackInformation, never()).cancel();
    }

    /**
     * @note ThreadSafety: Reflection mutates one test-owned mixin instance.
     * Cancels only the Vulkan backend blit as an empty operation during takeover.
     *
     * @throws ReflectiveOperationException When injected callback state cannot be inspected
     */
    @Test
    void blitCancelsAsEmptyOnlyDuringTakeover() throws ReflectiveOperationException {
        VulkanGpuSurfaceMixin surfaceMixin = newSurfaceMixin();
        PresentationTakeoverCoordinator coordinator = mock(PresentationTakeoverCoordinator.class);
        CallbackInfo callbackInformation = new CallbackInfo("blitFromTexture", true);
        setField(surfaceMixin, s_coordinatorFieldName, coordinator);
        when(coordinator.isTakenOver()).thenReturn(true);

        Method blitMethod = VulkanGpuSurfaceMixin.class.getDeclaredMethod(
            "barrieww$skipVanillaBlit",
            CommandEncoderBackend.class,
            GpuTextureView.class,
            CallbackInfo.class);
        blitMethod.setAccessible(true);
        invoke(blitMethod, surfaceMixin, null, null, callbackInformation);

        assertTrue(callbackInformation.isCancelled());
    }

    /**
     * @note ThreadSafety: Reflection mutates one test-owned mixin instance.
     * Presents Native work before cancellation and propagates reconfiguration state.
     *
     * @throws ReflectiveOperationException When injected callback state cannot be inspected
     */
    @Test
    void presentSubmitsFrameBeforeCancellingVanilla() throws ReflectiveOperationException {
        VulkanGpuSurfaceMixin surfaceMixin = newSurfaceMixin();
        PresentationTakeoverCoordinator coordinator = mock(PresentationTakeoverCoordinator.class);
        CallbackInfo callbackInformation = mock(CallbackInfo.class);
        setField(surfaceMixin, s_coordinatorFieldName, coordinator);
        when(coordinator.isTakenOver()).thenReturn(true);
        when(coordinator.requiresReconfiguration()).thenReturn(true);

        invokeNoArgumentCallback(
            surfaceMixin, "barrieww$presentTakeoverFrame", callbackInformation);

        InOrder callOrder = inOrder(coordinator, callbackInformation);
        callOrder.verify(coordinator).isTakenOver();
        callOrder.verify(coordinator).presentFrame();
        callOrder.verify(coordinator).requiresReconfiguration();
        callOrder.verify(callbackInformation).cancel();
        assertTrue((boolean) getField(surfaceMixin, "m_swapchainSuboptimal"));
    }

    /**
     * @note ThreadSafety: Reflection mutates one test-owned mixin instance.
     * Closes the coordinator at backend close HEAD without cancelling vanilla teardown.
     *
     * @throws Exception When injected callback state cannot be inspected or close verification fails
     */
    @Test
    void closeReleasesCoordinatorWithoutCancellingVanilla() throws Exception {
        VulkanGpuSurfaceMixin surfaceMixin = newSurfaceMixin();
        PresentationTakeoverCoordinator coordinator = mock(PresentationTakeoverCoordinator.class);
        Logger logger = mock(Logger.class);
        CallbackInfo callbackInformation = mock(CallbackInfo.class);
        setField(surfaceMixin, s_coordinatorFieldName, coordinator);
        setField(surfaceMixin, s_loggerFieldName, logger);

        invokeNoArgumentCallback(
            surfaceMixin, "barrieww$closePresentationTakeover", callbackInformation);
        invokeNoArgumentCallback(
            surfaceMixin, "barrieww$closePresentationTakeover", callbackInformation);

        verify(coordinator, org.mockito.Mockito.times(1)).close();
        verify(logger).info(
            "Presentation takeover closed before Minecraft Vulkan surface teardown");
        verifyNoMoreInteractions(logger);
        assertNull(getField(surfaceMixin, s_coordinatorFieldName));
        verify(callbackInformation, never()).cancel();
    }

    /**
     * @note ThreadSafety: Reflection mutates one test-owned mixin instance and logger mock.
     * Recovers one checked close failure with one bounded retry and exactly one WARN log.
     *
     * @throws Exception When injected callback state cannot be inspected or close stubbing fails
     */
    @Test
    void closeRecoversFirstFailureWithOneRetryAndWarning() throws Exception {
        VulkanGpuSurfaceMixin surfaceMixin = newSurfaceMixin();
        PresentationTakeoverCoordinator coordinator = mock(PresentationTakeoverCoordinator.class);
        Logger logger = mock(Logger.class);
        NativePresentationRuntimeException closeFailure =
            new NativePresentationRuntimeException("destroy failed", "destroySymbol", 7, -4);
        setField(surfaceMixin, s_coordinatorFieldName, coordinator);
        setField(surfaceMixin, s_loggerFieldName, logger);
        org.mockito.Mockito.doThrow(closeFailure).doNothing().when(coordinator).close();

        CallbackInfo callbackInformation = mock(CallbackInfo.class);
        invokeNoArgumentCallback(
            surfaceMixin, "barrieww$closePresentationTakeover", callbackInformation);
        invokeNoArgumentCallback(
            surfaceMixin, "barrieww$closePresentationTakeover", callbackInformation);

        verify(logger).warn(
            contains("recovered on bounded retry"),
            same(closeFailure));
        verify(coordinator, org.mockito.Mockito.times(2)).close();
        verifyNoMoreInteractions(logger);
        assertNull(getField(surfaceMixin, s_coordinatorFieldName));
        assertNull(getField(surfaceMixin, s_closeEscalationFieldName));
    }

    /**
     * @note ThreadSafety: Reflection mutates one test-owned mixin instance and logger mock.
     * Escalates two distinct close failures once and rethrows the stored escalation on repetition.
     *
     * @throws Exception When injected callback state cannot be inspected or close stubbing fails
     */
    @Test
    void closeEscalatesPersistentFailureBeforeVanillaTeardown() throws Exception {
        VulkanGpuSurfaceMixin surfaceMixin = newSurfaceMixin();
        PresentationTakeoverCoordinator coordinator = mock(PresentationTakeoverCoordinator.class);
        Logger logger = mock(Logger.class);
        NativePresentationRuntimeException firstFailure =
            new NativePresentationRuntimeException("first", "firstSymbol", 7, -4);
        NativePresentationRuntimeException secondFailure =
            new NativePresentationRuntimeException("second", "secondSymbol", 8, -5);
        setField(surfaceMixin, s_coordinatorFieldName, coordinator);
        setField(surfaceMixin, s_loggerFieldName, logger);
        org.mockito.Mockito.doThrow(firstFailure).doThrow(secondFailure)
            .when(coordinator).close();
        CallbackInfo callbackInformation = mock(CallbackInfo.class);

        InvocationTargetException firstInvocation = assertThrows(
            InvocationTargetException.class,
            () -> invokeNoArgumentCallback(
                surfaceMixin, "barrieww$closePresentationTakeover", callbackInformation));
        IllegalStateException escalation =
            (IllegalStateException) firstInvocation.getCause();
        InvocationTargetException repeatedInvocation = assertThrows(
            InvocationTargetException.class,
            () -> invokeNoArgumentCallback(
                surfaceMixin, "barrieww$closePresentationTakeover", callbackInformation));

        assertSame(escalation, repeatedInvocation.getCause());
        assertSame(secondFailure, escalation.getCause());
        assertSame(firstFailure, secondFailure.getSuppressed()[0]);
        assertSame(escalation, getField(surfaceMixin, s_closeEscalationFieldName));
        verify(logger).error(
            contains("Native symbol='secondSymbol', operation result=8, Vulkan result=-5"),
            same(secondFailure));
        verify(coordinator, org.mockito.Mockito.times(2)).close();
        verifyNoMoreInteractions(logger);
        assertSame(coordinator, getField(surfaceMixin, s_coordinatorFieldName));
    }

    /**
     * @note ThreadSafety: Reflection mutates one test-owned mixin instance and logger mock.
     * Avoids Java self-suppression when both bounded close attempts throw the same failure object.
     *
     * @throws Exception When injected callback state cannot be inspected or close stubbing fails
     */
    @Test
    void closeEscalationAvoidsSelfSuppressionForIdenticalFailures() throws Exception {
        VulkanGpuSurfaceMixin surfaceMixin = newSurfaceMixin();
        PresentationTakeoverCoordinator coordinator = mock(PresentationTakeoverCoordinator.class);
        Logger logger = mock(Logger.class);
        NativePresentationRuntimeException sharedFailure =
            new NativePresentationRuntimeException("shared", "destroySymbol", 7, -4);
        setField(surfaceMixin, s_coordinatorFieldName, coordinator);
        setField(surfaceMixin, s_loggerFieldName, logger);
        org.mockito.Mockito.doThrow(sharedFailure).when(coordinator).close();

        InvocationTargetException invocationFailure = assertThrows(
            InvocationTargetException.class,
            () -> invokeNoArgumentCallback(surfaceMixin,
                "barrieww$closePresentationTakeover", mock(CallbackInfo.class)));

        assertSame(sharedFailure, invocationFailure.getCause().getCause());
        assertEquals(0, sharedFailure.getSuppressed().length);
        verify(logger).error(
            contains("Native symbol='destroySymbol', operation result=7, Vulkan result=-4"),
            same(sharedFailure));
        verify(coordinator, org.mockito.Mockito.times(2)).close();
        verifyNoMoreInteractions(logger);
        assertSame(coordinator, getField(surfaceMixin, s_coordinatorFieldName));
    }

    /**
     * @note ThreadSafety: Creates one test-confined anonymous mixin instance.
     * Creates the otherwise abstract mixin test host.
     *
     * @return VulkanGpuSurfaceMixin Test-owned mixin instance
     */
    private static VulkanGpuSurfaceMixin newSurfaceMixin() {
        return new VulkanGpuSurfaceMixin() { };
    }

    /**
     * @note ThreadSafety: Immutable test value with borrowed primitive addresses.
     * Creates complete validated bootstrap handles.
     *
     * @return PresentationBootstrapHandles Complete test bootstrap handles
     * @warning MemoryOwnership: Primitive addresses own no Native objects.
     */
    private static PresentationBootstrapHandles bootstrapHandles() {
        return new PresentationBootstrapHandles(1L, 2L, 3L, 4L, 5L, 5L, 6, 6);
    }

    /**
     * @note ThreadSafety: Returns an immutable test configuration.
     * Creates the fixed configured extent used by callback tests.
     *
     * @return GpuSurface.Configuration Fixed test configuration
     */
    private static GpuSurface.Configuration configuration() {
        return new GpuSurface.Configuration(1280, 720, GpuSurface.PresentMode.FIFO);
    }

    /**
     * @note ThreadSafety: Reflection invokes one test-confined mixin instance.
     * Invokes the exact constructor-tail injection descriptor.
     *
     * @param VulkanGpuSurfaceMixin surfaceMixin Test-owned mixin instance
     * @param VulkanDevice surfaceDevice Mocked constructor device argument
     * @throws ReflectiveOperationException When the callback cannot be resolved or invoked
     */
    private static void invokeConstructorTail(
        VulkanGpuSurfaceMixin surfaceMixin,
        VulkanDevice surfaceDevice) throws ReflectiveOperationException {
        Method constructorTailMethod = VulkanGpuSurfaceMixin.class.getDeclaredMethod(
            "barrieww$probeBorrowedBootstrapHandles",
            VulkanDevice.class,
            long.class,
            CallbackInfo.class);
        constructorTailMethod.setAccessible(true);
        invoke(constructorTailMethod, surfaceMixin, surfaceDevice, 9L, null);
    }

    /**
     * @note ThreadSafety: Reflection invokes one test-confined mixin instance.
     * Invokes the exact configure injection descriptor.
     *
     * @param VulkanGpuSurfaceMixin surfaceMixin Test-owned mixin instance
     * @param GpuSurface.Configuration configuration Requested surface generation configuration
     * @param CallbackInfo callbackInformation Cancellable callback metadata
     * @throws ReflectiveOperationException When the callback cannot be resolved or invoked
     */
    private static void invokeConfigure(
        VulkanGpuSurfaceMixin surfaceMixin,
        GpuSurface.Configuration configuration,
        CallbackInfo callbackInformation) throws ReflectiveOperationException {
        Method configureMethod = VulkanGpuSurfaceMixin.class.getDeclaredMethod(
            "barrieww$configurePresentationTakeover",
            GpuSurface.Configuration.class,
            CallbackInfo.class);
        configureMethod.setAccessible(true);
        invoke(configureMethod, surfaceMixin, configuration, callbackInformation);
    }

    /**
     * @note ThreadSafety: Reflection invokes one test-confined mixin instance.
     * Invokes one no-argument backend callback injection.
     *
     * @param VulkanGpuSurfaceMixin surfaceMixin Test-owned mixin instance
     * @param String methodName Exact injected method name
     * @param CallbackInfo callbackInformation Callback metadata
     * @throws ReflectiveOperationException When the callback cannot be resolved or invoked
     */
    private static void invokeNoArgumentCallback(
        VulkanGpuSurfaceMixin surfaceMixin,
        String methodName,
        CallbackInfo callbackInformation) throws ReflectiveOperationException {
        Method callbackMethod = VulkanGpuSurfaceMixin.class.getDeclaredMethod(
            methodName,
            CallbackInfo.class);
        callbackMethod.setAccessible(true);
        invoke(callbackMethod, surfaceMixin, callbackInformation);
    }

    /**
     * @note ThreadSafety: Reflection mutates one test-owned instance.
     * Assigns one private shadow or unique field for callback testing.
     *
     * @param VulkanGpuSurfaceMixin surfaceMixin Test-owned mixin instance
     * @param String fieldName Exact field name
     * @param Object value Replacement field value
     * @throws ReflectiveOperationException When the field cannot be assigned
     */
    private static void setField(
        VulkanGpuSurfaceMixin surfaceMixin,
        String fieldName,
        Object value) throws ReflectiveOperationException {
        Field field = VulkanGpuSurfaceMixin.class.getDeclaredField(fieldName);
        field.setAccessible(true);
        field.set(surfaceMixin, value);
    }

    /**
     * @note ThreadSafety: Reflection reads one test-owned instance.
     * Reads one private shadow or unique field for callback assertions.
     *
     * @param VulkanGpuSurfaceMixin surfaceMixin Test-owned mixin instance
     * @param String fieldName Exact field name
     * @return Object Current field value
     * @throws ReflectiveOperationException When the field cannot be read
     */
    private static Object getField(
        VulkanGpuSurfaceMixin surfaceMixin,
        String fieldName) throws ReflectiveOperationException {
        Field field = VulkanGpuSurfaceMixin.class.getDeclaredField(fieldName);
        field.setAccessible(true);
        return field.get(surfaceMixin);
    }

    /**
     * @note ThreadSafety: Reflection invokes one test-owned instance serially.
     * Unwraps an injected callback invocation while preserving its original checked cause.
     *
     * @param Method method Accessible injected callback method
     * @param Object receiver Test-owned callback receiver
     * @param Object[] arguments Exact callback arguments
     * @throws ReflectiveOperationException When invocation fails
     */
    private static void invoke(Method method, Object receiver, Object... arguments)
        throws ReflectiveOperationException {
        try {
            method.invoke(receiver, arguments);
        } catch (InvocationTargetException invocationFailure) {
            Throwable cause = invocationFailure.getCause();
            if (cause instanceof ReflectiveOperationException reflectiveFailure) {
                throw reflectiveFailure;
            }
            throw invocationFailure;
        }
    }
}
