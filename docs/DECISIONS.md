# Implementation Decision Log

This document records the decisions made while building the Vulkan Orbit Viewer,
chunk by chunk. It complements `SPEC.md` (which sets the high-level direction)
and `Glossary.md` (which explains the concepts). The focus here is the *why*
behind concrete implementation choices — especially ones not spelled out in the
spec, ones forced by the host platform, and any deviations from the spec.

Decisions inherited directly from `SPEC.md` ("Confirmed Decisions" table —
C++, SDL3, Vulkan, MoltenVK, GLM, OBJ, layered files) are not repeated here
unless the implementation added nuance to them.

Status as of writing: Chunks 1–4 complete.

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

______________________________________________________________________

## Open items / things deferred deliberately

- **First on-screen frame** (clear colour visible): deferred from Chunk 4 to
  Chunk 6 per the scope decision above.
- **`findMemoryType` location**: may move from `render_pass.cpp` to
  `VulkanContext` in Chunk 7.
- **Swapchain `recreate()`** currently destroys before recreating (no
  `oldSwapchain`); could be upgraded if smooth resize-while-rendering is wanted.
- **Startup double swapchain-create log**: left in place; trivial to suppress
  with an "extent unchanged → skip" guard if desired.
