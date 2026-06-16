# Implementation Decision Log

This document records the decisions made while building the Vulkan Orbit Viewer,
chunk by chunk. It complements `SPEC.md` (which sets the high-level direction)
and `Glossary.md` (which explains the concepts). The focus here is the *why*
behind concrete implementation choices — especially ones not spelled out in the
spec, ones forced by the host platform, and any deviations from the spec.

Decisions inherited directly from `SPEC.md` ("Confirmed Decisions" table —
C++, SDL3, Vulkan, MoltenVK, GLM, OBJ, layered files) are not repeated here
unless the implementation added nuance to them.

Status as of writing: Chunks 1–7 complete.

______________________________________________________________________

## Cross-cutting decisions

### Build environment

- **CMake minimum 3.21.** Gives stable `FetchContent` and modern target-based
  linking. The host has CMake 4.x; the only friction was deprecation warnings
  emitted by GLM's own `cmake_minimum_required(VERSION 2.x)`, which are
  cosmetic and not ours to fix.
- **C++17.** Sufficient for everything the project does (`std::optional`,
  structured code) and supported by every target toolchain (GCC, Clang, MSVC,
  Apple Clang). Nothing yet needs C++20.
- **Dependencies fetched vs. host-provided.** SDL3 and GLM are pulled with
  `FetchContent` and pinned (`SDL3` → `release-3.2.0`, the first stable SDL3
  release; `GLM` → `1.0.1`) for reproducible builds. The **Vulkan SDK is
  host-provided** via `find_package(Vulkan)` — this is the standard for Vulkan
  development and the only thing a fresh checkout needs pre-installed.

### Host platform (the machine this was verified on)

- Apple M2 Pro, macOS (Darwin 25.x), Vulkan via **MoltenVK** (apiVersion 1.4.x).
- Because the GPU is reached through MoltenVK (a *portability* driver), several
  macOS-specific concessions were required — see the Chunk 2 entries below.
- The build links the Vulkan loader from `/usr/local/lib`; `VULKAN_SDK` is not
  set in the environment but `find_package(Vulkan)` locates it regardless.

### Code & documentation conventions

- **One concept owner per file / chapter.** `main.cpp` reads as a narrative; each
  subsystem is a self-contained class (`SdlContext`, `VulkanContext`,
  `Swapchain`, `RenderPass`). Matches the spec's "layered files" decision.
- **RAII for every GPU/OS resource.** Each owner creates in its constructor and
  destroys in the reverse order in its destructor; all owners are non-copyable.
  This makes Vulkan's strict "children before parents" teardown automatic via
  C++ destruction order.
- **Comment convention** follows the spec: what + why + `See Glossary: TERM`.
  An audit (grep of `See Glossary:` references vs. glossary headers) is run each
  chunk to ensure every reference resolves.
- **Glossary partitioning.** Entries are grouped by the chunk that introduced
  them, but the glossary is treated as broader than the code: a concept gets an
  entry even if it only appears in a comment (e.g. `MOLTENVK`, `PAINTERS_ALGORITHM`).
  To keep the per-chunk audit clean, comments do **not** reference a glossary
  term before its entry exists — forward concepts are described in prose with a
  "covered in Chunk N" note instead.

### Verification approach

