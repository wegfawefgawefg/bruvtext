cmake_minimum_required(VERSION 3.27)

project(bruvtext VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)
include(FindPkgConfig)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)
find_package(Freetype REQUIRED)
pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)
find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)
find_package(SDL3 CONFIG QUIET)

if (NOT SDL3_FOUND)
    set(BRUVTEXT_LOCAL_SDL3_CONFIG
        "/home/vega/Coding/Graphics/hardware-renderer-cpp/build/debug/_deps/sdl3-build"
    )
    if (EXISTS "${BRUVTEXT_LOCAL_SDL3_CONFIG}/SDL3Config.cmake")
        list(PREPEND CMAKE_PREFIX_PATH "${BRUVTEXT_LOCAL_SDL3_CONFIG}")
        find_package(SDL3 CONFIG QUIET)
    endif()
endif()

if (NOT SDL3_FOUND)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.10
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(SDL3)
endif()

add_library(bruvtext STATIC
    src/atlas_cache.cpp
    src/bruvtext.cpp
    src/context.cpp
    src/draw_list.cpp
    src/frame_state.cpp
    src/font_registry.cpp
    src/shape_cache.cpp
    src/shaping.cpp
)

target_include_directories(bruvtext
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${FREETYPE_INCLUDE_DIRS}
        ${HARFBUZZ_INCLUDE_DIRS}
)

target_compile_features(bruvtext PUBLIC cxx_std_20)

if (MSVC)
    target_compile_options(bruvtext PRIVATE /W4 /permissive-)
else()
    target_compile_options(bruvtext PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(bruvtext
    PUBLIC
        Freetype::Freetype
        ${HARFBUZZ_LIBRARIES}
)

add_executable(bruvtext-demo
    demo/main.cpp
    demo/vulkan_demo.cpp
)

set(BRUVTEXT_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/demo/shaders")
set(BRUVTEXT_TEXT_VERT_SPV "${BRUVTEXT_SHADER_OUTPUT_DIR}/text.vert.spv")
set(BRUVTEXT_TEXT_FRAG_SPV "${BRUVTEXT_SHADER_OUTPUT_DIR}/text.frag.spv")

add_custom_command(
    OUTPUT ${BRUVTEXT_TEXT_VERT_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BRUVTEXT_SHADER_OUTPUT_DIR}"
    COMMAND ${GLSLANG_VALIDATOR} -V -S vert -o ${BRUVTEXT_TEXT_VERT_SPV}
            ${CMAKE_CURRENT_SOURCE_DIR}/demo/shaders/text.vert
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/demo/shaders/text.vert
)

add_custom_command(
    OUTPUT ${BRUVTEXT_TEXT_FRAG_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BRUVTEXT_SHADER_OUTPUT_DIR}"
    COMMAND ${GLSLANG_VALIDATOR} -V -S frag -o ${BRUVTEXT_TEXT_FRAG_SPV}
            ${CMAKE_CURRENT_SOURCE_DIR}/demo/shaders/text.frag
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/demo/shaders/text.frag
)

add_custom_target(bruvtext-demo-shaders
    DEPENDS
        ${BRUVTEXT_TEXT_VERT_SPV}
        ${BRUVTEXT_TEXT_FRAG_SPV}
)

add_dependencies(bruvtext-demo bruvtext-demo-shaders)

target_link_libraries(bruvtext-demo
    PRIVATE
        bruvtext
        SDL3::SDL3
        Vulkan::Vulkan
)

target_compile_definitions(bruvtext-demo
    PRIVATE
        BRUVTEXT_ASSETS_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/assets"
        BRUVTEXT_SHADER_DIR="${BRUVTEXT_SHADER_OUTPUT_DIR}"
)

set_target_properties(bruvtext-demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/demo
)
