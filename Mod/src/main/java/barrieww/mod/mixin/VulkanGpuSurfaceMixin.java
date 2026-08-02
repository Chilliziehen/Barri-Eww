package barrieww.mod.mixin;

import barrieww.core.interoperability.HostImagePresentationBinding;
import barrieww.core.interoperability.NativePresentationRuntimeException;
import barrieww.core.interoperability.PresentationBootstrapHandles;
import barrieww.mod.BarriEwwClientInitializer;
import barrieww.mod.CorePresentationRuntimeFactory;
import barrieww.mod.MainRenderTargetGenerationTracker;
import barrieww.mod.MainRenderTargetResizeListener;
import barrieww.mod.MinecraftHostImagePresentationBindingExtractor;
import barrieww.mod.MinecraftVulkanBootstrapHandles;
import barrieww.mod.PresentationGenerationInputs;
import barrieww.mod.PresentationTakeoverCoordinator;
import com.mojang.blaze3d.pipeline.RenderTarget;
import com.mojang.blaze3d.systems.CommandEncoderBackend;
import com.mojang.blaze3d.systems.GpuSurface;
import com.mojang.blaze3d.textures.GpuTextureView;
import com.mojang.blaze3d.vulkan.VulkanDevice;
import com.mojang.blaze3d.vulkan.VulkanGpuSurface;
import java.nio.file.Path;
import java.util.Optional;
import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.GameRenderer;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.spongepowered.asm.mixin.Final;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.Unique;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * @note ThreadSafety: Minecraft constructs the surface on its serialized render initialization path.
 * Coordinates Native presentation generations exclusively through VulkanGpuSurface backend methods.
 * @warning MemoryOwnership: Minecraft retains device and surface ownership. The per-surface
 * coordinator exclusively owns only the Core presentation runtime generations it creates.
 */
@Mixin(VulkanGpuSurface.class)
public abstract class VulkanGpuSurfaceMixin {
    private static final Logger s_logger = LoggerFactory.getLogger("Barri-Eww");

    @Shadow(aliases = "device")
    @Final
    private VulkanDevice m_device;

    @Shadow(aliases = "surface")
    @Final
    private long m_surface;

    @Shadow(aliases = "swapchainImageFormat")
    @Final
    private int m_swapchainImageFormat;

    @Shadow(aliases = "swapchainSuboptimal")
    private boolean m_swapchainSuboptimal;

    @Unique
    private Logger barrieww$m_logger = s_logger;

    @Unique
    private PresentationTakeoverCoordinator barrieww$m_presentationTakeoverCoordinator;

    @Unique
    private MainRenderTargetResizeListener barrieww$m_mainRenderTargetResizeListener;

    @Unique
    private long barrieww$m_lastCommittedHostTargetGeneration;

    @Unique
    private long barrieww$m_lastCommittedHostImageHandle;

    @Unique
    private long barrieww$m_pendingHostTargetGeneration;

    @Unique
    private long barrieww$m_pendingHostImageHandle;