- The app is an infinite event loop with no headless mode, so each chunk is
  verified by: build clean (only SDL's own Cocoa-backend warnings allowed), then
  run in the background for ~2s, confirm the expected stdout and an **empty
  stderr** (validation layers print there), then terminate.
- Behaviours that need a real event (resize, deliberate validation errors) are
  verified with a **temporary** code probe that is run and then reverted, never
  committed. Examples used: forcing `SDL_SetWindowSize` to confirm swapchain /
  framebuffer recreation; skipping a `vkDestroy*` to confirm the validation
  messenger reports leaks.

______________________________________________________________________

## Chunk 1 — Scaffold and window

- **`SdlContext` owns the window and the event queue, nothing else.** Windowing
  is an OS concern; rendering is a Vulkan concern. Keeping them apart means the
  Vulkan code never learns which platform it runs on.
- **Poll, don't block, on events** (`SDL_PollEvent`). A real-time renderer must
  keep producing frames even with no input, so the loop takes pending events and
  moves on.
- **`.gitignore` added** for `build/` and the future `shaders/compiled/`. The
  project is not yet a git repository; this is hygiene for when it becomes one.
- **Window starts at 1280×720**, resizable. Arbitrary sensible default.

## Chunk 2 — Vulkan instance and device

- **Validation layers gated on `NDEBUG`** via a `constexpr kEnableValidation`.
  On in debug, off in release. If the layer is requested but not installed, the
  code degrades gracefully (warns, continues) rather than failing.
- **Debug messenger chained onto instance creation** (via `pNext`) *and*
  installed as a persistent messenger. The chained copy covers
  `vkCreateInstance`/`vkDestroyInstance` themselves, which the persistent one
  cannot.
- **macOS portability handling** (required by MoltenVK): enable
  `VK_KHR_portability_enumeration` + the `ENUMERATE_PORTABILITY` instance flag
  and `VK_KHR_get_physical_device_properties2` at instance creation;
  conditionally enable `VK_KHR_portability_subset` at device creation. All gated
  behind `#ifdef __APPLE__`.
- **Physical device selection is first-fit**, not scored. The selection criteria
  are: has a graphics queue family, can present to the surface, and supports the
  required device extensions. A production engine would prefer a discrete GPU;
  for a single-mesh viewer first-fit is enough.
- **`VK_KHR_swapchain` enabled at device creation in Chunk 2**, one chunk before
  the swapchain itself. The logical device is created once, so its extensions are
  committed here; enabling it early avoids recreating the device in Chunk 3.
- **API version targeted: Vulkan 1.3** (`VK_API_VERSION_1_3`). Comfortably
  supported by MoltenVK on this machine and broadly available.
- **Deviation / gotcha — device-level layers left at zero.** An initial pass set
  `enabledLayerCount` on the *device* "for old loaders". Modern validation layers
  now flag a non-zero device-level layer count as an **error** (device layers
  have been a no-op since Vulkan 1.0). The instance-level layer already covers
  all device calls, so the device layer fields are intentionally left unset. This
  was caught by the validation messenger and removed.

## Chunk 3 — Swapchain

- **Surface format preference: `B8G8R8A8_SRGB` + `SRGB_NONLINEAR`**, falling back
  to the surface's first listed format. sRGB matches how displays emit light, so
  colours are correct without manual gamma handling.
- **Present mode preference: MAILBOX, fallback FIFO.** FIFO is the only mode the
  spec guarantees. On this MoltenVK setup MAILBOX is not offered, so FIFO (classic
  v-sync) is used — confirmed in the startup log.
- **Image count: `minImageCount + 1`.** Resolves to 3 on this surface (triple
  buffering) so the app always has an image to draw into while another is shown.
- **Extent uses drawable pixels** (`SDL_GetWindowSizeInPixels`), clamped to the
  surface's allowed range, for correctness on HiDPI/Retina displays where pixels
  and logical points differ. (The window does not set
  `SDL_WINDOW_HIGH_PIXEL_DENSITY`, so on this machine they currently coincide.)
- **Resize handling lives in the main loop.** `SdlContext` records
  `WINDOW_PIXEL_SIZE_CHANGED` and exposes a read-and-clear `takeResized()`; the
  loop calls `swapchain.recreate()`. `recreate()` is a simple
  wait-idle → destroy → create (no `oldSwapchain` hand-off), which is the
  clearest form for a viewer that does not yet render during resize.
- **`VulkanContext::waitIdle()` added** (inline). The heavy device-wide stall is
  acceptable on the rare resize event; Chunk 6 introduces per-frame
  synchronisation for the common case.
- **Known cosmetic behaviour:** macOS fires an initial `PIXEL_SIZE_CHANGED` at
  startup, so the swapchain logs "created" twice on every launch. It is benign
  (same size, no validation errors) and conveniently exercises `recreate()` each
  run, so it was left as-is rather than special-cased.

## Chunk 4 — Render pass and framebuffers

- **Scope decision (spec contradiction resolved): render-pass objects only, no
  present loop.** Chunk 4's checkpoint describes a *visible* cleared frame ("the
  first frame actually presented to screen"), but presenting requires command
  buffers and synchronisation, which the spec assigns to **Chunk 6**. Asked the
  user; chose to build exactly Chunk 4's stated scope (render pass, depth buffer,
  framebuffers, clear colour) and verify by **clean creation with zero validation
  errors** instead of a visible frame. The on-screen clear colour is therefore
  deferred to Chunk 6, where the command-buffer/sync concepts properly belong.
  This keeps the chunk boundaries and glossary partitioning clean.
- **`RenderPass` owns more than the render pass** — also the depth image/view and
  the framebuffers. They are grouped because the spec puts them in one file and
  because they share a lifetime relationship with the render pass.
- **Render pass survives resize; targets do not.** The render pass depends only on
  attachment formats, so it is created once. The depth buffer and framebuffers
  depend on the swapchain extent/images, so `recreate()` rebuilds just those.
