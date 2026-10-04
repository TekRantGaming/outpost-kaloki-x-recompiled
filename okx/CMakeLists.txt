# outpost_kaloki_x - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(outpost_kaloki_x LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(OUTPOSTKALOKIX_SOURCES
    src/main.cpp
    src/fpe_guard.cpp
    src/frame_stats.cpp
    src/sampler.cpp
)

if(WIN32)
    add_executable(outpost_kaloki_x WIN32 ${OUTPOSTKALOKIX_SOURCES})
else()
    add_executable(outpost_kaloki_x ${OUTPOSTKALOKIX_SOURCES})
endif()

rexglue_setup_target(outpost_kaloki_x GPU_PLUGINS xenos)
