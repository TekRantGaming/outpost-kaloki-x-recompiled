# outpost_kaloki_x - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(outpost_kaloki_x VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(OUTPOSTKALOKIX_SOURCES
    src/main.cpp
    src/fpe_guard.cpp
    src/frame_stats.cpp
    src/sampler.cpp
    src/settings.cpp
    src/input_remap.cpp
    src/launcher.cpp
    src/platform.cpp
    src/stfs.cpp
    src/art.cpp
    src/overlay.cpp
    src/toast.cpp
    src/updater.cpp
)

if(WIN32)
    add_executable(outpost_kaloki_x WIN32 ${OUTPOSTKALOKIX_SOURCES})
else()
    add_executable(outpost_kaloki_x ${OUTPOSTKALOKIX_SOURCES})
endif()

rexglue_setup_target(outpost_kaloki_x GPU_PLUGINS xenos)

# The port's version (shown in the launcher, compared with GitHub releases).
target_compile_definitions(outpost_kaloki_x PRIVATE OKX_VERSION="${PROJECT_VERSION}")
if(WIN32)
    target_link_libraries(outpost_kaloki_x PRIVATE winhttp)
endif()
