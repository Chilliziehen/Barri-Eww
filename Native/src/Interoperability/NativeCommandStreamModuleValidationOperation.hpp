#pragma once

#include <cstddef>
#include <expected>
#include <span>

#include "BarriEww/CommandStream/CommandStreamModuleValidationFailure.hpp"
#include "BarriEww/CommandStream/CommandStreamModuleView.hpp"
#include "BarriEww/Interoperability/NativeCommandStreamModuleValidationBoundary.hpp"

namespace barrieww::interoperability {

using NativeCommandStreamModuleValidationFunction =
    std::expected<CommandStreamModuleView, CommandStreamModuleValidationFailure> (*)(
        std::span<const std::byte> moduleBytes);

/**
 * @note ThreadSafety: Concurrency-safe when validationFunction is concurrency-safe; the
 *       operation has no other shared mutable state.
 * @brief Implements Version 1 argument validation, status mapping, and exception
 *        containment around an injected module-validation function.
 * @param const void* moduleBytes Complete candidate module base, aligned to 8 bytes
 * @param std::uint64_t moduleByteSize Exact candidate module size in bytes
 * @param NativeCommandStreamModuleValidationStatus* validationStatus Writable status
 * @param NativeCommandStreamModuleValidationFunction validationFunction Validation
 *        implementation invoked inside the exception-containment boundary
 * @return NativeCommandStreamModuleValidationOperationResult Stable operation result
 * @warning MemoryOwnership: The caller owns both byte ranges. This operation borrows them
 *          synchronously and retains no pointer or validated view after return.
 */
[[nodiscard]] NativeCommandStreamModuleValidationOperationResult
executeNativeCommandStreamModuleValidationVersion1(
    const void* moduleBytes,
    std::uint64_t moduleByteSize,
    NativeCommandStreamModuleValidationStatus* validationStatus,
    NativeCommandStreamModuleValidationFunction validationFunction) noexcept;

} // namespace barrieww::interoperability
