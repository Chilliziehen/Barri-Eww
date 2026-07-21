#pragma once

#include <string>

namespace barrieww {

/**
 * @note ThreadSafety: Fully thread-safe. Every member is pure and stateless; the feature
 *       queries are constant expressions and describe() reads no shared mutable state.
 * @brief Compile-time introspection of the enabled build variant (the feature and backend
 *        switches from BuildSystem §3.3). Every feature query is constexpr so callers can
 *        branch at compile time (T0[1]); no runtime feature detection is ever performed.
 *
 * A switch reads as enabled here exactly when CMake injected its `<SWITCH>=1` macro
 * (see Native/CMakeLists.txt); a disabled switch leaves the macro undefined.
 */
class BuildConfiguration {
public:
    /**
     * @note ThreadSafety: Thread-safe (constant expression).
     * @brief Whether multithreaded command recording (THREADED_RECORDING) was compiled in.
     * @return bool True when THREADED_RECORDING was enabled at build time.
     */
    [[nodiscard]] static constexpr bool isThreadedRecordingEnabled() noexcept {
#if defined(THREADED_RECORDING) && THREADED_RECORDING
        return true;
#else
        return false;
#endif
    }

    /**
     * @note ThreadSafety: Thread-safe (constant expression).
     * @brief Whether the Vulkan backend (BARRIEWW_BACKEND_VULKAN) was compiled in.
     * @return bool True when BARRIEWW_BACKEND_VULKAN was enabled at build time.
     */
    [[nodiscard]] static constexpr bool isVulkanBackendEnabled() noexcept {
#if defined(BARRIEWW_BACKEND_VULKAN) && BARRIEWW_BACKEND_VULKAN
        return true;
#else
        return false;
#endif
    }

    /**
     * @note ThreadSafety: Thread-safe (constant expression).
     * @brief Whether the DX12 backend (BARRIEWW_BACKEND_DX12) was compiled in.
     * @return bool True when BARRIEWW_BACKEND_DX12 was enabled at build time.
     */
    [[nodiscard]] static constexpr bool isDirectX12BackendEnabled() noexcept {
#if defined(BARRIEWW_BACKEND_DX12) && BARRIEWW_BACKEND_DX12
        return true;
#else
        return false;
#endif
    }

    /**
     * @note ThreadSafety: Thread-safe (constant expression).
     * @brief Whether the Metal backend (BARRIEWW_BACKEND_METAL) was compiled in.
     * @return bool True when BARRIEWW_BACKEND_METAL was enabled at build time.
     */
    [[nodiscard]] static constexpr bool isMetalBackendEnabled() noexcept {
#if defined(BARRIEWW_BACKEND_METAL) && BARRIEWW_BACKEND_METAL
        return true;
#else
        return false;
#endif
    }

    /**
     * @note ThreadSafety: Thread-safe (constant expression).
     * @brief Whether the logging system (BARRIEWW_LOGGING) was compiled in.
     * @return bool True when BARRIEWW_LOGGING was enabled at build time.
     */
    [[nodiscard]] static constexpr bool isLoggingEnabled() noexcept {
#if defined(BARRIEWW_LOGGING) && BARRIEWW_LOGGING
        return true;
#else
        return false;
#endif
    }

    /**
     * @note ThreadSafety: Thread-safe (constant expression).
     * @brief Whether hot-path log I/O (BARRIEWW_CRITICAL_HOTPATH_LOG) was compiled in.
     *        Off by default because T0[1] forbids log I/O on the recording hot path; this
     *        is a diagnostics-only escape hatch (spec §7.2.1).
     * @return bool True when BARRIEWW_CRITICAL_HOTPATH_LOG was enabled at build time.
     */
    [[nodiscard]] static constexpr bool isCriticalHotpathLogEnabled() noexcept {
#if defined(BARRIEWW_CRITICAL_HOTPATH_LOG) && BARRIEWW_CRITICAL_HOTPATH_LOG
        return true;
#else
        return false;
#endif
    }

    /**
     * @note ThreadSafety: Thread-safe. Constructs and returns a fresh string; touches no
     *       shared mutable state.
     * @brief Builds a human-readable, multi-line summary of the compiled build variant,
     *        intended for startup diagnostics (a slow path). Never call this from the
     *        command-recording hot path (spec §7.2.3).
     * @return std::string A newly constructed summary describing each enabled switch.
     * @warning MemoryOwnership: The returned std::string is owned by the caller (returned
     *          by value). No ownership crosses any FFM boundary in this call.
     */
    [[nodiscard]] static std::string describe();
};

} // namespace barrieww
