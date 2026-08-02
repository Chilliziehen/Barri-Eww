package barrieww.core.interoperability;

import static org.junit.jupiter.api.Assertions.assertDoesNotThrow;
import static org.junit.jupiter.api.Assertions.assertArrayEquals;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertSame;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.junit.jupiter.api.Assertions.assertThrows;

import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.Linker;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.SymbolLookup;
import java.lang.foreign.ValueLayout;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.reflect.Constructor;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import org.junit.jupiter.api.Test;

/**
 * @note ThreadSafety: JUnit creates a fresh instance per test method; no shared state.
 * Covers presentation runtime binding argument validation that needs no live library.
 */
class PresentationRuntimeBindingTests {

    @Test
    void hostImageSymbolNamesAndDescriptorsMatchVersionOneContract() {
        assertEquals("barriEwwCreateHostImagePresentationRuntimeVersion1",
                NativePresentationRuntime.s_createHostImageSymbolName);
        assertEquals("barriEwwDetachHostImagePresentationResourcesVersion1",
                NativePresentationRuntime.s_detachHostImageResourcesSymbolName);
        assertEquals("barriEwwSubmitAndPresentHostImageFrameVersion1",
                NativePresentationRuntime.s_submitAndPresentHostImageFrameSymbolName);
        assertEquals(FunctionDescriptor.of(ValueLayout.JAVA_INT, ValueLayout.ADDRESS,
                        ValueLayout.ADDRESS),
                NativePresentationRuntime.s_createHostImageDescriptor);
        assertEquals(FunctionDescriptor.of(ValueLayout.JAVA_INT, ValueLayout.JAVA_LONG,
                        ValueLayout.ADDRESS),
                NativePresentationRuntime.s_detachHostImageResourcesDescriptor);
        assertEquals(FunctionDescriptor.of(ValueLayout.JAVA_INT, ValueLayout.JAVA_LONG,
                        ValueLayout.ADDRESS),
                NativePresentationRuntime.s_submitAndPresentHostImageFrameDescriptor);
    }

    @Test
    void hostImageInitializationCreatesEveryDowncallWithoutCriticalOptions() {
        List<Linker.Option[]> ordinaryOptions = new ArrayList<>();
        DowncallHandleFactory ordinaryDowncallHandleFactory =
                (symbolAddress, functionDescriptor, linkerOptions) -> {
                    ordinaryOptions.add(linkerOptions);
                    return MethodHandles.empty(functionDescriptor.toMethodType());
                };
        List<FunctionDescriptor> capturedDescriptors = new ArrayList<>();
        List<Linker.Option[]> capturedOptions = new ArrayList<>();
        DowncallHandleFactory downcallHandleFactory =
                (symbolAddress, functionDescriptor, linkerOptions) -> {
                    capturedDescriptors.add(functionDescriptor);
                    capturedOptions.add(linkerOptions);
                    return MethodHandles.empty(functionDescriptor.toMethodType());
                };
        MemorySegment[] symbolAddresses = new MemorySegment[9];
        for (int symbolIndex = 0; symbolIndex < symbolAddresses.length; ++symbolIndex) {
            symbolAddresses[symbolIndex] = MemorySegment.NULL;
        }

        MethodHandle[] ordinaryDowncallHandles =
                NativePresentationRuntime.createOrdinaryDowncallHandles(
                        symbolAddresses, ordinaryDowncallHandleFactory);
        MethodHandle[] downcallHandles = NativePresentationRuntime.createHostImageDowncallHandles(
                symbolAddresses, downcallHandleFactory);

        assertEquals(6, ordinaryDowncallHandles.length);
        for (Linker.Option[] linkerOptions : ordinaryOptions) {
            assertArrayEquals(new Linker.Option[0], linkerOptions);
        }
        assertEquals(8, downcallHandles.length);
        assertEquals(NativePresentationRuntime.s_createHostImageDescriptor,
                capturedDescriptors.get(5));
        assertEquals(NativePresentationRuntime.s_detachHostImageResourcesDescriptor,
                capturedDescriptors.get(6));
        assertEquals(NativePresentationRuntime.s_submitAndPresentHostImageFrameDescriptor,
                capturedDescriptors.get(7));
        assertArrayEquals(new Linker.Option[0], capturedOptions.get(5));
        assertArrayEquals(new Linker.Option[0], capturedOptions.get(6));
        assertArrayEquals(new Linker.Option[0], capturedOptions.get(7));
    }

    @Test
    void ordinarySymbolResolutionDoesNotRequireAdditiveHostImageSymbols() throws Exception {
        SymbolLookup baseOnlyLookup = symbolName -> {
            if (symbolName.equals(NativePresentationRuntime.s_createHostImageSymbolName)
                    || symbolName.equals(
                    NativePresentationRuntime.s_detachHostImageResourcesSymbolName)
                    || symbolName.equals(
                    NativePresentationRuntime.s_submitAndPresentHostImageFrameSymbolName)) {
                return Optional.empty();
            }
            return Optional.of(MemorySegment.NULL);
        };

        MemorySegment[] symbols = NativePresentationRuntime.resolveSymbolAddresses(
                baseOnlyLookup, Path.of("base-only").toAbsolutePath(), false);

        assertEquals(6, symbols.length);
    }

    @Test
    void hostImageSymbolResolutionRequiresEveryAdditiveSymbol() {
        String[] hostSymbolNames = {
                NativePresentationRuntime.s_createHostImageSymbolName,
                NativePresentationRuntime.s_detachHostImageResourcesSymbolName,
                NativePresentationRuntime.s_submitAndPresentHostImageFrameSymbolName
        };
        for (String missingSymbolName : hostSymbolNames) {
            SymbolLookup lookup = symbolName -> symbolName.equals(missingSymbolName)
                    ? Optional.empty() : Optional.of(MemorySegment.NULL);
            NativeLibraryLoadingException loadingException = assertThrows(
                    NativeLibraryLoadingException.class,
                    () -> NativePresentationRuntime.resolveSymbolAddresses(lookup,
                            Path.of("host-symbol-test").toAbsolutePath(), true));
            assertEquals(missingSymbolName, loadingException.nativeSymbolName());
        }
    }

