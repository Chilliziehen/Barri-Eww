package barrieww.mod.mixin;

import com.mojang.blaze3d.systems.GpuDevice;
import com.mojang.blaze3d.systems.GpuDeviceBackend;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;

/**
 * @note ThreadSafety: The accessor performs an immutable field read; the caller must obey the
 * Minecraft device lifecycle and thread constraints.
 * Provides read-only access to the Minecraft device backend during bootstrap extraction.
 * @warning MemoryOwnership: Minecraft owns the returned backend; callers only borrow its reference.
 */
@Mixin(GpuDevice.class)
public interface GpuDeviceAccessor {
    /** Returns the backend owned by the target Minecraft device. */
    @Accessor("backend")
    GpuDeviceBackend barrieww$getBackend();
}
