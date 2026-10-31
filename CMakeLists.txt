# after_burner_climax - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(after_burner_climax LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(AFTERBURNERCLIMAX_SOURCES
    src/main.cpp
    src/game_menu.cpp
    src/package_setup.cpp
)

if(WIN32)
    add_executable(after_burner_climax WIN32 ${AFTERBURNERCLIMAX_SOURCES})
else()
    add_executable(after_burner_climax ${AFTERBURNERCLIMAX_SOURCES})
endif()

rexglue_setup_target(after_burner_climax GPU_PLUGINS xenos)

# Second build for the World release (the retail XBLA package as found on
# archive.org; default.xex 0.0.1.2). Its code is at different addresses, so it
# is recompiled separately (world/generated). Both exes ship together; each
# checks the player's default.xex and hands off to the other if it matches.
include(world/generated/default/sources.cmake)
set(ABC_WORLD_GENERATED_SOURCES ${GENERATED_SOURCES})
if(WIN32)
    add_executable(after_burner_climax_world WIN32 ${AFTERBURNERCLIMAX_SOURCES})
else()
    add_executable(after_burner_climax_world ${AFTERBURNERCLIMAX_SOURCES})
endif()
target_sources(after_burner_climax_world PRIVATE ${ABC_WORLD_GENERATED_SOURCES})
# world/ first, so "generated/default/after_burner_climax_init.h" resolves to the World code.
target_include_directories(after_burner_climax_world BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/world
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/world/generated/default
)
target_compile_definitions(after_burner_climax_world PRIVATE ABC_WORLD=1)
target_link_libraries(after_burner_climax_world PRIVATE rex::runtime)
rexglue_configure_target(after_burner_climax_world GPU_PLUGINS xenos)