    /**
     * @note ThreadSafety: Called once at constructor TAIL on Minecraft's render initialization thread.
     * Retains the non-destructive borrowed-device log and creates one coordinator only after Native
     * library extraction has published a path.
     *
     * @param VulkanDevice device Minecraft Vulkan device supplied to the surface
     * @param long windowHandle Borrowed GLFW window handle supplied to Minecraft
     * @param CallbackInfo callbackInformation Mixin callback metadata
     * @warning MemoryOwnership: The probe borrows all Minecraft Vulkan objects and owns none.
     */
    @Inject(
        method = "<init>(Lcom/mojang/blaze3d/vulkan/VulkanDevice;J)V",
        at = @At("TAIL"))
    private void barrieww$probeBorrowedBootstrapHandles(
        VulkanDevice device,
        long windowHandle,
        CallbackInfo callbackInformation) {
        Optional<PresentationBootstrapHandles> bootstrapHandles =
            MinecraftVulkanBootstrapHandles.extractBorrowedHandles(m_device, m_surface);
        if (bootstrapHandles.isPresent()) {
            s_logger.info(
                "Borrowed Minecraft Vulkan bootstrap VkDevice=0x{}",
                Long.toHexString(bootstrapHandles.orElseThrow().logicalDeviceHandle()));
        }

        Optional<Path> nativeLibraryPath = BarriEwwClientInitializer.nativeLibraryPath();
        if (nativeLibraryPath.isPresent()) {
            PresentationTakeoverCoordinator coordinator = new PresentationTakeoverCoordinator(
                new CorePresentationRuntimeFactory(nativeLibraryPath.orElseThrow()),
                barrieww$m_logger);
            barrieww$m_presentationTakeoverCoordinator = coordinator;
            MainRenderTargetResizeListener resizeListener = () -> {
                PresentationTakeoverCoordinator activeCoordinator =
                    barrieww$m_presentationTakeoverCoordinator;
                return activeCoordinator == null
                    || activeCoordinator.detachHostImagePresentationResources();
            };
            barrieww$m_mainRenderTargetResizeListener = resizeListener;
            try {
                MainRenderTargetGenerationTracker.registerResizeListener(resizeListener);
            } catch (RuntimeException registrationFailure) {
                barrieww$m_mainRenderTargetResizeListener = null;
                barrieww$m_presentationTakeoverCoordinator = null;
                try {
                    coordinator.close();
                } catch (NativePresentationRuntimeException closeFailure) {
                    closeFailure.addSuppressed(registrationFailure);
                    barrieww$logCoordinatorCloseFailure(closeFailure);
                }
                barrieww$m_logger.error(
                    "Presentation takeover resize listener registration failed",
                    registrationFailure);
            }
        }
    }

    /**
     * @note ThreadSafety: Render-thread-confined generation transition at backend configure HEAD.
     * Retires the old runtime before deferred complete target resize and exact host-image extraction,
     * commits the validated generation-handle pair only after coordinator publication, and mirrors
     * coordinator recreation state.
     *
     * @param GpuSurface.Configuration configuration Requested surface generation configuration
     * @param CallbackInfo callbackInformation Cancellable Mixin callback metadata
     * @warning MemoryOwnership: Preparation borrows Minecraft handles only after old runtime retirement.
     * The coordinator retains sole ownership of candidate and committed Native runtime generations.
     */
    @Inject(
        method = "configure(Lcom/mojang/blaze3d/systems/GpuSurface$Configuration;)V",
        at = @At("HEAD"),
        cancellable = true)
    private void barrieww$configurePresentationTakeover(
        GpuSurface.Configuration configuration,
        CallbackInfo callbackInformation) {
        PresentationTakeoverCoordinator coordinator =
            barrieww$m_presentationTakeoverCoordinator;
        if (coordinator == null) {
            return;
        }

        barrieww$m_pendingHostTargetGeneration = 0L;
        barrieww$m_pendingHostImageHandle = 0L;
        boolean shouldCancelVanillaConfigure = coordinator.configure(
            () -> barrieww$prepareGeneration(configuration));
        if (coordinator.hasCommittedHostImagePresentationGeneration()) {
            barrieww$m_lastCommittedHostTargetGeneration =
                barrieww$m_pendingHostTargetGeneration;
            barrieww$m_lastCommittedHostImageHandle = barrieww$m_pendingHostImageHandle;
        }
        barrieww$m_pendingHostTargetGeneration = 0L;
        barrieww$m_pendingHostImageHandle = 0L;
        m_swapchainSuboptimal = coordinator.requiresReconfiguration();
        if (shouldCancelVanillaConfigure) {
            callbackInformation.cancel();
        }
    }

