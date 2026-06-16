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

# --- tinyobjloader -----------------------------------------------------------
# A tiny OBJ parser (used from Chunk 9 to load a mesh from disk). Its CMake builds
# a small static library target `tinyobjloader` that compiles the implementation,
# so our code only includes the header. OBJ is a readable text format, which keeps
# the asset-loading chunk transparent — see docs/DECISIONS.md for why we use OBJ
# (and not FBX) here. See Glossary: OBJ_FORMAT
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG v2.0.0rc13   # pinned for reproducible builds
    GIT_SHALLOW TRUE
)

# --- stb_image ---------------------------------------------------------------
# A single-header image loader (used from Chunk 13 to read the diffuse texture).
# The stb repo has no CMake target — it is just headers — so FetchContent only
# downloads it and we add its directory to the include path below.
# See Glossary: TEXTURE
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master   # stb publishes no release tags; master is the canonical source
    GIT_SHALLOW TRUE
)

# Download + configure the dependencies. SDL3 and tinyobjloader are built from
# source; glm is an interface (header-only) target with nothing to compile; stb is
# header-only and just populated (no target).
FetchContent_MakeAvailable(SDL3 glm tinyobjloader stb)

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
    src/mesh.cpp
    src/uniform_buffer.cpp
    src/camera.cpp
)

# Let the sources include each other by bare filename ("sdl_context.h"), and make
# the header-only stb_image.h findable as <stb_image.h>.
target_include_directories(vulkan-orbit-viewer PRIVATE src ${stb_SOURCE_DIR})

# SDL3::SDL3 pulls in the correct platform libraries transparently; glm::glm is
# a header-only interface target that just adds GLM's include path;
# Vulkan::Vulkan adds the Vulkan headers and links the loader.
target_link_libraries(vulkan-orbit-viewer PRIVATE SDL3::SDL3 glm::glm Vulkan::Vulkan tinyobjloader)

# GLM defaults to OpenGL conventions; two must change for Vulkan (used from Chunk
# 8's MVP maths). DEPTH_ZERO_TO_ONE makes projection matrices map depth to Vulkan's
# [0, 1] clip range (OpenGL uses [-1, 1]); FORCE_RADIANS makes all angle arguments
# radians. Set globally so every translation unit that includes GLM agrees.
# See Glossary: PROJECTION_MATRIX, CLIP_SPACE
target_compile_definitions(vulkan-orbit-viewer PRIVATE
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_RADIANS
    # Chunk 9 hashes glm::vec types for vertex deduplication via glm/gtx/hash.hpp,
    # which lives in GLM's "experimental" extensions and refuses to compile without
    # this opt-in. See Glossary: VERTEX_DEDUPLICATION
    GLM_ENABLE_EXPERIMENTAL)

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

# Likewise bake in the assets directory so Chunk 9's OBJ model loads regardless of
# the working directory the program is launched from. See Glossary: ASSET_PIPELINE
target_compile_definitions(vulkan-orbit-viewer PRIVATE ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
