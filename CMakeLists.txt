cmake_minimum_required(VERSION 3.21)
project(PeaceWalkerUltraWideFix VERSION 0.1.0 LANGUAGES C CXX)

set(PWUWFIX_RELEASE "v0.1.0-rc.3" CACHE STRING "Public release label")
option(PWUWFIX_FORCE_SIGNATURE_FALLBACK
    "Maintainer-only: ignore known PE identities to exercise signature resolution" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

include(CTest)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# The winmm proxy is generated from an export list: it produces the .def file,
# declarations, and assembly trampolines.
set(WINMM_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated/winmm")
if(MSVC)
    enable_language(ASM_MASM)
    set(WINMM_ASSEMBLY_FORMAT masm)
    set(WINMM_ASSEMBLY "${WINMM_GENERATED_DIR}/winmm_trampolines.asm")
else()
    enable_language(ASM)
    set(WINMM_ASSEMBLY_FORMAT gnu)
    set(WINMM_ASSEMBLY "${WINMM_GENERATED_DIR}/winmm_trampolines.S")
endif()
set(WINMM_DEFINITION "${WINMM_GENERATED_DIR}/winmm.def")
set(WINMM_INCLUDE "${WINMM_GENERATED_DIR}/winmm_exports.inc")

file(MAKE_DIRECTORY "${WINMM_GENERATED_DIR}")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/generate_winmm_proxy.py"
        --exports "${CMAKE_SOURCE_DIR}/cmake/winmm_exports.txt"
        --definition "${WINMM_DEFINITION}"
        --include "${WINMM_INCLUDE}"
        --assembly "${WINMM_ASSEMBLY}"
        --format "${WINMM_ASSEMBLY_FORMAT}"
    COMMAND_ERROR_IS_FATAL ANY
)

add_library(pw_ultrawide SHARED
    src/pw_ultrawide.cpp
    src/winmm_proxy.cpp
    "${WINMM_ASSEMBLY}"
    "${WINMM_DEFINITION}"
)
target_include_directories(pw_ultrawide PRIVATE "${WINMM_GENERATED_DIR}" src)
target_compile_definitions(pw_ultrawide PRIVATE PWUWFIX_VERSION="${PWUWFIX_RELEASE}")
if(PWUWFIX_FORCE_SIGNATURE_FALLBACK)
    target_compile_definitions(pw_ultrawide PRIVATE
        PWUWFIX_FORCE_SIGNATURE_FALLBACK=1)
endif()
set_target_properties(pw_ultrawide PROPERTIES
    OUTPUT_NAME winmm
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
if(MSVC)
    target_compile_options(pw_ultrawide PRIVATE
        $<$<COMPILE_LANGUAGE:C,CXX>:/W4;/permissive->)
endif()

add_executable(pw_direct_launcher WIN32 launcher_wrapper/pw_direct_launcher.cpp)
target_compile_definitions(pw_direct_launcher PRIVATE
    PWUWFIX_VERSION="${PWUWFIX_RELEASE}")
if(NOT MSVC)
    # MinGW's GUI CRT selects WinMain by default. The wrapper deliberately uses
    # wWinMain so launcher paths and arguments remain Unicode-safe.
    target_link_options(pw_direct_launcher PRIVATE -municode)
endif()
set_target_properties(pw_direct_launcher PROPERTIES
    OUTPUT_NAME launcher
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
if(MSVC)
    target_compile_options(pw_direct_launcher PRIVATE /W4 /permissive-)
endif()

if(BUILD_TESTING)
    add_executable(pw_math_test tests/pw_math_test.cpp)
    target_include_directories(pw_math_test PRIVATE src)
    add_test(NAME pw_math COMMAND pw_math_test)

    add_executable(pw_signature_test tests/pw_signature_test.cpp)
    target_include_directories(pw_signature_test PRIVATE src)
    add_test(NAME pw_signatures COMMAND pw_signature_test)

    add_executable(pw_launcher_args_probe tests/launcher_args_probe.cpp)
    set_target_properties(pw_launcher_args_probe PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()
