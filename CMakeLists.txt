# Vulkan Orbit Viewer — build configuration
#
# This project is built as a lightweight computer-graphics course: each chunk
# adds one system and stays readable top-to-bottom. Chunk 1 only needs a window,
# so only SDL3 (windowing + input) and GLM (maths, first used in Chunk 8) are
# fetched here. Vulkan itself is wired in from Chunk 2.

# CMake 3.21 gives us a stable FetchContent and modern target-based linking.
cmake_minimum_required(VERSION 3.21)

project(vulkan-orbit-viewer
    LANGUAGES CXX
    DESCRIPTION "A lightweight Vulkan computer-graphics course")

# C++17 covers everything this project needs and is supported by every toolchain
# we target: GCC, Clang, MSVC, and Apple Clang.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# FetchContent downloads and builds dependencies as part of our own configure
# step, so a fresh checkout builds with nothing pre-installed except the Vulkan
# SDK (needed from Chunk 2 onward).
include(FetchContent)

# --- Vulkan SDK --------------------------------------------------------------
# Unlike SDL3 and GLM, Vulkan is NOT fetched: the loader, headers, and (on
# macOS) MoltenVK come from the Vulkan SDK installed on the host machine. This
# is the standard way to do Vulkan development. find_package locates the loader
# library and headers; we link against the Vulkan::Vulkan target it defines.
# See Glossary: VULKAN_INSTANCE
find_package(Vulkan REQUIRED)

# --- SDL3 --------------------------------------------------------------------
# SDL3 is the windowing + input layer. It hides the platform window system
# (X11/Wayland on Linux, Cocoa on macOS, Win32 on Windows) behind one API and,
# from Chunk 2, bridges that window to a Vulkan surface.
# See Glossary: SDL3, WINDOWING_SYSTEM
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.0   # first stable SDL3 release — pinned for reproducible builds
    GIT_SHALLOW TRUE
)

# --- GLM ---------------------------------------------------------------------
# GLM is a header-only linear-algebra library modelled on GLSL's vector/matrix
# types. It is not used until Chunk 8 (the MVP matrix chain), but we fetch it
# now so the dependency graph is fixed from the start.
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
)

# Download + configure both dependencies. SDL3 is built from source; glm is an
# interface (header-only) target with nothing to compile.
FetchContent_MakeAvailable(SDL3 glm)

# --- The executable ----------------------------------------------------------
# main.cpp is the narrative entry point; each other .cpp is a self-contained
# "chapter". Chunk 1 has just the SDL window chapter.
add_executable(vulkan-orbit-viewer
    src/main.cpp
    src/sdl_context.cpp
    src/vulkan_context.cpp
    src/swapchain.cpp
    src/render_pass.cpp
    src/shader_pipeline.cpp
    src/renderer.cpp
)

# Let the sources include each other by bare filename ("sdl_context.h").
target_include_directories(vulkan-orbit-viewer PRIVATE src)

# SDL3::SDL3 pulls in the correct platform libraries transparently; glm::glm is
# a header-only interface target that just adds GLM's include path;
# Vulkan::Vulkan adds the Vulkan headers and links the loader.
target_link_libraries(vulkan-orbit-viewer PRIVATE SDL3::SDL3 glm::glm Vulkan::Vulkan)

# --- Shader compilation ------------------------------------------------------
# GLSL shaders are compiled to SPIR-V at build time with glslc (shipped in the
# Vulkan SDK; FindVulkan exposes it as the Vulkan::glslc target). Shader
# compilation is part of the build, never a manual step.
# See Glossary: SPIR_V, GLSL
set(SHADER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_OUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders/compiled)
file(MAKE_DIRECTORY ${SHADER_OUT_DIR})

set(SHADER_SOURCES mesh.vert mesh.frag)
set(SHADER_SPV_FILES)
foreach(shader ${SHADER_SOURCES})
    set(spv ${SHADER_OUT_DIR}/${shader}.spv)
    add_custom_command(
        OUTPUT ${spv}
        COMMAND Vulkan::glslc ${SHADER_SRC_DIR}/${shader} -o ${spv}
        DEPENDS ${SHADER_SRC_DIR}/${shader}
        COMMENT "Compiling ${shader} -> SPIR-V"
        VERBATIM)
    list(APPEND SHADER_SPV_FILES ${spv})
endforeach()

# A target that builds all SPIR-V, made a dependency of the executable so the
# shaders are always up to date before we run.
add_custom_target(shaders DEPENDS ${SHADER_SPV_FILES})
add_dependencies(vulkan-orbit-viewer shaders)

# Bake the compiled-shader directory in so the program finds its SPIR-V from any
# working directory (simple and reliable for this local course build).
target_compile_definitions(vulkan-orbit-viewer PRIVATE SHADER_DIR="${SHADER_OUT_DIR}")