    /**
     * @note ThreadSafety: Render-thread-confined post-retirement configure callback.
     * Resolves the current renderer and main target, performs a full public renderer resize when a
     * positive requested extent differs, then extracts one generation-stable exact host binding.
     * A previously committed generation cannot silently name a changed image handle.
     *
     * @param GpuSurface.Configuration configuration Requested positive framebuffer configuration
     * @return PresentationGenerationInputs Exact ready generation inputs, or null when initialization,
     * extent, generation, bootstrap, extraction, or handle validation is incomplete
     * @warning MemoryOwnership: Minecraft and Vulkan handles remain borrowed. This callback runs only
     * after the coordinator has closed the preceding runtime that could reference the old image.
     */
    @Unique
    private PresentationGenerationInputs barrieww$prepareGeneration(
        GpuSurface.Configuration configuration) {
        try {
            int framebufferWidth = configuration.width();
            int framebufferHeight = configuration.height();
            if (framebufferWidth <= 0 || framebufferHeight <= 0) {
                return null;
            }

            Minecraft minecraft = Minecraft.getInstance();
            if (minecraft == null) {
                return null;
            }
            GameRenderer gameRenderer = minecraft.gameRenderer;
            if (gameRenderer == null) {
                return null;
            }
            RenderTarget mainRenderTarget = gameRenderer.mainRenderTarget();
            if (mainRenderTarget == null) {
                return null;
            }
            if (mainRenderTarget.width != framebufferWidth
                || mainRenderTarget.height != framebufferHeight) {
                gameRenderer.resize(framebufferWidth, framebufferHeight);
            }

            Optional<PresentationBootstrapHandles> bootstrapHandles =
                MinecraftVulkanBootstrapHandles.extractBorrowedHandles(m_device, m_surface);
            if (bootstrapHandles.isEmpty()) {
                return null;
            }
            long generationBeforeExtraction =
                MainRenderTargetGenerationTracker.currentGeneration();
            Optional<HostImagePresentationBinding> hostImageBinding =
                MinecraftHostImagePresentationBindingExtractor.extract(
                    gameRenderer,
                    framebufferWidth,
                    framebufferHeight,
                    m_swapchainImageFormat);
            long generationAfterExtraction =
                MainRenderTargetGenerationTracker.currentGeneration();
            if (generationBeforeExtraction <= 0L
                || generationBeforeExtraction != generationAfterExtraction
                || hostImageBinding.isEmpty()) {
                return null;
            }

            long hostImageHandle = hostImageBinding.orElseThrow().hostImageHandle();
            if (barrieww$m_lastCommittedHostTargetGeneration == generationBeforeExtraction
                && barrieww$m_lastCommittedHostImageHandle != 0L
                && barrieww$m_lastCommittedHostImageHandle != hostImageHandle) {
                return null;
            }

            barrieww$m_pendingHostTargetGeneration = generationBeforeExtraction;
            barrieww$m_pendingHostImageHandle = hostImageHandle;
            return new PresentationGenerationInputs(
                bootstrapHandles.orElseThrow(),
                framebufferWidth,
                framebufferHeight,
                true,
                hostImageBinding.orElseThrow(),
                generationBeforeExtraction);
        } catch (RuntimeException initializationFailure) {
            return null;
        }
    }

    /**
     * @note ThreadSafety: Render-thread-confined frame acquisition hot path.
     * Begins the Native frame and always suppresses vanilla acquisition while the current generation
     * is taken over, allowing the outer GpuSurface to publish its acquired state naturally.
     *
     * @param CallbackInfo callbackInformation Cancellable Mixin callback metadata
     * @warning MemoryOwnership: Uses coordinator-owned runtime state without transferring ownership.
     */
    @Inject(method = "acquireNextTexture()V", at = @At("HEAD"), cancellable = true)
    private void barrieww$acquirePresentationTexture(CallbackInfo callbackInformation) {
        PresentationTakeoverCoordinator coordinator =
            barrieww$m_presentationTakeoverCoordinator;
        if (coordinator == null || !coordinator.isTakenOver()) {
            return;
        }

        coordinator.beginFrame();
        m_swapchainSuboptimal = coordinator.requiresReconfiguration();
        callbackInformation.cancel();
    }

