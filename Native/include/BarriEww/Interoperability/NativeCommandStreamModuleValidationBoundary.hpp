#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#if defined(_WIN32)
    #if defined(BARRIEWW_NATIVE_FFM_EXPORTS)
        #define BARRIEWW_NATIVE_FFM_EXPORT __declspec(dllexport)
    #else
        #define BARRIEWW_NATIVE_FFM_EXPORT __declspec(dllimport)
    #endif
#else
    #define BARRIEWW_NATIVE_FFM_EXPORT __attribute__((visibility("default")))
#endif

namespace barrieww {

/**
 * @note ThreadSafety: Enumeration; trivially thread-safe.
 * @brief Stable operation-level results returned by the Version 1 module-validation
 *        boundary. Values are part of the Java-to-Native ABI and never change in place.
 */
enum class NativeCommandStreamModuleValidationOperationResult : std::uint32_t {
    /** Native validation completed successfully. */
    Success = 0,
    /** One or more boundary arguments violate the Version 1 contract. */
    InvalidArgument = 1,
    /** The supplied bytes failed BECS module or embedded lane validation. */
    ValidationFailure = 2,
    /** An unexpected native exception was contained by the boundary. */
    InternalFailure = 3,
};

/**
 * @note ThreadSafety: Plain value type; separate instances may be written concurrently.
 * @brief Fixed-layout Version 1 validation status written by Native and decoded by Java.
 * @warning MemoryOwnership: Java owns the status memory and keeps it live for the entire
 *          downcall. Native writes it synchronously and retains no pointer after return.
 */
struct NativeCommandStreamModuleValidationStatus {
    std::uint32_t moduleValidationErrorCode;
    std::uint32_t laneStreamValidationErrorCode;
    std::uint64_t moduleByteOffset;
    std::uint64_t laneStreamByteOffset;
};

static_assert(sizeof(NativeCommandStreamModuleValidationStatus) == 24u);
static_assert(alignof(NativeCommandStreamModuleValidationStatus) == 8u);
static_assert(offsetof(NativeCommandStreamModuleValidationStatus,
                       moduleValidationErrorCode) == 0u);
static_assert(offsetof(NativeCommandStreamModuleValidationStatus,
                       laneStreamValidationErrorCode) == 4u);
static_assert(offsetof(NativeCommandStreamModuleValidationStatus, moduleByteOffset) == 8u);
static_assert(offsetof(NativeCommandStreamModuleValidationStatus, laneStreamByteOffset) == 16u);
static_assert(std::is_standard_layout_v<NativeCommandStreamModuleValidationStatus>);
static_assert(std::is_trivially_copyable_v<NativeCommandStreamModuleValidationStatus>);

} // namespace barrieww

/**
 * @note ThreadSafety: Concurrency-safe. The function has no shared mutable state and may
 *       validate distinct or identical byte ranges concurrently.
 * @brief Synchronously validates one BECM container and every embedded BECS lane through
 *        the fixed Version 1 FFM boundary. Resource-table schemas and Vulkan state are
 *        outside this boundary.
 * @param const void* moduleBytes Complete candidate module base, aligned to 8 bytes
 * @param std::uint64_t moduleByteSize Exact candidate module size in bytes
 * @param barrieww::NativeCommandStreamModuleValidationStatus* validationStatus Writable
 *        24-byte status storage; cleared before validation begins
 * @return barrieww::NativeCommandStreamModuleValidationOperationResult Stable operation
 *         result distinguishing success, invalid arguments, validation failure, and a
 *         contained unexpected native failure
 * @warning MemoryOwnership: Java owns moduleBytes and validationStatus. Native borrows
 *          both only for this synchronous call, allocates or frees neither, and retains
 *          no pointer or validated view after return.
 */
extern "C" BARRIEWW_NATIVE_FFM_EXPORT
barrieww::NativeCommandStreamModuleValidationOperationResult
barriEwwValidateCommandStreamModuleVersion1(
    const void* moduleBytes,
    std::uint64_t moduleByteSize,
    barrieww::NativeCommandStreamModuleValidationStatus* validationStatus) noexcept;
