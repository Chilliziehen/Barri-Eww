# Native unit tests (spec §4.1) — Catch2 (spec §4.1 framework choice).

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.4)
FetchContent_MakeAvailable(Catch2)

set(BARRIEWW_TEST_SOURCES
    BuildConfigurationTests.cpp
    CommandStreamValidatorTests.cpp
    CommandStreamModuleValidatorTests.cpp
    CommandStreamGoldenTests.cpp
    CommandStreamDisassemblerTests.cpp
    CommandStreamImageFormatTests.cpp
    CommandStreamBufferHandleTableValidatorTests.cpp
    CommandStreamBarrierBatchTableValidatorTests.cpp
    CommandStreamPipelineAndShaderTableValidatorTests.cpp
    CommandStreamImageHandleTableValidatorTests.cpp
    CommandStreamImageViewHandleTableValidatorTests.cpp
    CommandStreamRenderingTemplateTableValidatorTests.cpp
    CommandStreamGraphicsPipelineTableValidatorTests.cpp
    NativeCommandStreamModuleValidationOperationTests.cpp)
if(BARRIEWW_BACKEND_VULKAN)
    list(APPEND BARRIEWW_TEST_SOURCES
        VulkanContextTests.cpp
        VulkanFormatMappingTests.cpp
        VulkanFrameLoopTests.cpp
        VulkanImageTableTests.cpp
        VulkanPipelineTableTests.cpp
        CommandBufferRecorderTests.cpp
        ComputeDispatchTests.cpp
        IndirectDispatchTests.cpp
        ImageExecutionTests.cpp
        VulkanImageViewTableTests.cpp
        DynamicRenderingTests.cpp
        GraphicsPipelineExecutionTests.cpp
        GraphicsVertexPullingTests.cpp
        GraphicsModuleExecutionTests.cpp
        GpuDrivenDrawTests.cpp)
endif()

add_executable(BarriEwwNativeTests ${BARRIEWW_TEST_SOURCES})

add_executable(BarriEwwNativeFfmBoundaryTests
    NativeCommandStreamModuleValidationBoundaryTests.cpp)

target_link_libraries(BarriEwwNativeTests
    PRIVATE
        BarriEwwNative
        Catch2::Catch2WithMain)

target_link_libraries(BarriEwwNativeFfmBoundaryTests
    PRIVATE
        BarriEwwNativeFfm
        Catch2::Catch2WithMain)

if(BARRIEWW_BACKEND_VULKAN)
    add_executable(BarriEwwVulkanFrameLoopFailureTests
        VulkanFrameLoopFailureTests.cpp
        ../src/Vulkan/VulkanContext.cpp
        ../src/Vulkan/VulkanFrameLoop.cpp)
    target_include_directories(BarriEwwVulkanFrameLoopFailureTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(BarriEwwVulkanFrameLoopFailureTests
        PRIVATE
            Catch2::Catch2WithMain)
    add_executable(BarriEwwVulkanImageTableFailureTests
        VulkanImageTableFailureTests.cpp
        ../src/CommandStream/CommandStreamImageHandleTableValidator.cpp
        ../src/Vulkan/VulkanContext.cpp
        ../src/Vulkan/VulkanImageTable.cpp)
    target_include_directories(BarriEwwVulkanImageTableFailureTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(BarriEwwVulkanImageTableFailureTests
        PRIVATE
            Catch2::Catch2WithMain)

    add_executable(BarriEwwVulkanPipelineTableFailureTests
        VulkanPipelineTableFailureTests.cpp
        ../src/CommandStream/CommandStreamPipelineHandleTableValidator.cpp
        ../src/CommandStream/CommandStreamShaderModuleTableValidator.cpp
        ../src/Vulkan/VulkanContext.cpp
        ../src/Vulkan/VulkanPipelineTable.cpp)
    target_include_directories(BarriEwwVulkanPipelineTableFailureTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(BarriEwwVulkanPipelineTableFailureTests
        PRIVATE
            Catch2::Catch2WithMain)

    add_executable(BarriEwwNativePresentationRuntimeTests
        NativePresentationRuntimeBoundaryTests.cpp
        ../src/Interoperability/NativePresentationRuntimeBoundary.cpp
        ../src/Vulkan/VulkanPresentationRuntime.cpp)
    target_include_directories(BarriEwwNativePresentationRuntimeTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${Vulkan_INCLUDE_DIRS})
    target_compile_definitions(BarriEwwNativePresentationRuntimeTests
        PRIVATE
            BARRIEWW_NATIVE_FFM_EXPORTS=1)
    target_link_libraries(BarriEwwNativePresentationRuntimeTests
        PRIVATE
            Catch2::Catch2WithMain)

    foreach(failureTestTarget
            BarriEwwVulkanFrameLoopFailureTests
            BarriEwwVulkanImageTableFailureTests
            BarriEwwVulkanPipelineTableFailureTests
            BarriEwwNativePresentationRuntimeTests)
        if(BARRIEWW_ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${failureTestTarget} PRIVATE --coverage -O0 -g)
            target_link_options(${failureTestTarget} PRIVATE --coverage)
        endif()
    endforeach()
endif()

# Repository-level shared fixtures (cross-language golden files, ADR-0002).
target_compile_definitions(BarriEwwNativeTests
    PRIVATE
        BARRIEWW_TEST_DATA_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/../../TestData")
target_compile_definitions(BarriEwwNativeFfmBoundaryTests
    PRIVATE
        BARRIEWW_TEST_DATA_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/../../TestData")

# Register each Catch2 TEST_CASE with CTest so `ctest` runs them individually.
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(BarriEwwNativeTests)
if(BARRIEWW_BACKEND_VULKAN)
    catch_discover_tests(BarriEwwVulkanFrameLoopFailureTests)
    catch_discover_tests(BarriEwwVulkanImageTableFailureTests)
    catch_discover_tests(BarriEwwVulkanPipelineTableFailureTests)
    catch_discover_tests(BarriEwwNativePresentationRuntimeTests)
endif()
catch_discover_tests(BarriEwwNativeFfmBoundaryTests
    DL_PATHS
        $<TARGET_FILE_DIR:BarriEwwNativeFfm>)
