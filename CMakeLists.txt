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

set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# ---- Build type default ----
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif ()

# ---- Main executable ----
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")

find_package(SDL2 REQUIRED)

add_executable(${PROJECT_NAME}
        ${SOURCES}
)

target_include_directories(${PROJECT_NAME}
        PRIVATE
        "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        SDL2::SDL2
        SDL2::SDL2main
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        PROJECT_ROOT_PATH="${CMAKE_SOURCE_DIR}/"
        $<$<CONFIG:Debug>:DOOM_DEBUG>
)

# ---- Install rules ----
install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
)
