# Barri-Eww Native — C++ rendering backend (spec §3).
#
# Build system lives inside the module (spec §3.1); the repo root has no build system.
# All platform / feature differences are expressed as compile-time switches injected as
# macros (spec §3.2 / §3.3), never as runtime feature detection (T0[1]).

cmake_minimum_required(VERSION 4.4)

project(BarriEwwNative
    VERSION 0.1.0
    DESCRIPTION "Barri-Eww native rendering backend"
    LANGUAGES CXX)

# ── Language standard (spec §3.1): C++23, required, no compiler extensions ─────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# All sources are UTF-8; force MSVC to read/emit UTF-8 regardless of the system
# codepage (silences C4819 on non-UTF-8 locales).
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

# ── Compile-time feature / platform switches (spec §3.3 registry) ──────────────────────
# These are injected as macros below and consumed via #if in product code.
option(THREADED_RECORDING            "Enable multithreaded command recording"          ON)
option(BARRIEWW_BACKEND_VULKAN       "Enable the Vulkan backend (MVP)"                 ON)
option(BARRIEWW_BACKEND_DX12         "Enable the DX12 backend (non-MVP placeholder)"   OFF)
option(BARRIEWW_BACKEND_METAL        "Enable the Metal backend (non-MVP placeholder)"  OFF)
option(BARRIEWW_LOGGING              "Enable the logging system"                       ON)
option(BARRIEWW_CRITICAL_HOTPATH_LOG "Allow log I/O on the command-recording hot path" OFF)

# ── Build-orchestration options (registered in §3.3 as the "build" dimension) ──────────
# They only affect what CMake builds, not compiled-in product behavior, so they are NOT
# injected as product #if macros (spec §3.3 dimension note).
option(BARRIEWW_BUILD_TESTS    "Build Native unit tests (Catch2)"        ON)
option(BARRIEWW_ENABLE_COVERAGE "Instrument for coverage (GCC/Clang)"    OFF)
option(BARRIEWW_GPU_FRAME_METRICS "Compile GPU timestamp frame metrics" OFF)

# Switch dependency validation at configure time (spec §3.3): hot-path logging is
# meaningless without the logging system.
if(BARRIEWW_CRITICAL_HOTPATH_LOG AND NOT BARRIEWW_LOGGING)
    message(FATAL_ERROR
        "BARRIEWW_CRITICAL_HOTPATH_LOG=ON requires BARRIEWW_LOGGING=ON (spec §3.3).")
endif()

# ── Internal static implementation target ───────────────────────────────────────────────
add_library(BarriEwwNative STATIC)
set_target_properties(BarriEwwNative PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)

target_sources(BarriEwwNative
    PRIVATE
        src/BuildConfiguration.cpp
        # BECS command-stream format layer (ADR-0002): backend-neutral core, always built.
        src/CommandStream/CommandStreamValidator.cpp
        src/CommandStream/CommandStreamModuleValidator.cpp
        src/CommandStream/CommandStreamDisassembler.cpp
        src/CommandStream/CommandStreamBufferHandleTableValidator.cpp
        src/CommandStream/CommandStreamBarrierBatchTableValidator.cpp
        src/CommandStream/CommandStreamShaderModuleTableValidator.cpp
        src/CommandStream/CommandStreamPipelineHandleTableValidator.cpp
        src/CommandStream/CommandStreamImageHandleTableValidator.cpp
        src/CommandStream/CommandStreamImageViewHandleTableValidator.cpp
        src/CommandStream/CommandStreamRenderingTemplateTableValidator.cpp
        src/CommandStream/CommandStreamGraphicsPipelineTableValidator.cpp
        src/Interoperability/NativeCommandStreamModuleValidationOperation.cpp)

