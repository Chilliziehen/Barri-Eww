foreach(requiredVariable
        compiledShaderFile
        generatedHeaderFile
        embeddedShaderIdentifier)
    if(NOT DEFINED ${requiredVariable} OR "${${requiredVariable}}" STREQUAL "")
        message(FATAL_ERROR "${requiredVariable} must be provided")
    endif()
endforeach()

file(READ "${compiledShaderFile}" compiledShaderHexadecimal HEX)
string(LENGTH "${compiledShaderHexadecimal}" compiledShaderHexadecimalLength)

if(compiledShaderHexadecimalLength LESS 8)
    message(FATAL_ERROR "Compiled shader is smaller than the SPIR-V header")
endif()

string(SUBSTRING "${compiledShaderHexadecimal}" 0 8 compiledShaderMagic)
if(NOT compiledShaderMagic STREQUAL "03022307")
    message(FATAL_ERROR "Compiled shader does not begin with the SPIR-V magic word")
endif()

math(EXPR compiledShaderByteCount "${compiledShaderHexadecimalLength} / 2")
math(EXPR compiledShaderWordRemainder "${compiledShaderByteCount} % 4")
if(NOT compiledShaderWordRemainder EQUAL 0)
    message(FATAL_ERROR "Compiled shader size is not aligned to a SPIR-V word")
endif()

string(REGEX MATCHALL ".." compiledShaderBytes "${compiledShaderHexadecimal}")
set(embeddedShaderValues "")
foreach(compiledShaderByte IN LISTS compiledShaderBytes)
    string(APPEND embeddedShaderValues "    std::byte{0x${compiledShaderByte}},\n")
endforeach()

set(generatedHeaderContent
"#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace barrieww::vulkan::shaders {

alignas(std::uint32_t) inline constexpr std::array<std::byte, ${compiledShaderByteCount}>
    ${embeddedShaderIdentifier}{
${embeddedShaderValues}};

} // namespace barrieww::vulkan::shaders
")

cmake_path(GET generatedHeaderFile PARENT_PATH generatedHeaderDirectory)
file(MAKE_DIRECTORY "${generatedHeaderDirectory}")
file(WRITE "${generatedHeaderFile}" "${generatedHeaderContent}")
