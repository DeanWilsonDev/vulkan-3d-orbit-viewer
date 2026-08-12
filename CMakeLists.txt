cmake_minimum_required(VERSION 3.21)

project(
  vulkan-orbit-viewer
  LANGUAGES CXX
  DESCRIPTION "A lightweight Vulkan 3D renderer")

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# Vulkan SDK
find_package(Vulkan REQUIRED)

# SDL3
FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.2.0
  GIT_SHALLOW TRUE)

# GLM
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.1
  GIT_SHALLOW TRUE)

# tinyobjloader
FetchContent_Declare(
  tinyobjloader
  GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
  GIT_TAG v2.0.0rc13 # pinned for reproducible builds
  GIT_SHALLOW TRUE)

# --- stb_image ---------------------------------------------------------------
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG master # stb publishes no release tags; master is the canonical source
  GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(SDL3 glm tinyobjloader stb)

add_executable(vulkan-orbit-viewer src/main.cpp src/sdl-context.cpp
                                   src/vulkan-context.cpp)

target_include_directories(vulkan-orbit-viewer PRIVATE src ${stb_SOURCE_DIR})

target_link_libraries(
  vulkan-orbit-viewer PRIVATE SDL3::SDL3 glm::glm
  Vulkan::Vulkan tinyobjloader)
