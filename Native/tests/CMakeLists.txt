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
    CommandStreamBufferHandleTableValidatorTests.cpp)
if(BARRIEWW_BACKEND_VULKAN)
    list(APPEND BARRIEWW_TEST_SOURCES VulkanContextTests.cpp)
endif()

add_executable(BarriEwwNativeTests ${BARRIEWW_TEST_SOURCES})

target_link_libraries(BarriEwwNativeTests
    PRIVATE
        BarriEwwNative
        Catch2::Catch2WithMain)

# Repository-level shared fixtures (cross-language golden files, ADR-0002).
target_compile_definitions(BarriEwwNativeTests
    PRIVATE
        BARRIEWW_TEST_DATA_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/../../TestData")

# Register each Catch2 TEST_CASE with CTest so `ctest` runs them individually.
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(BarriEwwNativeTests)
