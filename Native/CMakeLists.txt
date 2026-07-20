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

# Switch dependency validation at configure time (spec §3.3): hot-path logging is
# meaningless without the logging system.
if(BARRIEWW_CRITICAL_HOTPATH_LOG AND NOT BARRIEWW_LOGGING)
    message(FATAL_ERROR
        "BARRIEWW_CRITICAL_HOTPATH_LOG=ON requires BARRIEWW_LOGGING=ON (spec §3.3).")
endif()

# ── Library target ────────────────────────────────────────────────────────────────────
add_library(BarriEwwNative)

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
        src/CommandStream/CommandStreamImageHandleTableValidator.cpp)

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
        BARRIEWW_CRITICAL_HOTPATH_LOG)
    if(${featureSwitch})
        target_compile_definitions(BarriEwwNative PUBLIC ${featureSwitch}=1)
    endif()
endforeach()

# ── Vulkan backend (spec §3.3 BARRIEWW_BACKEND_VULKAN) ─────────────────────────────────
# Only linked when the Vulkan backend is enabled; the backend receives its device/queues
# from the Java/FFM layer (ADR-0001), so no windowing/surface libraries are pulled in.
if(BARRIEWW_BACKEND_VULKAN)
    find_package(Vulkan REQUIRED)
    target_sources(BarriEwwNative
        PRIVATE
            src/Vulkan/VulkanContext.cpp
            src/Vulkan/VulkanBufferTable.cpp
            src/Vulkan/CommandBufferRecorder.cpp
            src/Vulkan/VulkanFrameLoop.cpp
            src/Vulkan/VulkanPipelineTable.cpp
            src/Vulkan/VulkanImageTable.cpp)
    target_link_libraries(BarriEwwNative PUBLIC Vulkan::Vulkan)
endif()

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