    @Test
    void hostImageCreateInfoWritesEveryFieldAndLeavesReservedZero() {
        PresentationBootstrapHandles bootstrapHandles = new PresentationBootstrapHandles(
                11L, 12L, 13L, 14L, 15L, 16L, 17, 18);
        HostImagePresentationBinding binding = HostImagePresentationBinding.create(
                19L, PresentationImageFormat.R8G8B8A8_UNORM,
                PresentationImageFormat.B8G8R8A8_UNORM, 1280, 720, 1280, 720);
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment createInfo = arena.allocate(
                    NativePresentationRuntime.s_hostImageCreateInfoLayout);
            createInfo.fill((byte) -1);

            NativePresentationRuntime.writeHostImageCreateInfo(createInfo, bootstrapHandles,
                    1280, 720, 2, binding);

            assertEquals(11L, createInfo.get(ValueLayout.JAVA_LONG, 0));
            assertEquals(12L, createInfo.get(ValueLayout.JAVA_LONG, 8));
            assertEquals(13L, createInfo.get(ValueLayout.JAVA_LONG, 16));
            assertEquals(14L, createInfo.get(ValueLayout.JAVA_LONG, 24));
            assertEquals(15L, createInfo.get(ValueLayout.JAVA_LONG, 32));
            assertEquals(16L, createInfo.get(ValueLayout.JAVA_LONG, 40));
            assertEquals(17, createInfo.get(ValueLayout.JAVA_INT, 48));
            assertEquals(18, createInfo.get(ValueLayout.JAVA_INT, 52));
            assertEquals(1280, createInfo.get(ValueLayout.JAVA_INT, 56));
            assertEquals(720, createInfo.get(ValueLayout.JAVA_INT, 60));
            assertEquals(2, createInfo.get(ValueLayout.JAVA_INT, 64));
            assertEquals(0, createInfo.get(ValueLayout.JAVA_INT, 68));
            assertEquals(19L, createInfo.get(ValueLayout.JAVA_LONG, 72));
            assertEquals(1, createInfo.get(ValueLayout.JAVA_INT, 80));
            assertEquals(2, createInfo.get(ValueLayout.JAVA_INT, 84));
            assertEquals(1280, createInfo.get(ValueLayout.JAVA_INT, 88));
            assertEquals(720, createInfo.get(ValueLayout.JAVA_INT, 92));
        }
    }

    @Test
    void ordinaryRuntimeRejectsHostImageMethodsWithoutNullPointerFailure() throws Exception {
        Arena libraryArena = Arena.ofConfined();
        NativePresentationRuntime runtime = createRuntimeForCloseTest(libraryArena,
                MethodHandles.lookup().findStatic(PresentationRuntimeBindingTests.class,
                        "destroySuccessfully", MethodType.methodType(int.class, long.class)));

        assertThrows(IllegalStateException.class,
                runtime::detachHostImagePresentationResources);
        assertThrows(IllegalStateException.class,
                runtime::submitAndPresentHostImageFrame);
        runtime.close();
    }

    @Test
    void hostImageMethodsReuseSegmentsAndMapOperationResults() throws Exception {
        Arena libraryArena = Arena.ofConfined();
        HostImageInvocationObserver observer = new HostImageInvocationObserver();
        NativePresentationRuntime runtime = createHostRuntimeForInvocationTest(
                libraryArena, observer);

        runtime.detachHostImagePresentationResources();
        runtime.detachHostImagePresentationResources();
        assertEquals(PresentationFrameStatus.SUBOPTIMAL,
                runtime.submitAndPresentHostImageFrame());
        assertEquals(PresentationFrameStatus.SUBOPTIMAL,
                runtime.submitAndPresentHostImageFrame());
        assertSame(observer.m_firstDetachResult, observer.m_secondDetachResult);
        assertSame(observer.m_firstSubmitResult, observer.m_secondSubmitResult);

        observer.m_detachOperationResult = 3;
        NativePresentationRuntimeException detachFailure = assertThrows(
                NativePresentationRuntimeException.class,
                runtime::detachHostImagePresentationResources);
        assertEquals(3, detachFailure.operationResultCode());
        assertEquals(-77, detachFailure.vulkanResult());
        assertEquals(NativePresentationRuntime.s_detachHostImageResourcesSymbolName,
                detachFailure.nativeSymbolName());

        observer.m_detachOperationResult = 0;
        observer.m_submitOperationResult = 3;
        NativePresentationRuntimeException submitFailure = assertThrows(
                NativePresentationRuntimeException.class,
                runtime::submitAndPresentHostImageFrame);
        assertEquals(3, submitFailure.operationResultCode());
        assertEquals(-88, submitFailure.vulkanResult());
        runtime.close();
        assertThrows(IllegalStateException.class,
                runtime::detachHostImagePresentationResources);
        assertThrows(IllegalStateException.class,
                runtime::submitAndPresentHostImageFrame);
    }

    @Test
    void submitAndPresentClearFrameSymbolNameMatchesVersionOneContract() {
        assertEquals("barriEwwSubmitAndPresentClearFrameVersion1",
                NativePresentationRuntime.s_submitAndPresentClearFrameSymbolName);
    }

    @Test
    void createRejectsARelativeLibraryPath() {
        PresentationBootstrapHandles handles = new PresentationBootstrapHandles(
                1L, 2L, 3L, 4L, 5L, 5L, 0, 0);
        NativeLibraryLoadingException loadingException = assertThrows(
                NativeLibraryLoadingException.class,
                () -> NativePresentationRuntime.create(Path.of("BarriEwwNativeFfm"), handles,
                        1280, 720, 2));
        assertEquals(NativePresentationRuntime.s_createSymbolName,
                loadingException.nativeSymbolName());
    }

    @Test
    void createFactoriesRejectInvalidDimensionsAndFrameCountsBeforeLibraryLookup() {
        Path missingAbsoluteLibraryPath = Path.of("missing-validation-library").toAbsolutePath();
        PresentationBootstrapHandles bootstrapHandles = new PresentationBootstrapHandles(
                1L, 2L, 3L, 4L, 5L, 5L, 0, 0);
        HostImagePresentationBinding hostImageBinding = HostImagePresentationBinding.create(
                6L, PresentationImageFormat.R8G8B8A8_UNORM,
                PresentationImageFormat.B8G8R8A8_UNORM, 1, 1, 1, 1);
        int[][] invalidCreateParameters = {
                {0, 1, 1}, {-1, 1, 1}, {1, 0, 1}, {1, -1, 1}, {1, 1, 0}, {1, 1, -1}
        };

        for (int[] createParameters : invalidCreateParameters) {
            assertThrows(IllegalArgumentException.class,
                    () -> NativePresentationRuntime.create(missingAbsoluteLibraryPath,
                            bootstrapHandles, createParameters[0], createParameters[1],
                            createParameters[2]));
            assertThrows(IllegalArgumentException.class,
                    () -> NativePresentationRuntime.createHostImagePresentation(
                            missingAbsoluteLibraryPath, bootstrapHandles, createParameters[0],
                            createParameters[1], createParameters[2], hostImageBinding));
        }
    }

    @Test
    void failedOwnerHandoffDestroysOnceClosesArenaAndPreservesFailures() throws Exception {
        Arena libraryArena = Arena.ofConfined();
        Arena creationArena = Arena.ofConfined();
        RuntimeException initializationFailure = new RuntimeException("owner initialization");
        RuntimeException destroyFailure = new RuntimeException("handoff destroy");
        OwnershipHandoffObserver observer = new OwnershipHandoffObserver(destroyFailure);
        MethodHandle destroyHandle = MethodHandles.lookup().findVirtual(
                OwnershipHandoffObserver.class, "destroy",
                MethodType.methodType(int.class, long.class)).bindTo(observer);

        NativePresentationRuntimeException runtimeException = assertThrows(
                NativePresentationRuntimeException.class,
                () -> NativePresentationRuntime.completeRuntimeOwnershipTransfer(
                        libraryArena, creationArena, destroyHandle, 91L,
                        () -> {
                            assertTrue(creationArena.scope().isAlive());
                            throw initializationFailure;
                        }, NativePresentationRuntime.s_createHostImageSymbolName));

        assertSame(initializationFailure, runtimeException.getCause());
        assertEquals(1, initializationFailure.getSuppressed().length);
        assertSame(destroyFailure, initializationFailure.getSuppressed()[0]);
        assertEquals(1, observer.m_invocationCount);
        assertEquals(91L, observer.m_runtimeAddress);
        assertFalse(libraryArena.scope().isAlive());
        assertFalse(creationArena.scope().isAlive());
    }

    @Test
    void ownerFactoryIsPreparedBeforeNativeCreateAndSuccessImmediatelyEntersGuard()
            throws Exception {
        Arena libraryArena = Arena.ofConfined();
        Arena creationArena = Arena.ofConfined();
        InitializationOrderObserver observer = new InitializationOrderObserver();
        MethodHandles.Lookup methodLookup = MethodHandles.lookup();
        MethodHandle createHandle = methodLookup.findVirtual(
                InitializationOrderObserver.class, "createNativeRuntime",
                MethodType.methodType(int.class, MemorySegment.class, MemorySegment.class))
                .bindTo(observer);
        MethodHandle destroyHandle = methodLookup.findVirtual(
                InitializationOrderObserver.class, "destroyNativeRuntime",
                MethodType.methodType(int.class, long.class)).bindTo(observer);
        MemorySegment createInfo = creationArena.allocate(96, 8);
        MemorySegment createResult = creationArena.allocate(32, 8);

        NativePresentationRuntimeException runtimeException = assertThrows(
                NativePresentationRuntimeException.class,
                () -> NativePresentationRuntime.invokeCreateAndCompleteRuntimeOwnershipTransfer(
                        createHandle, createInfo, createResult, libraryArena, creationArena,
                        destroyHandle, observer,
                        NativePresentationRuntime.s_createHostImageSymbolName));

        assertSame(observer.m_initializationFailure, runtimeException.getCause());
        assertEquals(List.of("factoryPrepared", "nativeCreate", "guardedOwnerCreation", "destroy"),
                observer.m_events);
        assertEquals(1, observer.m_destroyInvocationCount);
        assertFalse(libraryArena.scope().isAlive());
        assertFalse(creationArena.scope().isAlive());
    }

    @Test
    void hostInvocationFailuresPreserveExactCausesAndStacks() throws Exception {
        RuntimeException createFailure = new RuntimeException("host create invocation");
        MethodHandle createHandle = MethodHandles.lookup().findStatic(
                PresentationRuntimeBindingTests.class, "failHostImageCreate",
                MethodType.methodType(int.class, RuntimeException.class, MemorySegment.class,
                        MemorySegment.class)).bindTo(createFailure);
        try (Arena invocationArena = Arena.ofConfined()) {
            NativePresentationRuntimeException createException = assertThrows(
                    NativePresentationRuntimeException.class,
                    () -> NativePresentationRuntime.invokePresentationCreate(
                            createHandle, invocationArena.allocate(96, 8),
                            invocationArena.allocate(32, 8),
                            NativePresentationRuntime.s_createHostImageSymbolName));
            assertSame(createFailure, createException.getCause());
            assertTrue(createException.getCause().getStackTrace().length > 0);
        }

        Arena libraryArena = Arena.ofConfined();
        HostImageInvocationObserver observer = new HostImageInvocationObserver();
        NativePresentationRuntime runtime = createHostRuntimeForInvocationTest(
                libraryArena, observer);
        RuntimeException detachFailure = new RuntimeException("host detach invocation");
        observer.m_detachInvocationFailure = detachFailure;
        NativePresentationRuntimeException detachException = assertThrows(
                NativePresentationRuntimeException.class,
                runtime::detachHostImagePresentationResources);
        assertSame(detachFailure, detachException.getCause());
        assertTrue(detachException.getCause().getStackTrace().length > 0);

        observer.m_detachInvocationFailure = null;
        RuntimeException submitFailure = new RuntimeException("host submit invocation");
        observer.m_submitInvocationFailure = submitFailure;
        NativePresentationRuntimeException submitException = assertThrows(
                NativePresentationRuntimeException.class,
                runtime::submitAndPresentHostImageFrame);
        assertSame(submitFailure, submitException.getCause());
        assertTrue(submitException.getCause().getStackTrace().length > 0);
        runtime.close();
    }

    @Test
    void closeFinalizesOwnershipAfterOperationFailureWithoutRetry() throws Exception {
        Arena libraryArena = Arena.ofConfined();
        DestroyAttemptObserver destroyAttemptObserver = new DestroyAttemptObserver(false, 4);
        MethodHandle destroyHandle = MethodHandles.lookup().findVirtual(
                DestroyAttemptObserver.class, "destroy",
                MethodType.methodType(int.class, long.class)).bindTo(destroyAttemptObserver);
        NativePresentationRuntime runtime = createRuntimeForCloseTest(
                libraryArena, destroyHandle);

        NativePresentationRuntimeException runtimeException = assertThrows(
                NativePresentationRuntimeException.class, runtime::close);

        assertEquals(NativePresentationRuntime.s_destroySymbolName,
                runtimeException.nativeSymbolName());
        assertEquals(4, runtimeException.operationResultCode());
        assertEquals(0, runtimeException.vulkanResult());
        assertFalse(libraryArena.scope().isAlive());
        assertEquals(1, destroyAttemptObserver.m_invocationCount);
        assertEquals(0L, destroyAttemptObserver.m_runtimeAddress);

        assertDoesNotThrow(runtime::close);
        assertEquals(1, destroyAttemptObserver.m_invocationCount);
    }

    @Test
    void closeFinalizesOwnershipAfterInvocationFailureWithoutRetry() throws Exception {
        Arena libraryArena = Arena.ofConfined();
        DestroyAttemptObserver destroyAttemptObserver = new DestroyAttemptObserver(true, 0);
        MethodHandle destroyHandle = MethodHandles.lookup().findVirtual(
                DestroyAttemptObserver.class, "destroy",
                MethodType.methodType(int.class, long.class)).bindTo(destroyAttemptObserver);
        NativePresentationRuntime runtime = createRuntimeForCloseTest(
                libraryArena, destroyHandle);

        NativePresentationRuntimeException runtimeException = assertThrows(
                NativePresentationRuntimeException.class, runtime::close);

        assertEquals(NativePresentationRuntime.s_destroySymbolName,
                runtimeException.nativeSymbolName());
        assertEquals(-1, runtimeException.operationResultCode());
        assertEquals(0, runtimeException.vulkanResult());
        assertTrue(runtimeException.getMessage().contains("Injected destroy failure"));
        assertFalse(libraryArena.scope().isAlive());
        assertEquals(1, destroyAttemptObserver.m_invocationCount);
        assertEquals(0L, destroyAttemptObserver.m_runtimeAddress);

        assertDoesNotThrow(runtime::close);
        assertEquals(1, destroyAttemptObserver.m_invocationCount);
    }

    @Test
    void closeFinalizesOwnershipAfterSuccessWithoutRetry() throws Exception {
        Arena libraryArena = Arena.ofConfined();
        DestroyAttemptObserver destroyAttemptObserver = new DestroyAttemptObserver(false, 0);
        MethodHandle destroyHandle = MethodHandles.lookup().findVirtual(
                DestroyAttemptObserver.class, "destroy",
                MethodType.methodType(int.class, long.class)).bindTo(destroyAttemptObserver);
        NativePresentationRuntime runtime = createRuntimeForCloseTest(
                libraryArena, destroyHandle);

        assertDoesNotThrow(runtime::close);

        assertFalse(libraryArena.scope().isAlive());
        assertEquals(1, destroyAttemptObserver.m_invocationCount);
        assertEquals(0L, destroyAttemptObserver.m_runtimeAddress);
        assertDoesNotThrow(runtime::close);
        assertEquals(1, destroyAttemptObserver.m_invocationCount);
    }

    @Test
    void publicFramePathsDecodeJavaOnlyBoundaryResults() throws Exception {
        Arena libraryArena = Arena.ofConfined();
        MethodHandles.Lookup methodLookup = MethodHandles.lookup();
        MethodHandle destroyHandle = methodLookup.findStatic(
                PresentationRuntimeBindingTests.class, "destroySuccessfully",
                MethodType.methodType(int.class, long.class));
        MethodHandle beginHandle = methodLookup.findStatic(
                PresentationRuntimeBindingTests.class, "beginFrameSuccessfully",
                MethodType.methodType(int.class, long.class, int.class, int.class,
                        MemorySegment.class, MemorySegment.class));
        MethodHandle submitHandle = methodLookup.findStatic(
                PresentationRuntimeBindingTests.class, "submitFrameSuccessfully",
                MethodType.methodType(int.class, long.class, long.class, MemorySegment.class));
        MethodHandle presentClearHandle = methodLookup.findStatic(
                PresentationRuntimeBindingTests.class, "presentClearFrameSuccessfully",
                MethodType.methodType(int.class, long.class, int.class, int.class, float.class,
                        float.class, float.class, MemorySegment.class, MemorySegment.class,
                        MemorySegment.class));
        MethodHandle submitAndPresentClearFrameHandle = methodLookup.findStatic(
                PresentationRuntimeBindingTests.class, "submitAndPresentClearFrameSuccessfully",
                MethodType.methodType(int.class, long.class, float.class, float.class,
                        float.class, MemorySegment.class));
        NativePresentationRuntime runtime = createRuntimeForInvocationTest(libraryArena,
                destroyHandle, beginHandle, submitHandle, presentClearHandle,
                submitAndPresentClearFrameHandle);

        assertEquals(PresentationFrameStatus.SUBOPTIMAL, runtime.beginFrameStatus(1280, 720));
        PresentationBeginFrame beginFrame = runtime.beginFrame(1280, 720);
        assertEquals(PresentationFrameStatus.SUBOPTIMAL, beginFrame.status());
        assertEquals(1, beginFrame.frameSlotIndex());
        assertEquals(2, beginFrame.imageIndex());
        assertEquals(3L, beginFrame.frameSequence());
        assertEquals(4L, beginFrame.swapchainGeneration());
        assertTrue(beginFrame.priorMetrics().isEmpty());
        assertEquals(PresentationFrameStatus.RECREATE_REQUIRED,
                runtime.submitAndPresentFrame(0L));
        assertEquals(PresentationFrameStatus.SUCCESS,
                runtime.submitAndPresentClearFrame(0.1f, 0.2f, 0.3f));
        PresentationClearFrame clearFrame = runtime.presentClearFrame(
                1280, 720, 0.1f, 0.2f, 0.3f);
        assertEquals(PresentationFrameStatus.SURFACE_UNAVAILABLE, clearFrame.beginStatus());
        assertEquals(PresentationFrameStatus.SUBOPTIMAL, clearFrame.submitStatus());
        assertEquals(37, runtime.selectedFormatValue());
        assertEquals(1, runtime.selectedPresentModeValue());
        assertEquals(0, runtime.selectedSharingModeValue());
        assertEquals(3, runtime.swapchainImageCount());
        runtime.close();
        assertFalse(libraryArena.scope().isAlive());
    }

    @Test
    void repeatedStatusCallsReuseIdenticalOutputSegments() throws Exception {
        Arena libraryArena = Arena.ofConfined();
        FrameInvocationObserver invocationObserver = new FrameInvocationObserver();
        NativePresentationRuntime runtime = createRuntimeForFrameObservationTest(
                libraryArena, invocationObserver);

        runtime.beginFrameStatus(1280, 720);
        runtime.beginFrameStatus(1280, 720);
        runtime.submitAndPresentClearFrame(0.1f, 0.2f, 0.3f);
        runtime.submitAndPresentClearFrame(0.1f, 0.2f, 0.3f);

        assertTrue(invocationObserver.m_firstBeginResultAddress != 0L);
        assertTrue(invocationObserver.m_firstPriorMetricsAddress != 0L);
        assertTrue(invocationObserver.m_firstSubmitResultAddress != 0L);
        assertEquals(invocationObserver.m_firstBeginResultAddress,
                invocationObserver.m_secondBeginResultAddress);
        assertEquals(invocationObserver.m_firstPriorMetricsAddress,
                invocationObserver.m_secondPriorMetricsAddress);
        assertEquals(invocationObserver.m_firstSubmitResultAddress,
                invocationObserver.m_secondSubmitResultAddress);
        runtime.close();
    }

    @Test
    void nativeClearedInvalidMetricsDoNotLeakPriorOptionalValue() throws Exception {
        Arena libraryArena = Arena.ofConfined();
        FrameInvocationObserver invocationObserver = new FrameInvocationObserver();
        invocationObserver.m_shouldAlternateMetricsValidity = true;
        NativePresentationRuntime runtime = createRuntimeForFrameObservationTest(
                libraryArena, invocationObserver);

        PresentationBeginFrame validMetricsFrame = runtime.beginFrame(1280, 720);
        PresentationBeginFrame invalidMetricsFrame = runtime.beginFrame(1280, 720);

        assertTrue(validMetricsFrame.priorMetrics().isPresent());
        assertEquals(91L, validMetricsFrame.priorMetrics().orElseThrow().frameSequence());
        assertTrue(invalidMetricsFrame.priorMetrics().isEmpty());
        runtime.close();
    }

    @Test
    void operationFailuresReadRawVulkanResultsFromExactOffsets() throws Exception {
        Arena libraryArena = Arena.ofConfined();
        FrameInvocationObserver invocationObserver = new FrameInvocationObserver();
        invocationObserver.m_beginOperationResult = 3;
        NativePresentationRuntime runtime = createRuntimeForFrameObservationTest(
                libraryArena, invocationObserver);

        NativePresentationRuntimeException beginFailure = assertThrows(
                NativePresentationRuntimeException.class,
                () -> runtime.beginFrameStatus(1280, 720));
        assertEquals(3, beginFailure.operationResultCode());
        assertEquals(-1001, beginFailure.vulkanResult());
        assertEquals(NativePresentationRuntime.s_beginFrameSymbolName,
                beginFailure.nativeSymbolName());

        invocationObserver.m_beginOperationResult = 0;
        invocationObserver.m_submitAndPresentClearFrameOperationResult = 3;
        NativePresentationRuntimeException submitFailure = assertThrows(
                NativePresentationRuntimeException.class,
                () -> runtime.submitAndPresentClearFrame(0.1f, 0.2f, 0.3f));
        assertEquals(3, submitFailure.operationResultCode());
        assertEquals(-1002, submitFailure.vulkanResult());
        assertEquals(NativePresentationRuntime.s_submitAndPresentClearFrameSymbolName,
                submitFailure.nativeSymbolName());
        runtime.close();
    }

    @Test
    void missingSymbolFailureNamesExactRequestedSymbol() {
        Path nativeLibraryPath = Path.of("missing-presentation-library").toAbsolutePath();
        SymbolLookup missingSymbolLookup = symbolName -> Optional.empty();

        NativeLibraryLoadingException loadingException = assertThrows(
                NativeLibraryLoadingException.class,
                () -> NativePresentationRuntime.findSymbol(missingSymbolLookup,
                        nativeLibraryPath,
                        NativePresentationRuntime.s_submitAndPresentClearFrameSymbolName));

        assertEquals(nativeLibraryPath, loadingException.nativeLibraryPath());
        assertEquals(NativePresentationRuntime.s_submitAndPresentClearFrameSymbolName,
                loadingException.nativeSymbolName());
    }

    /**
     * Creates a runtime whose destroy handle executes Java-only test behavior.
     *
     * @param Arena libraryArena Confined Arena owned by the returned test runtime
     * @param MethodHandle destroyHandle Java-only destroy behavior
     * @return NativePresentationRuntime Runtime configured for close testing
     * @throws Exception When reflective constructor access fails
     * @warning MemoryOwnership: The returned runtime owns libraryArena and all allocated test
     *          segments; close releases them. The Java-only MethodHandle owns no native memory.
     */
    private static NativePresentationRuntime createRuntimeForCloseTest(
            Arena libraryArena, MethodHandle destroyHandle) throws Exception {
        return createRuntimeForInvocationTest(libraryArena, destroyHandle, destroyHandle,
                destroyHandle, destroyHandle, destroyHandle);
    }

    /**
     * Creates a runtime backed entirely by Java-only invocation handles.
     *
     * @param Arena libraryArena Confined Arena owned by the returned test runtime
     * @param MethodHandle destroyHandle Java-only destroy behavior
     * @param MethodHandle beginHandle Java-only begin-frame behavior
     * @param MethodHandle submitHandle Java-only submit-frame behavior
     * @param MethodHandle presentClearHandle Java-only combined clear-frame behavior
     * @param MethodHandle submitAndPresentClearFrameHandle Java-only clear-submit-and-present
     *        behavior
     * @return NativePresentationRuntime Runtime configured for boundary mapping tests
     * @throws Exception When reflective constructor access fails
     * @warning MemoryOwnership: The returned runtime owns libraryArena and all allocated test
     *          segments. Every MethodHandle invokes Java only and owns no native memory.
     */
    private static NativePresentationRuntime createRuntimeForInvocationTest(
            Arena libraryArena, MethodHandle destroyHandle, MethodHandle beginHandle,
            MethodHandle submitHandle, MethodHandle presentClearHandle,
            MethodHandle submitAndPresentClearFrameHandle) throws Exception {
        Constructor<NativePresentationRuntime> constructor =
                NativePresentationRuntime.class.getDeclaredConstructor(
                        Arena.class, MethodHandle.class, MethodHandle.class, MethodHandle.class,
                        MethodHandle.class, MethodHandle.class, MethodHandle.class,
                        MethodHandle.class, MemorySegment.class, MemorySegment.class,
                        MemorySegment.class, MemorySegment.class, long.class, int.class,
                        int.class, int.class, int.class);
        constructor.setAccessible(true);
        MemorySegment beginResult = libraryArena.allocate(40, 8);
        MemorySegment priorMetrics = libraryArena.allocate(112, 8);
        MemorySegment submitResult = libraryArena.allocate(8, 4);
        return constructor.newInstance(libraryArena, destroyHandle, beginHandle, submitHandle,
                presentClearHandle, submitAndPresentClearFrameHandle, null, null, beginResult,
                priorMetrics, submitResult, null, 0L, 37, 1, 0, 3);
    }

    /**
     * @note ThreadSafety: Test-confined; the caller owns the supplied confined Arena.
     * Creates a host runtime backed by Java-only detach and submit handles.
     *
     * @param Arena libraryArena Confined Arena owned by the returned runtime
     * @param HostImageInvocationObserver observer Java-only host invocation observer
     * @return NativePresentationRuntime Host runtime configured for boundary mapping tests
     * @throws Exception When MethodHandle or reflective constructor access fails
     * @warning MemoryOwnership: The returned runtime owns libraryArena and every allocated
     *          segment. The observer retains segment references only until this test closes it.
     */
    private static NativePresentationRuntime createHostRuntimeForInvocationTest(
            Arena libraryArena, HostImageInvocationObserver observer) throws Exception {
        MethodHandles.Lookup methodLookup = MethodHandles.lookup();
        MethodHandle destroyHandle = methodLookup.findStatic(
                PresentationRuntimeBindingTests.class, "destroySuccessfully",
                MethodType.methodType(int.class, long.class));
        MethodHandle detachHandle = methodLookup.findVirtual(
                HostImageInvocationObserver.class, "detach",
                MethodType.methodType(int.class, long.class, MemorySegment.class))
                .bindTo(observer);
        MethodHandle hostSubmitHandle = methodLookup.findVirtual(
                HostImageInvocationObserver.class, "submit",
                MethodType.methodType(int.class, long.class, MemorySegment.class))
                .bindTo(observer);
        Constructor<NativePresentationRuntime> constructor =
                NativePresentationRuntime.class.getDeclaredConstructor(
                        Arena.class, MethodHandle.class, MethodHandle.class, MethodHandle.class,
                        MethodHandle.class, MethodHandle.class, MethodHandle.class,
                        MethodHandle.class, MemorySegment.class, MemorySegment.class,
                        MemorySegment.class, MemorySegment.class, long.class, int.class,
                        int.class, int.class, int.class);
        constructor.setAccessible(true);
        return constructor.newInstance(libraryArena, destroyHandle, destroyHandle, destroyHandle,
                destroyHandle, destroyHandle, detachHandle, hostSubmitHandle,
                libraryArena.allocate(40, 8), libraryArena.allocate(112, 8),
                libraryArena.allocate(8, 4), libraryArena.allocate(8, 4), 0L, 37, 1, 0, 3);
    }

    /**
     * Creates a runtime whose begin and clear-submit-and-present handles observe reusable output
     * storage.
     *
     * @param Arena libraryArena Confined Arena owned by the returned test runtime
     * @param FrameInvocationObserver invocationObserver Java-only frame invocation observer
     * @return NativePresentationRuntime Runtime configured for output-storage tests
     * @throws Exception When MethodHandle or reflective constructor access fails
     * @warning MemoryOwnership: The returned runtime owns libraryArena and its test segments;
     *          invocationObserver only records scalar addresses and retains no MemorySegment.
     */
    private static NativePresentationRuntime createRuntimeForFrameObservationTest(
            Arena libraryArena, FrameInvocationObserver invocationObserver) throws Exception {
        MethodHandles.Lookup methodLookup = MethodHandles.lookup();
        MethodHandle destroyHandle = methodLookup.findStatic(
                PresentationRuntimeBindingTests.class, "destroySuccessfully",
                MethodType.methodType(int.class, long.class));
        MethodHandle beginHandle = methodLookup.findVirtual(
                FrameInvocationObserver.class, "beginFrame",
                MethodType.methodType(int.class, long.class, int.class, int.class,
                        MemorySegment.class, MemorySegment.class)).bindTo(invocationObserver);
        MethodHandle submitHandle = methodLookup.findStatic(
                PresentationRuntimeBindingTests.class, "submitFrameSuccessfully",
                MethodType.methodType(int.class, long.class, long.class, MemorySegment.class));
        MethodHandle presentClearHandle = methodLookup.findStatic(
                PresentationRuntimeBindingTests.class, "presentClearFrameSuccessfully",
                MethodType.methodType(int.class, long.class, int.class, int.class, float.class,
                        float.class, float.class, MemorySegment.class, MemorySegment.class,
                        MemorySegment.class));
        MethodHandle submitAndPresentClearFrameHandle = methodLookup.findVirtual(
                FrameInvocationObserver.class, "submitAndPresentClearFrame",
                MethodType.methodType(int.class, long.class, float.class, float.class,
                        float.class, MemorySegment.class)).bindTo(invocationObserver);
        return createRuntimeForInvocationTest(libraryArena, destroyHandle, beginHandle,
                submitHandle, presentClearHandle, submitAndPresentClearFrameHandle);
    }

    /**
     * Returns a successful Java-only destroy result.
     *
     * @param long runtimeAddress Synthetic runtime value that is never dereferenced
     * @return int Success operation result
     */
    private static int destroySuccessfully(long runtimeAddress) {
        return runtimeAddress == 0L ? 0 : 1;
    }

    /** Records one deterministic destroy attempt for lifetime-finalization tests. */
    private static final class DestroyAttemptObserver {
        private final boolean m_throwsOnInvocation;
        private final int m_operationResult;
        private int m_invocationCount;
        private long m_runtimeAddress;

        private DestroyAttemptObserver(boolean throwsOnInvocation, int operationResult) {
            m_throwsOnInvocation = throwsOnInvocation;
            m_operationResult = operationResult;
        }

        /**
         * Records the runtime identity and returns or throws the configured destroy outcome.
         *
         * @param long runtimeAddress Synthetic runtime value that is never dereferenced
         * @return int Configured operation result when invocation does not throw
         */
        private int destroy(long runtimeAddress) {
            m_invocationCount++;
            m_runtimeAddress = runtimeAddress;
            if (m_throwsOnInvocation) {
                throw new IllegalStateException(
                        "Injected destroy failure at " + runtimeAddress);
            }
            return m_operationResult;
        }
    }

    /**
     * Writes a deterministic begin-frame result through Java-only segments.
     *
     * @param long runtimeAddress Synthetic runtime value that is never dereferenced
     * @param int framebufferWidth Test framebuffer width
     * @param int framebufferHeight Test framebuffer height
     * @param MemorySegment beginResult Writable begin-frame output
     * @param MemorySegment priorMetrics Writable prior-metrics output
     * @return int Success operation result
     * @warning MemoryOwnership: The test runtime owns both segments; this method writes them
     *          synchronously and retains nothing.
     */
    private static int beginFrameSuccessfully(long runtimeAddress, int framebufferWidth,
                                              int framebufferHeight, MemorySegment beginResult,
                                              MemorySegment priorMetrics) {
        beginResult.fill((byte) 0);
        priorMetrics.fill((byte) 0);
        beginResult.set(ValueLayout.JAVA_INT, 0, 3);
        beginResult.set(ValueLayout.JAVA_INT, 4, 1);
        beginResult.set(ValueLayout.JAVA_INT, 8, 2);
        beginResult.set(ValueLayout.JAVA_LONG, 16, 3L);
        beginResult.set(ValueLayout.JAVA_LONG, 24, 4L);
        return runtimeAddress == 0L && framebufferWidth == 1280 && framebufferHeight == 720
                ? 0 : 1;
    }

    /**
     * Writes a deterministic submit-frame result through a Java-only segment.
     *
     * @param long runtimeAddress Synthetic runtime value that is never dereferenced
     * @param long commandBufferHandle Synthetic command buffer value that is never dereferenced
     * @param MemorySegment submitResult Writable submit-frame output
     * @return int Success operation result
     * @warning MemoryOwnership: The test runtime owns submitResult; this method writes it
     *          synchronously and retains nothing.
     */
    private static int submitFrameSuccessfully(long runtimeAddress, long commandBufferHandle,
                                               MemorySegment submitResult) {
        submitResult.fill((byte) 0);
        submitResult.set(ValueLayout.JAVA_INT, 0, 2);
        return runtimeAddress == 0L && commandBufferHandle == 0L ? 0 : 1;
    }

    /**
     * Writes a deterministic clear-submit result through a Java-only segment.
     *
     * @param long runtimeAddress Synthetic runtime value that is never dereferenced
     * @param float clearRed Test red clear channel
     * @param float clearGreen Test green clear channel
     * @param float clearBlue Test blue clear channel
     * @param MemorySegment submitResult Writable submit-frame output
     * @return int Success operation result
     * @warning MemoryOwnership: The test runtime owns submitResult; this method writes it
     *          synchronously and retains nothing.
     */
    private static int submitAndPresentClearFrameSuccessfully(
            long runtimeAddress, float clearRed, float clearGreen, float clearBlue,
            MemorySegment submitResult) {
        submitResult.fill((byte) 0);
        return runtimeAddress == 0L && clearRed == 0.1f && clearGreen == 0.2f
                && clearBlue == 0.3f ? 0 : 1;
    }

    /**
     * Writes deterministic combined clear-frame outputs through Java-only segments.
     *
     * @param long runtimeAddress Synthetic runtime value that is never dereferenced
     * @param int framebufferWidth Test framebuffer width
     * @param int framebufferHeight Test framebuffer height
     * @param float clearRed Test red clear channel
     * @param float clearGreen Test green clear channel
     * @param float clearBlue Test blue clear channel
     * @param MemorySegment beginResult Writable begin-frame output
     * @param MemorySegment priorMetrics Writable prior-metrics output
     * @param MemorySegment submitResult Writable submit-frame output
     * @return int Success operation result
     * @warning MemoryOwnership: The test runtime owns all segments; this method writes them
     *          synchronously and retains nothing.
     */
    private static int presentClearFrameSuccessfully(
            long runtimeAddress, int framebufferWidth, int framebufferHeight, float clearRed,
            float clearGreen, float clearBlue, MemorySegment beginResult,
            MemorySegment priorMetrics, MemorySegment submitResult) {
        beginResult.fill((byte) 0);
        priorMetrics.fill((byte) 0);
        submitResult.fill((byte) 0);
        beginResult.set(ValueLayout.JAVA_INT, 0, 1);
        submitResult.set(ValueLayout.JAVA_INT, 0, 3);
        return runtimeAddress == 0L && framebufferWidth == 1280 && framebufferHeight == 720
                && clearRed == 0.1f && clearGreen == 0.2f && clearBlue == 0.3f ? 0 : 1;
    }

    /**
     * @note ThreadSafety: Test-confined; one test thread owns each instance.
     * Observes Java-only frame invocations without retaining MemorySegments.
     * @warning MemoryOwnership: Stores only scalar addresses. The test runtime owns every
     *          observed MemorySegment and closes their confined Arena.
     */
    private static final class FrameInvocationObserver {
        private long m_firstBeginResultAddress;
        private long m_secondBeginResultAddress;
        private long m_firstPriorMetricsAddress;
        private long m_secondPriorMetricsAddress;
        private long m_firstSubmitResultAddress;
        private long m_secondSubmitResultAddress;
        private int m_beginInvocationCount;
        private int m_submitAndPresentClearFrameInvocationCount;
        private int m_beginOperationResult;
        private int m_submitAndPresentClearFrameOperationResult;
        private boolean m_shouldAlternateMetricsValidity;

        /**
         * @note ThreadSafety: Test-confined; invoke serially from the owning test thread.
         * Writes and observes one begin-frame result.
         *
         * @param long runtimeAddress Synthetic runtime value that is never dereferenced
         * @param int framebufferWidth Test framebuffer width
         * @param int framebufferHeight Test framebuffer height
         * @param MemorySegment beginResult Writable reusable begin-frame output
         * @param MemorySegment priorMetrics Writable reusable prior-metrics output
         * @return int Configured operation result
         * @warning MemoryOwnership: The test runtime owns both segments; this method records
         *          their scalar addresses, writes synchronously and retains no segment.
         */
        private int beginFrame(long runtimeAddress, int framebufferWidth, int framebufferHeight,
                               MemorySegment beginResult, MemorySegment priorMetrics) {
            beginResult.fill((byte) 0);
            priorMetrics.fill((byte) 0);
            if (m_beginInvocationCount == 0) {
                m_firstBeginResultAddress = beginResult.address();
                m_firstPriorMetricsAddress = priorMetrics.address();
            } else if (m_beginInvocationCount == 1) {
                m_secondBeginResultAddress = beginResult.address();
                m_secondPriorMetricsAddress = priorMetrics.address();
            }
            if (m_shouldAlternateMetricsValidity && m_beginInvocationCount == 0) {
                beginResult.set(ValueLayout.JAVA_INT, 12, 1);
                priorMetrics.set(ValueLayout.JAVA_LONG, 0, 91L);
            }
            beginResult.set(ValueLayout.JAVA_INT, 32, -1001);
            beginResult.set(ValueLayout.JAVA_INT, 36, -2001);
            ++m_beginInvocationCount;
            return runtimeAddress == 0L && framebufferWidth == 1280 && framebufferHeight == 720
                    ? m_beginOperationResult : 1;
        }

        /**
         * @note ThreadSafety: Test-confined; invoke serially from the owning test thread.
         * Writes and observes one clear-submit-and-present result.
         *
         * @param long runtimeAddress Synthetic runtime value that is never dereferenced
         * @param float clearRed Test red clear channel
         * @param float clearGreen Test green clear channel
         * @param float clearBlue Test blue clear channel
         * @param MemorySegment submitResult Writable reusable submit output
         * @return int Configured operation result
         * @warning MemoryOwnership: The test runtime owns submitResult; this method records its
         *          scalar address, writes synchronously and retains no segment.
         */
        private int submitAndPresentClearFrame(long runtimeAddress, float clearRed,
                                               float clearGreen, float clearBlue,
                                               MemorySegment submitResult) {
            submitResult.fill((byte) 0);
            if (m_submitAndPresentClearFrameInvocationCount == 0) {
                m_firstSubmitResultAddress = submitResult.address();
            } else if (m_submitAndPresentClearFrameInvocationCount == 1) {
                m_secondSubmitResultAddress = submitResult.address();
            }
            submitResult.set(ValueLayout.JAVA_INT, 4, -1002);
            ++m_submitAndPresentClearFrameInvocationCount;
            return runtimeAddress == 0L && clearRed == 0.1f && clearGreen == 0.2f
                    && clearBlue == 0.3f
                    ? m_submitAndPresentClearFrameOperationResult : 1;
        }
    }

    /**
     * @note ThreadSafety: Test-confined; invoke once on the owning test thread.
     * Throws the configured failure from a Java-only host-create handle.
     *
     * @param RuntimeException invocationFailure Exact injected invocation failure
     * @param MemorySegment createInfo Synthetic host create-info segment
     * @param MemorySegment createResult Synthetic host create-result segment
     * @return int Never returns
     * @warning MemoryOwnership: The test owns both segments and their Arena; this method retains
     *          neither segment and transfers no ownership.
     */
    private static int failHostImageCreate(RuntimeException invocationFailure,
                                           MemorySegment createInfo,
                                           MemorySegment createResult) {
        throw invocationFailure;
    }

    /**
     * @note ThreadSafety: Test-confined; one test thread invokes this observer once.
     * Records a compensating destroy and throws its exact configured failure.
     * @warning MemoryOwnership: Stores only the scalar Native runtime address and owns no Arena or
     *          native memory.
     */
    private static final class OwnershipHandoffObserver {
        private final RuntimeException m_destroyFailure;
        private int m_invocationCount;
        private long m_runtimeAddress;

        /**
         * Creates one deterministic ownership-handoff observer.
         *
         * @param RuntimeException destroyFailure Exact failure thrown by destroy
         */
        private OwnershipHandoffObserver(RuntimeException destroyFailure) {
            m_destroyFailure = destroyFailure;
        }

        /**
         * @note ThreadSafety: Test-confined; invoke once on the owning test thread.
         * Records the consumed runtime address and throws the configured destroy failure.
         *
         * @param long runtimeAddress Synthetic Native runtime address
         * @return int Never returns
         * @warning MemoryOwnership: The address is treated as consumed by this sole attempt; this
         *          observer stores only its scalar value and owns no Native allocation.
         */
        private int destroy(long runtimeAddress) {
            ++m_invocationCount;
            m_runtimeAddress = runtimeAddress;
            throw m_destroyFailure;
        }
    }

    /**
     * @note ThreadSafety: Test-confined; one test thread drives each ordered callback serially.
     * Records owner-factory preparation, Native create, guarded owner construction and destroy.
     * @warning MemoryOwnership: The observer borrows test-owned create segments synchronously and
     *          stores only event strings and scalar counters.
     */
    private static final class InitializationOrderObserver
            implements PresentationRuntimeOwnerFactory {
        private final List<String> m_events = new ArrayList<>();
        private final RuntimeException m_initializationFailure =
                new RuntimeException("ordered owner initialization");
        private int m_destroyInvocationCount;

        /** Creates the prepared owner factory and records that preparation immediately. */
        private InitializationOrderObserver() {
            m_events.add("factoryPrepared");
        }

        /**
         * @note ThreadSafety: Test-confined; invoke once after factory preparation.
         * Writes a successful synthetic Native create result.
         *
         * @param MemorySegment createInfo Synthetic create-info segment
         * @param MemorySegment createResult Writable synthetic create-result segment
         * @return int Native success operation result
         * @warning MemoryOwnership: The test owns both segments; this method writes synchronously
         *          and retains neither address.
         */
        private int createNativeRuntime(MemorySegment createInfo, MemorySegment createResult) {
            m_events.add("nativeCreate");
            createResult.set(ValueLayout.JAVA_LONG, 0, 91L);
            return 0;
        }

        /**
         * @note ThreadSafety: Test-confined; invoke once inside the ownership guard.
         * Records guarded owner construction and throws the deterministic primary failure.
         *
         * @return NativePresentationRuntime Never returns
         * @warning MemoryOwnership: The enclosing guard retains and cleans the synthetic Native
         *          runtime address after this failure.
         */
        @Override
        public NativePresentationRuntime createRuntime() {
            m_events.add("guardedOwnerCreation");
            throw m_initializationFailure;
        }

        /**
         * @note ThreadSafety: Test-confined; invoke exactly once after owner construction fails.
         * Records the compensating destroy attempt.
         *
         * @param long runtimeAddress Synthetic Native runtime address
         * @return int Success only for the expected transferred address
         * @warning MemoryOwnership: This sole attempt consumes runtimeAddress and stores no Native
         *          pointer or allocation.
         */
        private int destroyNativeRuntime(long runtimeAddress) {
            m_events.add("destroy");
            ++m_destroyInvocationCount;
            return runtimeAddress == 91L ? 0 : 1;
        }
    }

    /**
     * @note ThreadSafety: Test-confined; one test thread invokes each instance serially.
     * Observes host detach and submit calls and writes deterministic boundary outputs.
     * @warning MemoryOwnership: Retained MemorySegment references remain owned by the test
     *          runtime and must not be accessed after that runtime closes its Arena.
     */
    private static final class HostImageInvocationObserver {
        private MemorySegment m_firstDetachResult;
        private MemorySegment m_secondDetachResult;
        private MemorySegment m_firstSubmitResult;
        private MemorySegment m_secondSubmitResult;
        private int m_detachInvocationCount;
        private int m_submitInvocationCount;
        private int m_detachOperationResult;
        private int m_submitOperationResult;
        private RuntimeException m_detachInvocationFailure;
        private RuntimeException m_submitInvocationFailure;

        /**
         * @note ThreadSafety: Test-confined; invoke serially from the owning test thread.
         * Writes and observes one reusable detach result.
         *
         * @param long runtimeAddress Synthetic runtime value that is never dereferenced
         * @param MemorySegment detachResult Writable reusable detach output
         * @return int Configured operation result
         * @warning MemoryOwnership: The runtime owns detachResult; this method retains its
         *          reference only for same-lifetime identity assertions.
         */
        private int detach(long runtimeAddress, MemorySegment detachResult) {
            if (m_detachInvocationFailure != null) {
                throw m_detachInvocationFailure;
            }
            detachResult.fill((byte) 0);
            detachResult.set(ValueLayout.JAVA_INT, 0, -77);
            if (m_detachInvocationCount++ == 0) {
                m_firstDetachResult = detachResult;
            } else if (m_detachInvocationCount == 2) {
                m_secondDetachResult = detachResult;
            }
            return runtimeAddress == 0L ? m_detachOperationResult : 1;
        }

        /**
         * @note ThreadSafety: Test-confined; invoke serially from the owning test thread.
         * Writes and observes one reusable host submit result.
         *
         * @param long runtimeAddress Synthetic runtime value that is never dereferenced
         * @param MemorySegment submitResult Writable reusable submit output
         * @return int Configured operation result
         * @warning MemoryOwnership: The runtime owns submitResult; this method retains its
         *          reference only for same-lifetime identity assertions.
         */
        private int submit(long runtimeAddress, MemorySegment submitResult) {
            if (m_submitInvocationFailure != null) {
                throw m_submitInvocationFailure;
            }
            submitResult.fill((byte) 0);
            submitResult.set(ValueLayout.JAVA_INT, 0, 3);
            submitResult.set(ValueLayout.JAVA_INT, 4, -88);
            if (m_submitInvocationCount++ == 0) {
                m_firstSubmitResult = submitResult;
            } else if (m_submitInvocationCount == 2) {
                m_secondSubmitResult = submitResult;
            }
            return runtimeAddress == 0L ? m_submitOperationResult : 1;
        }
    }
}
