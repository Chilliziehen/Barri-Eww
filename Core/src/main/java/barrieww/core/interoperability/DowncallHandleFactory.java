package barrieww.core.interoperability;

import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.Linker;
import java.lang.foreign.MemorySegment;
import java.lang.invoke.MethodHandle;

/**
 * @note ThreadSafety: Initialization-confined; callers invoke an instance only while constructing
 *       one binding on the thread that owns its lookup Arena.
 * Creates one FFM downcall handle with explicit linker options so tests can pin criticality.
 * @warning MemoryOwnership: The returned handle borrows symbolAddress and must not outlive the
 *          Arena that owns that address. This factory neither owns nor closes the lookup Arena.
 */
@FunctionalInterface
interface DowncallHandleFactory {
    /**
     * @note ThreadSafety: Initialization-confined; invoke on the lookup Arena owner thread.
     * Creates one downcall handle using exactly the supplied descriptor and linker options.
     *
     * @param MemorySegment symbolAddress Borrowed native symbol address
     * @param FunctionDescriptor functionDescriptor Fixed native function descriptor
     * @param Linker.Option[] linkerOptions Explicit downcall options; empty means non-critical
     * @return MethodHandle Downcall handle tied to the symbol address lifetime
     * @warning MemoryOwnership: The returned handle borrows symbolAddress and shares its owning
     *          lookup Arena lifetime; callers must not cache it globally or close it independently.
     */
    MethodHandle createDowncallHandle(MemorySegment symbolAddress,
                                      FunctionDescriptor functionDescriptor,
                                      Linker.Option... linkerOptions);
}
