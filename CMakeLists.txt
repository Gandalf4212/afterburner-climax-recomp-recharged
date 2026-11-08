# After Burner Climax Recomp (Recharged) - ReXGlue recompiled project
#
# One executable, recompiled from the World release (default.xex 0.0.1.2).
# The SDK is built from source with this repo's patches (patches/rexglue-sdk):
#   cmake -S . -B out -G Ninja -DREXSDK_DIR=<patched rexglue-sdk tree> ...
# See docs/building.md.

cmake_minimum_required(VERSION 3.25)
project(after_burner_climax LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT REXSDK_DIR)
    message(FATAL_ERROR "after_burner_climax needs REXSDK_DIR: the ReXGlue SDK v0.10.1 source tree with "
                        "patches/rexglue-sdk (git am) and patches/ffmpeg (in thirdparty/FFmpeg) applied. "
                        "See docs/building.md.")
endif()

# The recompiled code in generated/default is committed, so the game builds
# without default.xex (e.g. on GitHub Actions). generated/rexglue.cmake always
# adds a step that re-runs codegen, which needs the xex; when it isn't there we
# include a copy of that file with the step removed and use the committed code.
# (Same approach as mad-riders-recomp.)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/default.xex")
    set(_abc_default_skip OFF)
else()
    set(_abc_default_skip ON)
endif()
option(ABC_SKIP_CODEGEN "Build from the committed generated code without re-running codegen"
       ${_abc_default_skip})

if(ABC_SKIP_CODEGEN)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/generated/rexglue.cmake" _abc_rexglue)
    string(FIND "${_abc_rexglue}" "add_custom_command(" _abc_start)
    string(FIND "${_abc_rexglue}" "add_custom_target(after_burner_climax_codegen" _abc_target)
    if(_abc_start EQUAL -1 OR _abc_target EQUAL -1)
        message(FATAL_ERROR "generated/rexglue.cmake changed layout; update the ABC_SKIP_CODEGEN block")
    endif()
    string(SUBSTRING "${_abc_rexglue}" 0 ${_abc_start} _abc_head)
    string(SUBSTRING "${_abc_rexglue}" ${_abc_target} -1 _abc_tail)
    # Drop the codegen target's dependency on the stamp the removed command made.
    string(REGEX REPLACE "add_custom_target\\(after_burner_climax_codegen[^)]*\\)"
           "add_custom_target(after_burner_climax_codegen)" _abc_tail "${_abc_tail}")
    # Paths in the file are relative to the project root.
    string(REPLACE "include(generated/" "include(\${CMAKE_CURRENT_SOURCE_DIR}/generated/"
           _abc_out "${_abc_head}${_abc_tail}")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/rexglue_prebuilt.cmake" "${_abc_out}")
    message(STATUS "after_burner_climax: using committed generated code (codegen step skipped)")
    include("${CMAKE_CURRENT_BINARY_DIR}/rexglue_prebuilt.cmake")
else()
    include(generated/rexglue.cmake)
endif()

set(ABC_SOURCES
    src/main.cpp
    src/game_menu.cpp
    src/package_setup.cpp
    src/perf_overlay.cpp
    src/frame_rate.cpp
)

if(WIN32)
    add_executable(after_burner_climax WIN32 ${ABC_SOURCES})
else()
    add_executable(after_burner_climax ${ABC_SOURCES})
endif()

rexglue_setup_target(after_burner_climax GPU_PLUGINS xenos)

# Built from source, the SDK does not pass its ImGui headers on (the menu includes them).
target_include_directories(after_burner_climax PRIVATE "${REXSDK_DIR}/thirdparty/imgui")

# NativeXRenderer (docs/native-x-renderer.md): built in, used when recomp_renderer = "nativex" (F1 > Renderer).
# The SDK's Xenos plugin keeps running the command processor it observes; with Xenia selected it draws as before.
add_subdirectory(thirdparty/native_x_renderer)
target_sources(after_burner_climax PRIVATE src/native_x_renderer_abc.cpp src/shader_scan.cpp)
# The shader scan inflates shader_hlsl.farc with stb_image's zlib decoder (internal linkage).
target_include_directories(after_burner_climax PRIVATE "${REXSDK_DIR}/thirdparty/stb")
target_link_libraries(after_burner_climax PRIVATE native_x_renderer)
target_compile_definitions(after_burner_climax PRIVATE ABC_NATIVE_X_RENDERER=1)
native_x_renderer_copy_runtime(after_burner_climax)

# Debug info (CodeView, shipped as after_burner_climax.pdb) for our code and NativeXRenderer only, so a crash report
# (src/crash_handler.h) names functions and lines; the generated game code stays without it (size, link time).
if(WIN32)
    set_source_files_properties(src/main.cpp src/game_menu.cpp src/package_setup.cpp src/native_x_renderer_abc.cpp src/perf_overlay.cpp src/frame_rate.cpp
        src/shader_scan.cpp PROPERTIES COMPILE_OPTIONS "-g;-gcodeview")
    foreach(_nxr_target native_x_renderer native_x_renderer_sdk)
        if(TARGET ${_nxr_target})
            target_compile_options(${_nxr_target} PRIVATE -g -gcodeview)
        endif()
    endforeach()
    target_link_options(after_burner_climax PRIVATE "LINKER:/DEBUG:FULL" "LINKER:/OPT:REF" "LINKER:/OPT:ICF")
endif()

# CI passes the commit so logs say which build produced them.
set(ABC_BUILD "" CACHE STRING "Build identifier shown in the log")
if(ABC_BUILD)
    target_compile_definitions(after_burner_climax PRIVATE ABC_BUILD="${ABC_BUILD}")
endif()