- **Clear colour: dark blue-grey `(0.02, 0.02, 0.05)` linear.** Visibly not
  black, so a future cleared frame is unmistakably distinct from an
  uncleared/broken one. Depth clears to **1.0** (far plane) to match a
  "keep nearer fragment" depth test.
- **Depth format chosen by capability query** (`chooseDepthFormat`), preferring
  `D32_SFLOAT`, then the combined depth+stencil formats. The view's aspect mask
  adds the stencil bit only when the chosen format carries stencil.
- **Depth attachment `storeOp = DONT_CARE`.** The depth result is never read back
  after the pass, so the GPU is told it may discard it — a real bandwidth saving
  on tile-based GPUs.
- **`findMemoryType` lives locally in `render_pass.cpp` for now.** The depth image
  needs device-local memory, which requires picking a memory type — a Chunk 7
  topic. To avoid pulling Chunk 7's file changes and glossary entries forward, the
  helper is a private static here and the memory-types concept is described in
  prose with a "covered in Chunk 7" note rather than a glossary reference. Chunk 7
  may promote it to `VulkanContext` when buffers need the same logic.
- **Single subpass + one external dependency.** The viewer never needs multiple
  subpasses; the dependency is included as correct practice for the layout
  transition even though nothing is drawn yet.

## Chunk 5 — Shaders and the graphics pipeline

