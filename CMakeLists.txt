cmake_minimum_required(VERSION 3.16)

project(
        DoomClone
        VERSION 0.1.0
        LANGUAGES CXX
)

# ---- C++ standard ----
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Build type default ----
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif ()

# ---- Compiler warnings ----
add_library(project_warnings INTERFACE)

target_compile_options(project_warnings INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# ---- Main executable ----
file(GLOB_RECURSE SOURCES "src/*.cpp")

find_package(glm REQUIRED)
find_package(SDL2 REQUIRED)
find_package(fmt CONFIG REQUIRED)

add_executable(${PROJECT_NAME}
        ${SOURCES}
        src/renderer/editor/commands.cpp
        src/bsp/bsp.cpp
        include/bsp.h
        include/serialization.h
        src/core/serialization.cpp
        src/renderer/editor/editor_serialization.cpp
        src/bsp/bsp_visualization.cpp
        include/utils.h
)

target_include_directories(${PROJECT_NAME}
        PRIVATE
        "${PROJECT_SOURCE_DIR}/include"
)

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        project_warnings
        glm::glm
        SDL2::SDL2
        SDL2::SDL2main
        fmt::fmt
)

# ---- Install rules ----
install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
)