    /**
     * @note ThreadSafety: Render-thread-confined backend frame hot path.
     * Converts only the Vulkan backend blit into an empty operation during takeover so the outer
     * GpuSurface publishes its blitted state naturally.
     *
     * @param CommandEncoderBackend commandEncoderBackend Unused Minecraft backend encoder
     * @param GpuTextureView sourceTextureView Borrowed Minecraft source color view
     * @param CallbackInfo callbackInformation Cancellable Mixin callback metadata
     * @warning MemoryOwnership: The encoder and texture view remain Minecraft-owned and are not used.
     */
    @Inject(
        method = "blitFromTexture(Lcom/mojang/blaze3d/systems/CommandEncoderBackend;"
            + "Lcom/mojang/blaze3d/textures/GpuTextureView;)V",
        at = @At("HEAD"),
        cancellable = true)
    private void barrieww$skipVanillaBlit(
        CommandEncoderBackend commandEncoderBackend,
        GpuTextureView sourceTextureView,
        CallbackInfo callbackInformation) {
        PresentationTakeoverCoordinator coordinator =
            barrieww$m_presentationTakeoverCoordinator;
        if (coordinator != null && coordinator.isTakenOver()) {
            callbackInformation.cancel();
        }
    }

    /**
     * @note ThreadSafety: Render-thread-confined frame presentation hot path.
     * Submits and presents the Native fixed-color frame and always suppresses vanilla presentation
     * while takeover remains active, allowing the outer GpuSurface to clear acquisition naturally.
     *
     * @param CallbackInfo callbackInformation Cancellable Mixin callback metadata
     * @warning MemoryOwnership: Uses coordinator-owned runtime state without transferring ownership.
     */
    @Inject(method = "present()V", at = @At("HEAD"), cancellable = true)
    private void barrieww$presentTakeoverFrame(CallbackInfo callbackInformation) {
        PresentationTakeoverCoordinator coordinator =
            barrieww$m_presentationTakeoverCoordinator;
        if (coordinator == null || !coordinator.isTakenOver()) {
            return;
        }

        coordinator.presentFrame();
        m_swapchainSuboptimal = coordinator.requiresReconfiguration();
        callbackInformation.cancel();
    }

    /**
     * @note ThreadSafety: Render-thread-confined teardown at backend close HEAD.
     * Closes the coordinator before Minecraft destroys surface-related objects and contains one
     * checked Core close failure without retrying the consumed Native runtime address.
     *
     * @param CallbackInfo callbackInformation Non-cancellable Mixin callback metadata
     * @warning MemoryOwnership: The sole destroy attempt finalizes the Native address. This callback
     * detaches the coordinator first and allows Minecraft to continue teardown after checked failure.
     */
    @Inject(method = "close()V", at = @At("HEAD"))
    private void barrieww$closePresentationTakeover(CallbackInfo callbackInformation) {
        MainRenderTargetResizeListener resizeListener =
            barrieww$m_mainRenderTargetResizeListener;
        if (resizeListener != null) {
            barrieww$m_mainRenderTargetResizeListener = null;
            MainRenderTargetGenerationTracker.unregisterResizeListener(resizeListener);
        }

        PresentationTakeoverCoordinator coordinator =
            barrieww$m_presentationTakeoverCoordinator;
        if (coordinator == null) {
            return;
        }
        barrieww$m_presentationTakeoverCoordinator = null;

        try {
            coordinator.close();
            barrieww$m_logger.info(
                "Presentation takeover closed before Minecraft Vulkan surface teardown");
        } catch (NativePresentationRuntimeException closeFailure) {
            barrieww$logCoordinatorCloseFailure(closeFailure);
        }
    }

    /**
     * @note ThreadSafety: Render-thread-confined slow teardown or initialization failure path.
     * Reports complete checked Core close context exactly once at the call site.
     *
     * @param NativePresentationRuntimeException closeFailure Consumed-runtime close failure
     * @warning MemoryOwnership: Logging observes but does not own the checked failure.
     */
    @Unique
    private void barrieww$logCoordinatorCloseFailure(
        NativePresentationRuntimeException closeFailure) {
        barrieww$m_logger.error(
            "Presentation takeover close failed; Native symbol='"
                + closeFailure.nativeSymbolName()
                + "', operation result=" + closeFailure.operationResultCode()
                + ", Vulkan result=" + closeFailure.vulkanResult(),
            closeFailure);
    }
}
