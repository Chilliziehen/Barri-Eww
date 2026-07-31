package barrieww.mod.mixin;

import barrieww.core.interoperability.NativePresentationRuntimeException;
import barrieww.core.interoperability.PresentationBootstrapHandles;
import barrieww.mod.BarriEwwClientInitializer;
import barrieww.mod.CorePresentationRuntimeFactory;
import barrieww.mod.MinecraftClearTakeoverReadiness;
import barrieww.mod.MinecraftVulkanBootstrapHandles;
import barrieww.mod.PresentationGenerationInputs;
import barrieww.mod.PresentationTakeoverCoordinator;
import com.mojang.blaze3d.systems.CommandEncoderBackend;
import com.mojang.blaze3d.systems.GpuSurface;
import com.mojang.blaze3d.textures.GpuTextureView;
import com.mojang.blaze3d.vulkan.VulkanDevice;
import com.mojang.blaze3d.vulkan.VulkanGpuSurface;
import java.nio.file.Path;
import java.util.Optional;
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

    @Shadow(aliases = "swapchainSuboptimal")
    private boolean m_swapchainSuboptimal;

    @Unique
    private Logger barrieww$m_logger = s_logger;

    @Unique
    private PresentationTakeoverCoordinator barrieww$m_presentationTakeoverCoordinator;

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
            barrieww$m_presentationTakeoverCoordinator = new PresentationTakeoverCoordinator(
                new CorePresentationRuntimeFactory(nativeLibraryPath.orElseThrow()),
                barrieww$m_logger);
        }
    }

    /**
     * @note ThreadSafety: Render-thread-confined generation transition at backend configure HEAD.
     * Supplies complete or explicitly unready generation inputs, mirrors coordinator recreation
     * state, and cancels vanilla swapchain creation only after a primed Native generation exists or
     * terminal interception is required.
     *
     * @param GpuSurface.Configuration configuration Requested surface generation configuration
     * @param CallbackInfo callbackInformation Cancellable Mixin callback metadata
     * @warning MemoryOwnership: Bootstrap handles and host texture readiness are borrowed snapshots;
     * the coordinator owns any runtime generation created from them.
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

        Optional<PresentationBootstrapHandles> bootstrapHandles =
            MinecraftVulkanBootstrapHandles.extractBorrowedHandles(m_device, m_surface);
        boolean isHostColorTextureReady =
            MinecraftClearTakeoverReadiness.isMinecraftColorTextureReady();
        PresentationGenerationInputs generationInputs = new PresentationGenerationInputs(
            bootstrapHandles.orElse(null),
            configuration.width(),
            configuration.height(),
            true,
            isHostColorTextureReady);
        boolean shouldCancelVanillaConfigure = coordinator.configure(generationInputs);
        m_swapchainSuboptimal = coordinator.requiresReconfiguration();
        if (shouldCancelVanillaConfigure) {
            callbackInformation.cancel();
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
     * Closes the coordinator before Minecraft destroys surface-related objects, contains one checked
     * Core close failure with complete context, and never suppresses vanilla teardown.
     *
     * @param CallbackInfo callbackInformation Non-cancellable Mixin callback metadata
     * @warning MemoryOwnership: Releases coordinator-owned runtime generations before borrowed
     * Minecraft Vulkan objects become invalid.
     */
    @Inject(method = "close()V", at = @At("HEAD"))
    private void barrieww$closePresentationTakeover(CallbackInfo callbackInformation) {
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
            barrieww$m_logger.error(
                "Presentation takeover close failed; Native symbol='"
                    + closeFailure.nativeSymbolName()
                    + "', operation result=" + closeFailure.operationResultCode()
                    + ", Vulkan result=" + closeFailure.vulkanResult(),
                closeFailure);
        }
    }
}