target_include_directories(BarriEwwNative
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Inject each enabled switch as a =1 macro so `#if THREADED_RECORDING` works, and leave
# disabled switches undefined so no runtime branch or dead code survives (T0[1]).
foreach(featureSwitch
        THREADED_RECORDING
        BARRIEWW_BACKEND_VULKAN
        BARRIEWW_BACKEND_DX12
        BARRIEWW_BACKEND_METAL
        BARRIEWW_LOGGING
        BARRIEWW_CRITICAL_HOTPATH_LOG
        BARRIEWW_GPU_FRAME_METRICS)
    if(${featureSwitch})
        target_compile_definitions(BarriEwwNative PUBLIC ${featureSwitch}=1)
    endif()
endforeach()

# ── Vulkan backend (spec §3.3 BARRIEWW_BACKEND_VULKAN) ─────────────────────────────────
# Only linked when the Vulkan backend is enabled; the backend receives its device/queues
# from the Java/FFM layer (ADR-0001), so no windowing/surface libraries are pulled in.
if(BARRIEWW_BACKEND_VULKAN)
    find_package(Vulkan REQUIRED COMPONENTS glslc)
    set(hostImagePresentationGeneratedShaderDirectory
        "${CMAKE_CURRENT_BINARY_DIR}/generated/BarriEww/Vulkan/Shaders")
    set(hostImagePresentationCompiledShaderDirectory
        "${CMAKE_CURRENT_BINARY_DIR}/compiled-shaders")
    set(hostImagePresentationVertexShaderSource
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Vulkan/Shaders/HostImagePresentationVertexShader.vert")
    set(hostImagePresentationFragmentShaderSource
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Vulkan/Shaders/HostImagePresentationFragmentShader.frag")
    set(hostImagePresentationVertexCompiledShader
        "${hostImagePresentationCompiledShaderDirectory}/HostImagePresentationVertexShader.spv")
    set(hostImagePresentationFragmentCompiledShader
        "${hostImagePresentationCompiledShaderDirectory}/HostImagePresentationFragmentShader.spv")
    set(hostImagePresentationVertexShaderHeader
        "${hostImagePresentationGeneratedShaderDirectory}/HostImagePresentationVertexShader.hpp")
    set(hostImagePresentationFragmentShaderHeader
        "${hostImagePresentationGeneratedShaderDirectory}/HostImagePresentationFragmentShader.hpp")

    add_custom_command(
        OUTPUT ${hostImagePresentationVertexCompiledShader}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${hostImagePresentationCompiledShaderDirectory}
        COMMAND Vulkan::glslc --target-env=vulkan1.2 -O
                ${hostImagePresentationVertexShaderSource}
                -o ${hostImagePresentationVertexCompiledShader}
        DEPENDS
            ${hostImagePresentationVertexShaderSource}
            Vulkan::glslc
        VERBATIM)
    add_custom_command(
        OUTPUT ${hostImagePresentationFragmentCompiledShader}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${hostImagePresentationCompiledShaderDirectory}
        COMMAND Vulkan::glslc --target-env=vulkan1.2 -O
                ${hostImagePresentationFragmentShaderSource}
                -o ${hostImagePresentationFragmentCompiledShader}
        DEPENDS
            ${hostImagePresentationFragmentShaderSource}
            Vulkan::glslc
        VERBATIM)

    set(generateEmbeddedShaderHeaderScript
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateEmbeddedShaderHeader.cmake")
    add_custom_command(
        OUTPUT ${hostImagePresentationVertexShaderHeader}
        COMMAND ${CMAKE_COMMAND}
                -DcompiledShaderFile=${hostImagePresentationVertexCompiledShader}
                -DgeneratedHeaderFile=${hostImagePresentationVertexShaderHeader}
                -DembeddedShaderIdentifier=g_hostImagePresentationVertexShaderBinary
                -P ${generateEmbeddedShaderHeaderScript}
        DEPENDS
            ${hostImagePresentationVertexCompiledShader}
            ${generateEmbeddedShaderHeaderScript}
        VERBATIM)
    add_custom_command(
        OUTPUT ${hostImagePresentationFragmentShaderHeader}
        COMMAND ${CMAKE_COMMAND}
                -DcompiledShaderFile=${hostImagePresentationFragmentCompiledShader}
                -DgeneratedHeaderFile=${hostImagePresentationFragmentShaderHeader}
                -DembeddedShaderIdentifier=g_hostImagePresentationFragmentShaderBinary
                -P ${generateEmbeddedShaderHeaderScript}
        DEPENDS
            ${hostImagePresentationFragmentCompiledShader}
            ${generateEmbeddedShaderHeaderScript}
        VERBATIM)

    add_custom_target(BarriEwwHostImagePresentationShaders
        DEPENDS
            ${hostImagePresentationVertexShaderHeader}
            ${hostImagePresentationFragmentShaderHeader})
    add_dependencies(BarriEwwNative BarriEwwHostImagePresentationShaders)
    target_sources(BarriEwwNative
        PRIVATE
            ${hostImagePresentationVertexShaderHeader}
            ${hostImagePresentationFragmentShaderHeader}
            src/Vulkan/VulkanContext.cpp
            src/Vulkan/VulkanBufferTable.cpp
            src/Vulkan/CommandBufferRecorder.cpp
            src/Vulkan/VulkanFrameLoop.cpp
            src/Vulkan/VulkanPipelineTable.cpp
            src/Vulkan/VulkanImageTable.cpp
            src/Vulkan/VulkanImageViewTable.cpp
            src/Vulkan/VulkanGraphicsPipelineTable.cpp
            src/Vulkan/VulkanHostImagePresentationResources.cpp
            src/Vulkan/VulkanPresentationRuntime.cpp)
    target_include_directories(BarriEwwNative
        PRIVATE
            ${CMAKE_CURRENT_BINARY_DIR}/generated)
    target_link_libraries(BarriEwwNative PUBLIC Vulkan::Vulkan)
endif()

# ── Java-loadable FFM shared boundary (spec §3.6 / §6.6) ────────────────────────────────
add_library(BarriEwwNativeFfm SHARED
    src/Interoperability/NativeCommandStreamModuleValidationBoundary.cpp)

# The presentation runtime FFM boundary is inherently Vulkan/swapchain (ADR-0004), so it is
# only compiled into the shared library when the Vulkan backend is enabled.
if(BARRIEWW_BACKEND_VULKAN)
    target_sources(BarriEwwNativeFfm
        PRIVATE
            src/Interoperability/NativePresentationRuntimeBoundary.cpp)
endif()

target_include_directories(BarriEwwNativeFfm
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(BarriEwwNativeFfm
    PRIVATE
        BarriEwwNative)

target_compile_definitions(BarriEwwNativeFfm
    PRIVATE
        BARRIEWW_NATIVE_FFM_EXPORTS=1)

set_target_properties(BarriEwwNativeFfm PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/ffm"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/ffm"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/ffm")

foreach(configurationName Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${configurationName}" configurationNameUpper)
    set_target_properties(BarriEwwNativeFfm PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${configurationNameUpper} "${CMAKE_BINARY_DIR}/ffm"
        LIBRARY_OUTPUT_DIRECTORY_${configurationNameUpper} "${CMAKE_BINARY_DIR}/ffm"
        ARCHIVE_OUTPUT_DIRECTORY_${configurationNameUpper} "${CMAKE_BINARY_DIR}/ffm")
endforeach()

# ── Coverage instrumentation (spec §4.5 gate: Native >= 90%) ───────────────────────────
if(BARRIEWW_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(BarriEwwNative PUBLIC --coverage -O0 -g)
        target_link_options(BarriEwwNative PUBLIC --coverage)
    else()
        message(WARNING "BARRIEWW_ENABLE_COVERAGE is only supported on GCC/Clang; ignored.")
    endif()
endif()

# ── Tests (spec §4.1) ──────────────────────────────────────────────────────────────────
if(BARRIEWW_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