- **Scope decision (reversed from Chunk 4): build the frame loop and show the
  triangle.** Unlike Chunk 4, Chunk 5's "what gets built" explicitly lists
  "a hardcoded triangle rendered using the pipeline" — rendering is in-scope by
  the spec's own words. Asked the user, who chose to bring the minimal
  command-buffer + synchronisation machinery (the spec's Chunk 6 content) forward
  so the triangle is actually visible. This restores the spec's "earliest visible
  output" goal and makes Chunk 5's checkpoint literally verifiable. **Consequence:**
  Chunk 6 becomes "expand to frames-in-flight = 2 + robust sync + barriers/layout
  transitions" rather than introducing command buffers from scratch.
- **New file `renderer.h/.cpp` (deviation from the spec's file list).** The spec
  would put command buffers + sync in `vulkan_context`. A dedicated `Renderer`
  keeps `VulkanContext` focused on instance/device and isolates the per-frame
  orchestration. Documented here as the deviation it is.
- **Single frame in flight in Chunk 5** (one command buffer, one image-available
  semaphore, one fence). The minimum that can present; Chunk 6 raises it to 2.
- **Per-swapchain-image `renderFinished` semaphores**, not per-frame. This is the
  pattern current validation layers expect (a per-frame render-finished semaphore
  can still be in use by the presentation engine when reused). They are rebuilt in
  `onSwapchainRecreated` since the image count could change.
- **Hardcoded triangle via `gl_VertexIndex`, no vertex buffer.** Three positions
  live in `mesh.vert`; vertex buffers arrive in Chunk 7. Cull mode is therefore
  `NONE` (the triangle's winding is not guaranteed); back-face culling waits for
  Chunk 7.
- **Dynamic viewport + scissor.** Left dynamic in the pipeline and set per-frame,
  so the single pipeline survives window resizes without rebuilding.
- **Depth test enabled in the pipeline** (`LESS`, depth write on) against the
  Chunk 4 depth attachment, even though a single triangle does not need it — it
  exercises the full target set and is correct for what follows.
- **Colour: flat orange `(0.90, 0.55, 0.20)`** in `mesh.frag`, chosen to read
  clearly against the dark blue-grey clear colour. Per-fragment lighting is
  Chunk 11.
- **Shader build: `glslc` via `Vulkan::glslc`**, compiling `mesh.vert`/`mesh.frag`
  to `shaders/compiled/*.spv` as a CMake custom target the executable depends on.
  Compilation is never a manual step (matches the spec).
- **Shader location baked in via `SHADER_DIR` compile definition** (absolute path
  to `shaders/compiled`). Simplest reliable way to find SPIR-V regardless of
  working directory for a local course build; a shipped app would instead copy
  shaders next to the executable or embed them.

## Chunk 6 — Command buffers and synchronisation

- **Scope (consequence of the Chunk 5 decision): this chunk *expands* the frame
  loop rather than introducing it.** Because Chunk 5 pulled command buffers + a
  minimal 1-frame sync forward, Chunk 6's real work was: raise to two frames in
  flight, complete the synchronisation, and add the remaining glossary entries
  (`SYNCHRONISATION`, `FRAMES_IN_FLIGHT`, `PIPELINE_BARRIER`,
  `IMAGE_LAYOUT_TRANSITION`).
- **Frames in flight = 2.** Per-frame command buffer, image-available semaphore,
  and in-flight fence are now `std::array`s of size 2; `m_currentFrame` cycles
  `0,1`. This lets the CPU record frame K+1 while the GPU runs frame K.
- **`renderFinished` semaphores stay per swapchain image** (not per frame) — the
  validation-safe pattern retained from Chunk 5. So there are two distinct counts:
  per-frame (2) and per-image (swapchain image count, 3 here).
- **No explicit `vkCmdPipelineBarrier`.** All ordering and image-layout
  transitions this project needs are handled by the render pass's attachment
  layouts + subpass dependency (an implicit barrier). `PIPELINE_BARRIER` and
  `IMAGE_LAYOUT_TRANSITION` are documented as concepts and tied to that code; an
  explicit barrier would only be needed for render-to-texture / compute steps.
- **Changes stayed in `renderer.h/.cpp`** (+ a comment in `render_pass.cpp` and
  `main.cpp`), not `vulkan_context` as the spec's Chunk 6 file list suggested —
  consistent with the Chunk 5 decision to keep the frame loop in `Renderer`.
- **Verified "full frame rate / no unnecessary stalling"** with a temporary FPS
  counter: steady ~119 fps on this 120 Hz (ProMotion) display, i.e. capped at the
  v-synced refresh rate (FIFO), which is the expected best case. Counter reverted.

## Chunk 7 — Vertex and index buffers

- **`Vertex` = position + normal + uv** (in `mesh.h`). Normal is unused until
  Chunk 11 and uv is never sampled (no textures in scope), but both are defined
  now so the buffer layout and attribute descriptions are stable from the start.
- **Buffer helpers added to `VulkanContext`** (`createBuffer`, `copyBuffer`,
  `findMemoryType`) plus a dedicated `m_uploadPool` (TRANSIENT) for one-time
  transfer commands. This matches the spec's "Chunk 7 touches vulkan_context".
- **`findMemoryType` moved** from the local static in `render_pass.cpp` to
  `VulkanContext` (resolving the open item); the depth buffer now calls it too.
- **Staging-buffer pattern** in `Mesh::uploadDeviceLocal`: host-visible staging →
  `memcpy` → device-local destination via `copyBuffer`. Used for both the vertex
  and index buffers. Note: on the M2's unified memory, host-visible and
  device-local may be the same physical RAM, but the pattern is kept because it is
  correct and portable to discrete GPUs.
- **`copyBuffer` submits and `vkQueueWaitIdle`s** — fine because uploads are
  one-off setup, not per-frame work.
- **Cube built in clip space directly** (`Mesh::cube`): centred at z = 0.5 with
  z ∈ [0.2, 0.8] so the whole cube is inside Vulkan's clip volume (NDC z is 0..1),
  x/y ∈ ±0.4. 24 vertices (4 per face) so each face carries its own flat normal;
  36 indices. With no transforms it renders as a centred square (the front face) —
  the spec's "may look like a hexagon" caveat; a straight-on axis-aligned cube is a
  square, which is correct.
- **Back-face culling enabled** (`CULL_MODE_BACK`). Cube wound CCW-from-outside;
  with no viewport Y-flip yet, Vulkan reads that as CW on screen, so
  `frontFace = CLOCKWISE`. **This will need revisiting in Chunk 8**: the standard
  Vulkan projection flips Y (`proj[1][1] *= -1`), which inverts apparent winding,
  so `frontFace` will likely switch to `COUNTER_CLOCKWISE` then. Verified visually
  this chunk via screenshot (solid square, no culled-through gaps).
- **Necessary changes outside the spec's Chunk 7 file list:** the vertex shader
  (`mesh.vert`) now reads the position attribute instead of a hardcoded array, and
  `ShaderPipeline` now wires the vertex input state and enables culling. Both are
  unavoidable to render from a vertex buffer; noted here.
- **Draw path:** `Renderer::recordCommandBuffer` now calls `mesh.bindAndDraw`
  (bind vertex+index buffers, `vkCmdDrawIndexed`) instead of `vkCmdDraw(3,…)`;
  `drawFrame` gained a `const Mesh&` parameter.

______________________________________________________________________

## Chunk 8 — Uniform buffers and MVP transforms

- **`frontFace` flipped to `COUNTER_CLOCKWISE`** as predicted in Chunk 7. The
  projection matrix negates Y (`proj[1][1] *= -1`), which restores the cube's
  CCW-from-outside winding on screen, so the Chunk 7 `CLOCKWISE` setting would now
  cull the visible faces. Verified by screenshot: solid 3-face cube, not inside-out.
- **GLM configured for Vulkan globally** via two compile definitions in
  `CMakeLists.txt`: `GLM_FORCE_DEPTH_ZERO_TO_ONE` (Vulkan clip depth is [0, 1], not
  OpenGL's [−1, 1]) and `GLM_FORCE_RADIANS`. Set as target defs (not per-file
  `#define`s) so every translation unit that includes GLM agrees — `mesh.h` already
  pulls GLM in transitively, so a header-order `#define` would be fragile.
- **`Camera` is plain maths, owns no GPU objects** (`camera.h/.cpp`): position /
  target / up + fov / near / far, producing `viewMatrix()` (`glm::lookAt`) and
  `projectionMatrix(aspect)` (`glm::perspective` + the Y-flip). Aspect is a
  parameter (recomputed per-frame from the swapchain extent), not a stored field, so
  resize never stretches the cube.
- **Camera is static and positioned off-axis** (eye ≈ (1.2, 0.9, −1.0), target =
  the cube's centre (0, 0, 0.5)) so three faces are visible and foreshortening
  reads clearly — satisfying the "looks like a 3D cube, not a flat hexagon"
  checkpoint. Mouse-driven orbit is Chunk 10.
- **Cube geometry left untouched** (`mesh.cpp` not in the Chunk 8 file list): the
  model matrix is identity and the camera simply targets the cube's existing centre
  at z = 0.5, rather than recentring the mesh at the origin. Keeps the change inside
  the spec's stated file set.
- **`UniformBuffers` owns the whole descriptor stack** (`uniform_buffer.h/.cpp`):
  the per-frame uniform buffers *and* the descriptor set layout / pool / sets. The
  spec lists `uniform_buffer.*` and not a separate descriptor file, so they live
  together. `UniformBufferObject` = model, view, proj (three `mat4`, no std140
  padding needed).
- **One uniform buffer + descriptor set per frame in flight** (count =
  `Renderer::kMaxFramesInFlight` = 2), *not* per swapchain image — they track CPU
  frame pacing, not the presentation image, so the CPU can write next frame's
  matrices without racing the GPU. Passed as a constructor arg to avoid a
  `uniform_buffer → renderer` header dependency.
- **Uniform buffers are host-visible + coherent and persistently mapped** — mapped
  once at construction, updated by a plain `memcpy` each frame in
  `Renderer::drawFrame`. The mesh's staging→device-local dance would be pure
  overhead for tiny, every-frame data.
- **`ShaderPipeline` gained a `VkDescriptorSetLayout` constructor parameter**; the
  pipeline layout now references one set layout (was empty). The layout object is
  owned by `UniformBuffers` and only borrowed by the pipeline, so creation order in
  `main` is: `UniformBuffers` → `ShaderPipeline`.
- **MVP applied in `mesh.vert`**: added the `set = 0, binding = 0` uniform block and
  `gl_Position = proj * view * model * vec4(inPosition, 1.0)`.
- **`drawFrame` signature grew** by `const Camera&` and `UniformBuffers&`. The
  Renderer builds the UBO (identity model + camera view/proj at the current aspect),
  uploads it for the current frame slot, and binds that frame's descriptor set in
  `recordCommandBuffer`. Keeping the frame-index logic inside the Renderer (where it
  already lives) is why the Renderer, not `main`, assembles the UBO.

______________________________________________________________________

## Chunk 9 — OBJ loading

- **OBJ kept; source model was FBX.** The user had the model in FBX but the project
  is specced (and built) around OBJ. FBX would need Autodesk's proprietary SDK or a
  heavy black-box importer (Assimp) that hides the parsing + vertex deduplication
  this chunk exists to teach, and breaks the FetchContent / clean-checkout ethos.
  Decision: user exports FBX → OBJ in Blender; we load OBJ. `assets/mesh.obj` (+ an
  auto-exported `mesh.mtl`) is a triangulatable, normalled, UV'd low-poly model.
- **tinyobjloader via FetchContent**, pinned to `v2.0.0rc13`. Its CMake builds a
  small static `tinyobjloader` target that compiles the implementation TU, so our
  code only `#include <tiny_obj_loader.h>` — no `TINYOBJLOADER_IMPLEMENTATION`
  define on our side.
- **`config.triangulate = true`.** The sample model is quad-based (`f` lines with
  four corners); we let tinyobjloader fan-triangulate so the rest of the code can
  assume triangles.
- **Vertex deduplication via `std::unordered_map<Vertex, uint32_t>`.** Added
  `Vertex::operator==` (in `mesh.h`) and a `std::hash<Vertex>` specialization (in
  `mesh.cpp`, combining `glm/gtx/hash.hpp` hashes — hence the new
  `GLM_ENABLE_EXPERIMENTAL` compile def). Verified live: 9072 corners → 2829 unique
  vertices for the sample model.
- **Flat-normal fallback.** If a face's corners lack normals (`normal_index < 0`),
  a per-triangle normal is derived from the winding (normalized edge cross product)
  and shared by the three corners. The sample model ships normals, so this is
  dormant but kept for robustness and to back the FLAT_NORMALS/SMOOTH_NORMALS
  glossary distinction.
- **Camera framed from the mesh bounding box, not by mutating geometry.** `Mesh`
  computes its object-space AABB at construction and exposes `boundsCenter()` /
  `boundsRadius()`. `main` sets `camera.target` to the centre and backs the camera
  off along a fixed diagonal at `radius / sin(fov/2) · 1.3`. This was essential:
  the sample model is only ~0.05 units across, so the camera's default `nearPlane =
  0.1` would have clipped it entirely — `main` now also derives near/far from the
  fit distance. Only **public** `Camera` fields are touched; `camera.cpp` (Chunk
  10's file) is untouched. Normalizing the mesh to a unit box was rejected as it
  distorts the artist's data and fights Chunk 10's orbit-around-target.
- **OBJ warnings routed to stdout, not stderr.** tinyobjloader returns warnings as
  a string (it does not print); we print them via `std::cout` so the project's
  "clean stderr = no validation errors" verification heuristic stays meaningful.
- **`assets/` directory + `ASSET_DIR` compile def**, mirroring the existing
  `SHADER_DIR` pattern, so the model loads regardless of working directory.
- **`Mesh::cube` left in place** but no longer called — a zero-cost reference for the
  hardcoded-geometry approach. `main`'s mesh variable renamed `cube` → `model`.
- **Note:** the auto-generated `mesh.mtl` contains an absolute `map_Kd` path into the
  user's Downloads. We ignore materials/textures entirely, so it is inert; left
  as-exported rather than hand-editing the asset.

______________________________________________________________________

## Chunk 10 — Orbit camera and input

- **`Camera` refactored from free position → spherical orbit state.** It now stores
  `target` + `distance` + `azimuthRadians` + `elevationRadians` instead of a raw
  `position`; `position()` is derived. This is the natural model for an orbit camera
  and matches the spec's spherical-coordinates framing. Consequence: the public
  `nearPlane`/`farPlane` fields became private (`m_nearPlane`/`m_farPlane`), now
  recomputed each frame from the distance — so `main`'s Chunk 9 framing was updated
  to set `distance`/`sceneRadius`/`minDistance`/`maxDistance` and call `snap()`
  instead of setting `position`/`nearPlane`/`farPlane`.
- **Orbit & pan are displacement-based; zoom is multiplicative.** Orbit/pan are driven
  by mouse-pixel deltas (so total motion = total drag, inherently frame-rate
  independent); zoom multiplies distance by a constant per wheel tick (even feel at
  any scale). Sensitivities are constants in `camera.cpp`.
- **Delta time used for easing, not for drag.** The camera keeps a *displayed* state
  that eases toward the *goal* state each frame via `alpha = 1 − e^(−k·dt)` — the
  frame-rate-independent exponential-smoothing form. This is where `dt` is genuinely
  needed and gives the checkpoint's "continuous and clean, no jitter at any frame
  rate". Drag itself does **not** multiply by `dt` (that would make it frame-rate
  *dependent*); the code comments call this out. `snap()` settles the displayed state
  to the goal once after framing so the first frame is already in place.
- **Gimbal lock handled by clamping, not quaternions.** Elevation is clamped to ±89°
  so the view direction never aligns with the up vector (which would degenerate
  `glm::lookAt`). The spec explicitly prefers clamping at this stage; quaternions are
  noted as the heavier alternative.
- **Zoom clamped to keep the camera outside the mesh.** `minDistance = modelRadius *
  1.1` (just outside the bounding sphere) and `maxDistance = fitDistance * 8`. Near/far
  planes track the displayed distance (`distance − 1.5·radius` … `distance + 3·radius`,
  near floored positive), so the model never clips at any zoom.
- **Pan scaled by distance, not pixel-exact.** `worldPerPixel = distance · k`; simple
  and feels consistent across zoom. A fully screen-accurate pan would also divide by
  the viewport height in pixels — deferred as unnecessary for the checkpoint.
- **Input gathered in `SdlContext`, applied in `main`.** `SdlContext` accumulates
  mouse motion (`xrel`/`yrel`), wheel ticks, and live button states from the SDL
  event loop, exposed via `takeMouseInput()` (a `MouseInput` struct) which clears the
  per-frame deltas but keeps button levels. `main` applies them to the camera each
  frame, then calls `camera.update(dt)`. Keeps all SDL specifics behind `SdlContext`.
- **Verification limit:** interactive drag/zoom cannot be injected headlessly (per the
  project's GUI-checkpoint approach), so Chunk 10 was verified by a clean build, a
  correctly-framed static render (screenshot), zero validation errors, and code review
  of the input→camera→matrix path — not by a recorded drag.

______________________________________________________________________

## Chunk 11 — Diffuse lighting

- **One directional light, fixed in world space.** Direction + colour + a constant
  ambient term, all in the UBO. World-space (not view-space) lighting means orbiting
  the camera leaves the light put, which is the checkpoint requirement. Direction
  points *toward* the light so `dot(N, L)` is positive on lit faces.
- **Per-fragment (Phong) shading, not per-vertex.** `mesh.vert` outputs the
  world-space normal; `mesh.frag` renormalises and evaluates `ambient + max(dot(N,L),
  0)·lightColour`, modulating the existing orange as the albedo. The model's smooth
  OBJ normals therefore shade smoothly. Per-vertex/Gouraud documented as the contrast.
- **Lighting math lives in `Renderer::drawFrame`, not `main.cpp`.** The spec lists
  `main.cpp` for Chunk 11, but in this codebase the UBO is assembled in the renderer
  (the Chunk 8 deviation), so the normal matrix and the light constants are set there.
  `main.cpp` is unchanged this chunk. The light is hardcoded (not user-tweakable),
  which is why it did not need to surface in `main`.
- **Normal matrix = `transpose(inverse(mat3(model)))`, uploaded as a `mat4`.** Built
  every frame even though the model is currently identity (so it is also identity),
  so the correct construction is already in place for when the model gains rotation or
  non-uniform scale. Stored as a `mat4` and read as `mat3` in the shader to sidestep
  std140's per-column `mat3` padding.
- **Lighting vectors are `vec4`, not `vec3`, in the UBO.** std140 gives `vec3` a
  16-byte alignment with awkward trailing-padding rules; using `vec4` (w unused) makes
  the C++ struct and the GLSL block layouts trivially identical. Documented in
  `uniform_buffer.h`.
- **Descriptor set layout stage flags widened to `VERTEX | FRAGMENT`.** The single
  UBO is now read by both stages (matrices/normal in the vertex shader, light in the
  fragment shader), so the one descriptor binding is made visible to both.
- **Verification:** the static screenshot shows clear diffuse shading (bright
  light-facing faces, dark-but-not-black shadow faces, the model's form and kickstand
  now readable), replacing the flat orange — confirming the lighting. "Light stays
  fixed while orbiting" is true by construction (world-space constant, no camera term)
  but not headlessly drag-tested.

______________________________________________________________________

## Chunk 12 — Polish and completeness

- **Glossary audit passed.** A scripted check extracts every `See Glossary:` token
  from `src/` and `shaders/` (95 unique) and confirms each resolves to a real `##`
  entry in `Glossary.md`. No dangling references. The two spec-required entries
  (`RESOURCE_LIFETIME`, `VULKAN_TEARDOWN_ORDER`) were added and tied to a reference in
  `VulkanContext`'s destructor; no other gaps were found, so no other entries needed
  adding. Total entries: 116.
- **Teardown order verified, not changed.** It is already correct and enforced
  structurally by the RAII declaration order in `main` (destruction is the exact
  reverse: framebuffers before image views, all Vk children before the device, surface
  before the window). Documented in the destructor comment + the two new glossary
  entries rather than altered.
- **Validation clean.** Running with layers on (debug build) produces no validation
  output on `stderr` at startup, steady state, or during the startup swapchain
  recreate. Verified on macOS / MoltenVK (the only platform available here); the code
  is written portably but Linux/Windows were not run — noted honestly in the README.
- **Command-line model argument.** `main(argc, argv)` takes an optional OBJ path
  (`argv[1]`), defaulting to the bundled `assets/mesh.obj`. The camera's bounds-based
  auto-framing (Chunk 9) means any model frames correctly with no per-model tuning.
- **Second sample model added: `assets/torus.obj`.** Generated procedurally (64×32
  segment torus, R=1.0/r=0.35, with normals + UVs) rather than downloaded, keeping the
  repo self-contained. It is a deliberately different scale (~2.7 units vs the switch's
  ~0.05) to exercise auto-framing; verified loading via the CLI arg (dedup 12288→2048).
- **README written** covering what the project is, prerequisites (Vulkan SDK is the
  one non-fetched dependency), per-platform build, controls, and the model argument.
- **Reviewed open items, intentionally left as-is:** the benign startup
  double-swapchain-create (a duplicate log line, not a warning) and the minimised-window
  busy-wait `continue`. Neither crashes nor warns, so both are kept simple for this POC
  rather than adding guards in the polish pass. The `oldSwapchain`-less recreate is
  robust (it waits for device idle first); the smoother-resize optimisation remains a
  noted nicety, not a fix.

______________________________________________________________________

## Chunk 13 — Diffuse texture support

- **stb_image via FetchContent**, pinned to `master` (stb publishes no release tags).
  Header-only with no CMake target, so it is only populated and `${stb_SOURCE_DIR}` is
  added to the include path; `STB_IMAGE_IMPLEMENTATION` is defined once, in `mesh.cpp`.
- **Image GPU helpers added to `VulkanContext`** (`createImage`,
  `transitionImageLayout`, `copyBufferToImage`) plus a factored
  `begin/endSingleTimeCommands` that `copyBuffer` now also uses. This mirrors the
  existing buffer helpers and is the natural home, but is a **deviation** from the
  spec's Chunk 13 file list (which omits `vulkan_context`). `transitionImageLayout`
  only supports the two transitions the project needs (undefined→transfer-dst,
  transfer-dst→shader-read) and throws otherwise.
- **`Texture` class lives in `mesh.h/.cpp`** (per the spec file list): owns the image,
  memory, view, and sampler; loads via stb_image (forced RGBA) and uploads with the
  same staging pattern as geometry. An **empty path yields a 1×1 white fallback**, so a
  model with no diffuse map (the cube, the torus) still has a valid descriptor to bind
  and renders white-lit rather than crashing.
- **MTL parsed for the diffuse path.** `Mesh::fromObjFile` reads the first material's
  `diffuse_texname` and resolves it **relative to the OBJ's directory** — necessary
  because the `map_Kd` path was scrubbed to a basename in Chunk 9. `Mesh` exposes
  `diffuseTexturePath()`; `main` loads the `Texture` from it.
- **Descriptor binding 1 = combined image sampler** (fragment stage), added to the
  layout/pool/sets in `uniform_buffer.cpp`. The descriptors are written with the
  texture's view+sampler; the same texture is bound for every frame in flight (only the
  UBO differs per frame). This is why `main` was **reordered**: mesh → texture →
  uniforms → pipeline, so the descriptors can be built with the texture in hand.
- **`shader_pipeline` NOT touched** despite the spec listing it: the descriptor set
  layout lives in `uniform_buffer` here (the Chunk 8 deviation), so adding the sampler
  binding needed no pipeline change — it just references the updated layout. Conversely
  **`mesh.vert` WAS touched** (not in the spec list) to pass UV through to the fragment
  shader, which is unavoidable for sampling.
- **sRGB texture format** (`VK_FORMAT_R8G8B8A8_SRGB`): the diffuse map holds sRGB-encoded
  colour, so the GPU converts samples to linear for the (linear) lighting maths, and the
  sRGB swapchain converts back on output. Correct colour pipeline end to end.
- **V-flip on UV load** (`1 - v`): caught during verification — the texture rendered
  upside-down at first. OBJ uses V=0 at the image bottom (OpenGL), Vulkan samples V=0 at
  the top, so V is flipped when read in `Mesh::fromObjFile`. Verified by screenshot: the
  "NINTENDO SWITCH" logo reads right-side-up after the fix.
- **Bilinear filtering, REPEAT wrapping, single mip level, no anisotropy.** Mipmaps and
  trilinear filtering are documented as concepts but **not generated** — a single level
  is fine for inspecting one model at close range and keeps the chunk focused.
  Anisotropy is left off because the device feature was not enabled at device creation.
- **Verified:** the Sketchfab Switch (OBJ+MTL+JPG) renders with its diffuse texture, lit
  by Lambert (brighter toward the light), no visible seams; the untextured torus renders
  white-lit via the fallback; zero validation errors; clean exit.

______________________________________________________________________

## Open items / things deferred deliberately

- **`Renderer` lives in its own file**, where the spec attributed command buffers
  to `vulkan_context`. Settled: keeping it separate; noted as a deviation.
- **`frontFace` winding**: resolved in Chunk 8 — flipped to `COUNTER_CLOCKWISE`
  once the projection matrix's Y-flip went in, as anticipated.
- **Swapchain `recreate()`** currently destroys before recreating (no
  `oldSwapchain`); could be upgraded for smoother resize-while-rendering.
- **Startup double swapchain-create log**: left in place; trivial to suppress
  with an "extent unchanged → skip" guard if desired.
- **Minimised window** uses a busy-wait `continue` (skips drawing); fine for a
  POC, could block on events to save power.
