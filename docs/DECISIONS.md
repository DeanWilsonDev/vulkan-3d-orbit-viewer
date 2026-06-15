# Implementation Decision Log

This document records the decisions made while building the Vulkan Orbit Viewer,
chunk by chunk. It complements `SPEC.md` (which sets the high-level direction)
and `Glossary.md` (which explains the concepts). The focus here is the *why*
behind concrete implementation choices — especially ones not spelled out in the
spec, ones forced by the host platform, and any deviations from the spec.

Decisions inherited directly from `SPEC.md` ("Confirmed Decisions" table —
C++, SDL3, Vulkan, MoltenVK, GLM, OBJ, layered files) are not repeated here
unless the implementation added nuance to them.

Status as of writing: Chunks 1–6 complete.

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

______________________________________________________________________

## Open items / things deferred deliberately

- **`Renderer` lives in its own file**, where the spec attributed command buffers
  to `vulkan_context`. Settled: keeping it separate; noted as a deviation.
- **`findMemoryType` location**: still a local static in `render_pass.cpp`; may
  move to `VulkanContext` in Chunk 7 when buffers need the same logic.
- **Swapchain `recreate()`** currently destroys before recreating (no
  `oldSwapchain`); could be upgraded for smoother resize-while-rendering.
- **Startup double swapchain-create log**: left in place; trivial to suppress
  with an "extent unchanged → skip" guard if desired.
- **Minimised window** uses a busy-wait `continue` (skips drawing); fine for a
  POC, could block on events to save power.
