# Vulkan Orbit Viewer

A small, readable Vulkan application that loads a 3D model from an OBJ file and lets
you orbit, pan, and zoom around it with the mouse, lit by a single directional light.

It is built as a **lightweight computer-graphics course**: the code is meant to be
read top to bottom, each source file is a self-contained "chapter", and every Vulkan
or graphics concept it uses is explained in [`Glossary.md`](Glossary.md). The
chunk-by-chunk specification lives in [`docs/SPEC.md`](docs/SPEC.md) and the running
log of design decisions in [`docs/DECISIONS.md`](docs/DECISIONS.md).

## What it does

- Opens a resizable window and brings up Vulkan (instance, device, swapchain, render
  pass, graphics pipeline, command buffers with two frames in flight).
- Loads a mesh from a Wavefront **OBJ** file, deduplicating its vertices into GPU
  vertex/index buffers.
- Renders it in perspective with an **orbit camera** and **per-fragment diffuse +
  ambient lighting**.

## Controls

| Input                | Action                          |
| -------------------- | ------------------------------- |
| **Left-drag**        | Orbit the camera around the model |
| **Right-drag**       | Pan (slide the focal point)     |
| **Scroll wheel**     | Zoom in / out                   |
| Resize window        | Handled live; the view re-fits  |
| Close window / Cmd+Q | Quit                            |

The light is fixed in world space, so orbiting moves the camera, not the light.

## Prerequisites

- A C++17 compiler (Apple Clang, GCC, MSVC).
- [CMake](https://cmake.org/) 3.21 or newer.
- The [**Vulkan SDK**](https://vulkan.lunarg.com/) installed on the host. This is the
  one dependency that is **not** fetched automatically — it provides the Vulkan
  loader, headers, validation layers, and the `glslc` shader compiler. On macOS the
  SDK ships MoltenVK, which runs Vulkan on top of Metal.

Everything else — **SDL3** (windowing/input), **GLM** (maths), and
**tinyobjloader** (OBJ parsing) — is downloaded and built automatically by CMake via
`FetchContent` on the first configure, so a fresh checkout needs nothing pre-installed
beyond the Vulkan SDK.

## Building

The build is the same on every platform; only how you install the Vulkan SDK differs.

```sh
cmake -S . -B build
cmake --build build
```

The first configure is slow because SDL3 and tinyobjloader are compiled from source.
Shaders are compiled to SPIR-V automatically as part of the build (no manual step).

### macOS

Install the Vulkan SDK from the LunarG installer (it sets things up under
`~/VulkanSDK/<version>`). `find_package(Vulkan)` locates the loader and headers
without any environment variable needing to be set.

### Linux

Install the Vulkan SDK (or your distro's `vulkan-loader`, `vulkan-validationlayers`,
`vulkan-headers`, and `glslc`/`shaderc` packages). Ensure a Vulkan-capable driver is
present. CMake's `find_package(Vulkan)` then resolves them.

### Windows

Install the Vulkan SDK from LunarG (it sets the `VULKAN_SDK` environment variable).
Configure with your preferred generator, e.g.:

```sh
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Debug
```

> Validation layers are enabled automatically in debug builds and disabled in release
> builds (`NDEBUG`). Validation messages, if any, print to `stderr`.

## Running

```sh
# Load the bundled sample model
./build/vulkan-orbit-viewer

# Or load a specific OBJ
./build/vulkan-orbit-viewer assets/torus.obj
./build/vulkan-orbit-viewer path/to/your/model.obj
```

With no argument the bundled `assets/mesh.obj` is loaded. The camera automatically
frames whatever model it is given, regardless of that model's scale or origin.

Sample models in [`assets/`](assets/):

- `mesh.obj` — a low-poly handheld console.
- `torus.obj` — a procedurally generated torus.

OBJ models should be triang(or quad) meshes; quads and n-gons are triangulated on
load. If a model has no normals, flat per-face normals are generated so it is still
shaded.

## Project layout

| Path                  | What it is                                             |
| --------------------- | ------------------------------------------------------ |
| `src/`                | The application, one "chapter" per file (start in `main.cpp`) |
| `shaders/`            | GLSL source; compiled to `shaders/compiled/` at build time |
| `assets/`             | Sample models                                          |
| `Glossary.md`         | Every graphics/Vulkan concept the code uses, explained |
| `docs/SPEC.md`        | The chunk-by-chunk build specification                 |
| `docs/DECISIONS.md`   | The running log of design decisions and deviations     |
