cmake_minimum_required(VERSION 3.16)
project(PowerGridSim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SOURCES
    src/globals.cpp
    src/graph_core.cpp
    src/graph_layout.cpp
    src/graph_simulation.cpp
    src/graph_overload.cpp
    src/renderer.cpp
    src/main.cpp
)

add_executable(PowerGridSim ${SOURCES})
target_include_directories(PowerGridSim PRIVATE include)

# ── Raylib ──────────────────────────────────────────────────────────────────
# Option A: installed via package manager / vcpkg
find_package(raylib QUIET)
if (raylib_FOUND)
    target_link_libraries(PowerGridSim PRIVATE raylib)
else()
    # Option B: auto-fetch (requires internet on first configure)
    include(FetchContent)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG        5.0
    )
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(raylib)
    target_link_libraries(PowerGridSim PRIVATE raylib)
endif()

# ── Warnings ─────────────────────────────────────────────────────────────────
if (MSVC)
    target_compile_options(PowerGridSim PRIVATE /W4)
else()
    target_compile_options(PowerGridSim PRIVATE -Wall -Wextra -Wpedantic)
endif()
