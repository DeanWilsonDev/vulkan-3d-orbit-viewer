# Glossary

A living companion reference for the Vulkan Orbit Viewer. It is **not** a closed
list of terms that appear in the code — it is a broad reference for the computer
graphics domain. Any concept a graphics programmer would treat as domain
knowledge gets an entry, and if understanding one entry requires another, that
related concept is added too.

Terms are written in `SCREAMING_SNAKE_CASE` so a `See Glossary: TERM` reference
in the source code is easy to search for. Entries are grouped by the chunk that
introduced them, but the Glossary is meant to be read and searched as a whole.

______________________________________________________________________

## Chunk 1 — Windowing and the Event Loop

## WINDOWING_SYSTEM

**What it is**
The part of an operating system that creates and manages windows: the
rectangular regions on screen that applications draw into. It owns where windows
sit, which one has focus, and it delivers input (mouse, keyboard) to the right
window. On Linux this is X11 or Wayland, on macOS it is Cocoa/Quartz, and on
Windows it is the Win32 window manager.

**Why it matters**
A renderer does not draw to "the screen" directly — it draws into a window the
windowing system hands it, and the windowing system composites that window with
everyone else's onto the display. Graphics APIs like Vulkan are deliberately
ignorant of windows; they need an external system to obtain a drawable region
and to feed back input and resize events. This is also why every platform needs
its own glue, and why a cross-platform abstraction (here, SDL3) is so valuable.

**How it appears in this project**
`src/sdl_context.cpp` talks to the windowing system entirely through SDL3:
`SDL_Init(SDL_INIT_VIDEO)` connects to it and `SDL_CreateWindow` asks it for a
window. No other file in the project touches the windowing system — that
isolation is the whole point of `SdlContext`.

**Further reading**
- [Wayland (freedesktop.org)](https://wayland.freedesktop.org/docs/html/)
- [SDL3 Video subsystem](https://wiki.libsdl.org/SDL3/CategoryVideo)
- [Windowing system (Wikipedia)](https://en.wikipedia.org/wiki/Windowing_system)

______________________________________________________________________

## SDL3

**What it is**
Simple DirectMedia Layer, version 3 — a cross-platform C library that abstracts
the parts of an operating system a game or interactive application needs:
creating windows, reading mouse/keyboard/gamepad input, audio, timers, and the
glue that lets a graphics API draw into a window. It presents one API and
implements it per platform underneath.

**Why it matters**
Without it, every platform would need its own window-creation and input code
(Xlib/Wayland, Cocoa, Win32). SDL3 collapses those into a single API, so the
rest of the project is platform-agnostic. Crucially for this project, SDL3 also
knows how to bridge a window to Vulkan: it can report the instance extensions
Vulkan needs to present to that window and can create a Vulkan surface for it
(used from Chunk 2). SDL3 is the chosen layer here because it matches Umbra's
existing dependency and handles X11/Wayland transparently.

**How it appears in this project**
The whole of `src/sdl_context.h` / `src/sdl_context.cpp`. `CMakeLists.txt`
fetches and builds it via `FetchContent` (pinned to release `3.2.0`). Note SDL3
changed some conventions from SDL2: `SDL_Init` returns a `bool` (true on
success) and `SDL_CreateWindow` no longer takes x/y position arguments.

**Further reading**
- [SDL3 Wiki — front page](https://wiki.libsdl.org/SDL3/FrontPage)
- [Migrating from SDL2 to SDL3](https://wiki.libsdl.org/SDL3/README/migration)
- [SDL GitHub repository](https://github.com/libsdl-org/SDL)

______________________________________________________________________

## EVENT_LOOP

**What it is**
The central loop of an interactive application. Each pass through the loop is one
"frame": it pulls pending events (input, window changes) from a queue, reacts to
them, updates application state, and — in a renderer — draws. The loop repeats as
fast as it can (or capped to a refresh rate) until something tells it to stop.

**Why it matters**
A GUI or real-time program cannot just run start-to-finish like a script; it must
stay responsive to the user indefinitely while continuously producing output. The
event loop is the structure that makes that possible: it is the heartbeat that
ties input, simulation, and rendering into a steady rhythm. Almost every
interactive graphics program — games, editors, viewers — is built around one.

**How it appears in this project**
The `while (running)` loop in `src/main.cpp` is the event loop. In Chunk 1 it
only calls `processEvents()`; in later chunks the same loop will also update the
camera and submit a frame to the GPU. `SdlContext::processEvents()` in
`src/sdl_context.cpp` implements the "drain events and decide whether to
continue" half of each iteration.

**Further reading**
- [Game Programming Patterns — Game Loop](https://gameprogrammingpatterns.com/game-loop.html)
- [Event loop (Wikipedia)](https://en.wikipedia.org/wiki/Event_loop)

______________________________________________________________________

## EVENT_POLLING

**What it is**
Asking the event system "are there any events waiting right now?" and processing
whatever it returns without waiting for more. The opposite is *blocking*, where
the program sleeps until an event arrives. SDL exposes both: `SDL_PollEvent`
(non-blocking, returns immediately) and `SDL_WaitEvent` (blocking).

**Why it matters**
A real-time renderer must keep drawing frames even when the user is doing
nothing — an animation must advance, a spinning model must keep spinning. If the
loop blocked waiting for input, the picture would freeze between events. Polling
lets the loop grab any pending input and immediately move on to updating and
rendering the next frame. (A purely static UI that only redraws on input might
prefer blocking to save power, but that is not this project.)

**How it appears in this project**
The `while (SDL_PollEvent(&event))` loop inside `SdlContext::processEvents()` in
`src/sdl_context.cpp`. It consumes all queued events each frame and returns
control to the main loop immediately, regardless of whether any input arrived.

**Further reading**
- [SDL_PollEvent](https://wiki.libsdl.org/SDL3/SDL_PollEvent)
- [SDL_WaitEvent](https://wiki.libsdl.org/SDL3/SDL_WaitEvent)
- [Polling vs. interrupts/blocking (Wikipedia)](https://en.wikipedia.org/wiki/Polling_(computer_science))

______________________________________________________________________

## Chunk 2 — Vulkan Instance and Device

## VULKAN_INSTANCE

**What it is**
The root Vulkan object and the very first thing you create. It represents your
application's connection to the Vulkan library: it records which API version and
which instance-level extensions and layers you want, and from it you enumerate
GPUs. Every other Vulkan object is created, directly or indirectly, from the
instance.

**Why it matters**
Vulkan has no global state — there is no implicit "current context" like OpenGL's.
The instance is the explicit object that holds what would otherwise be global:
the loaded extensions, the validation layers, and the list of available physical
devices. Making it explicit is what lets two Vulkan applications (or two parts of
one) coexist without stepping on each other.

**How it appears in this project**
`VulkanContext::createInstance` in `src/vulkan_context.cpp` builds a
`VkInstanceCreateInfo` (application info, extensions from SDL, validation layer,
macOS portability flags) and calls `vkCreateInstance`. The handle is stored in
`m_instance` and destroyed last in the destructor.

**Further reading**
- [Vulkan Tutorial — Instance](https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Instance)
- [Vulkan Spec — Instances](https://docs.vulkan.org/spec/latest/chapters/initialization.html)

______________________________________________________________________

## VULKAN_EXTENSIONS

**What it is**
Optional additions to the Vulkan API that are not part of the core spec. They
come in two scopes: *instance* extensions (capabilities of the Vulkan
implementation as a whole, e.g. talking to a window system) and *device*
extensions (capabilities of a particular GPU, e.g. the swapchain). You must
explicitly request every extension you use, and only after checking it is
available.

**Why it matters**
Core Vulkan is deliberately minimal and platform-neutral — it knows nothing about
windows, for example. Everything platform- or vendor-specific is layered on as an
extension, so the core stays portable while hardware and OS features are still
reachable. Presenting to a screen at all requires extensions (the WSI family),
which is why even a "hello triangle" needs a handful.

**How it appears in this project**
Instance extensions are gathered in `createInstance` (`SDL_Vulkan_GetInstanceExtensions`
for surface support, `VK_EXT_debug_utils` for validation, and the macOS
portability extensions). Device extensions are in `kRequiredDeviceExtensions`
(`VK_KHR_swapchain`) and the conditional `VK_KHR_portability_subset`, enabled in
`createLogicalDevice`. Extension functions like `vkCreateDebugUtilsMessengerEXT`
must be looked up at runtime with `vkGetInstanceProcAddr`.

**Further reading**
- [Vulkan Guide — Extensions and features](https://docs.vulkan.org/guide/latest/extensions.html)
- [Vulkan Spec — Extensions](https://docs.vulkan.org/spec/latest/chapters/extensions.html)

______________________________________________________________________

## VALIDATION_LAYERS

**What it is**
Optional layers that sit between your application and the Vulkan driver and check
everything you do: correct parameters, correct object lifetimes, correct
synchronisation, threading rules, and more. The standard one,
`VK_LAYER_KHRONOS_validation`, reports problems through a *debug messenger* — a
callback you register that receives each message. They are a development tool and
are disabled in release builds.

**Why it matters**
Vulkan's core API does almost no error checking — that is a deliberate design
choice for performance, and it means a mistake usually produces a crash, a
corrupted image, or silent undefined behaviour rather than a helpful error.
Validation layers put that checking back, but only while you are developing.
Practically, they are the difference between "the screen is black and I have no
idea why" and a precise sentence telling you which call was wrong and why. You
always develop with them on.

**How it appears in this project**
Enabled in debug builds via `kEnableValidation` in `src/main.cpp`. In
`src/vulkan_context.cpp`: `validationLayerAvailable` checks the layer is
installed, `debugCallback` prints messages to stderr, `populateDebugMessengerCreateInfo`
selects which severities/types to receive, and `setupDebugMessenger` installs the
persistent messenger (a second copy is chained to instance creation so instance
setup/teardown is also covered).

**Further reading**
- [Vulkan Tutorial — Validation layers](https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Validation_layers)
- [Vulkan Validation Layers (GitHub)](https://github.com/KhronosGroup/Vulkan-ValidationLayers)

______________________________________________________________________

## SURFACE

**What it is**
A `VkSurfaceKHR` — an abstract handle representing the specific window region
Vulkan will present rendered images to. It is the bridge between Vulkan (which
knows nothing about windows) and the operating system's windowing system. It is
created with a platform-specific extension, though here SDL hides that detail.

**Why it matters**
Vulkan core cannot talk to a window, by design — that keeps it portable. The
surface is the negotiated meeting point: from it you query which image formats,
colour spaces, and present modes the window supports, and the swapchain (Chunk 3)
is built to present into it. Whether a GPU queue can present at all is a property
of the (queue family, surface) pair, not of the queue alone, because it depends
on the window system.

**How it appears in this project**
`VulkanContext::createSurface` calls `SDL_Vulkan_CreateSurface`, which picks the
right platform WSI extension and wraps the native window for us — one call that
works on Linux, macOS, and Windows. The window itself is created with the
`SDL_WINDOW_VULKAN` flag in `src/sdl_context.cpp`. The surface is destroyed
before the instance that owns it.

**Further reading**
- [Vulkan Tutorial — Window surface](https://vulkan-tutorial.com/Drawing_a_triangle/Presentation/Window_surface)
- [SDL_Vulkan_CreateSurface](https://wiki.libsdl.org/SDL3/SDL_Vulkan_CreateSurface)

______________________________________________________________________

## PHYSICAL_DEVICE

**What it is**
A `VkPhysicalDevice` — a handle to a single piece of Vulkan-capable hardware (a
GPU) that the instance can see, or sometimes a software implementation. You do
not create it; you *enumerate* the available ones and query each for its
properties, features, memory, and queue families to decide which to use.

**Why it matters**
A machine may have several GPUs (an integrated one and a discrete one, say), and
they differ in capabilities and performance. Vulkan refuses to choose for you:
selecting the physical device is your job, based on what your application needs.
The physical device is read-only — it describes hardware. To actually *use* it
you create a logical device from it.

**How it appears in this project**
`VulkanContext::pickPhysicalDevice` enumerates devices with
`vkEnumeratePhysicalDevices` and picks the first that has the queue families we
need and supports the required device extensions. A real engine would score
devices to prefer a discrete GPU; a single-mesh viewer takes first-fit. The
chosen device's name is printed at startup.

**Further reading**
- [Vulkan Tutorial — Physical devices and queue families](https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Physical_devices_and_queue_families)
- [Vulkan Spec — Devices](https://docs.vulkan.org/spec/latest/chapters/devsandqueues.html)

______________________________________________________________________

## LOGICAL_DEVICE

**What it is**
A `VkDevice` — your application's own working handle to a chosen physical device.
Creating it is where you commit to which queue families, device features, and
device extensions you will use. Almost every Vulkan call that does real work
(allocating memory, creating pipelines, recording commands) goes through the
logical device.

**Why it matters**
The split between physical and logical device separates *describing* hardware
from *using* it. The logical device is the configured, stateful connection: two
applications can each create their own logical device for the same physical GPU
with different features enabled, isolated from one another. It is also the object
whose creation declares, up front, exactly what subset of the GPU you intend to
use — consistent with Vulkan's "no surprises for the driver" philosophy.

**How it appears in this project**
`VulkanContext::createLogicalDevice` builds one `VkDeviceQueueCreateInfo` per
unique required queue family, requests the device extensions, and calls
`vkCreateDevice`. It then retrieves the graphics and present queue handles with
`vkGetDeviceQueue`. The device is destroyed first in the destructor (before the
instance).

**Further reading**
- [Vulkan Tutorial — Logical device and queues](https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Logical_device_and_queues)
- [Vulkan Spec — Devices and Queues](https://docs.vulkan.org/spec/latest/chapters/devsandqueues.html)

______________________________________________________________________

## QUEUE_FAMILY

**What it is**
GPUs execute submitted work through *queues*, and queues are grouped into
*families* by capability. One family might support graphics, compute, and
transfer; another might support only transfer (a dedicated copy engine). Each
family advertises its capabilities as flags, and you request queues from the
families whose capabilities you need.

**Why it matters**
Work is not handed to the GPU one call at a time — it is recorded and *submitted*
to a queue, which the GPU drains asynchronously. Different hardware blocks handle
different work, which is why queues come in families. Notably, the ability to
*present* to a surface is not a queue-family flag at all: it depends on the window
system and must be queried per (family, surface). The graphics and present
families are often the same on a given GPU, but the spec does not promise it, so
correct code looks them up independently.

**How it appears in this project**
`QueueFamilyIndices` (in `src/vulkan_context.h`) holds the graphics and present
family indices as `std::optional`. `findQueueFamilies` scans the families,
matching `VK_QUEUE_GRAPHICS_BIT` for graphics and using
`vkGetPhysicalDeviceSurfaceSupportKHR` for present. On the M2 Pro both resolve to
family 0, which is why `createLogicalDevice` deduplicates them with a `std::set`.

**Further reading**
- [Vulkan Tutorial — Queue families](https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Physical_devices_and_queue_families)
- [Vulkan Guide — Queues](https://docs.vulkan.org/guide/latest/queues.html)

______________________________________________________________________

## MOLTENVK

**What it is**
A *portability* implementation of Vulkan for Apple platforms: a layer that
translates Vulkan calls into Apple's native Metal API, since macOS and iOS do not
support Vulkan directly. It ships with the Vulkan SDK on macOS. Because it is not
100% conformant to the Vulkan spec, it identifies itself as a portability driver.

**Why it matters**
It is what makes "write Vulkan once, run on macOS too" possible without a
separate Metal backend. The cost is a few required concessions: portability
drivers are hidden from enumeration unless you opt in (the
`VK_KHR_portability_enumeration` instance extension plus a creation flag), and if
a device exposes `VK_KHR_portability_subset` you are *required* to enable it,
acknowledging the small ways it diverges from full Vulkan.

**How it appears in this project**
The `#ifdef __APPLE__` block in `VulkanContext::createInstance` enables the
portability-enumeration extension and sets
`VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR`;
`createLogicalDevice` conditionally enables `VK_KHR_portability_subset`. On this
machine the selected GPU is "Apple M2 Pro" via the MoltenVK driver.

**Further reading**
- [MoltenVK (GitHub)](https://github.com/KhronosGroup/MoltenVK)
- [Vulkan Guide — Portability](https://docs.vulkan.org/guide/latest/portability_initialization.html)

______________________________________________________________________

## Chunk 3 — Swapchain

## SWAPCHAIN

**What it is**
A `VkSwapchainKHR` — a small set of images, managed as a rotating queue, that you
render into and then hand to the display. At any moment one image is being shown
on screen while the application draws the next into a different one; when the new
one is ready it is *presented* and the roles rotate. The swapchain is created for
a specific surface, at a specific size, format, and present mode.

**Why it matters**
You must never draw directly into the image currently being scanned out to the
monitor — the viewer would see it half-finished (tearing) or flickering. The
swapchain solves this by giving you separate images to work on and a controlled
hand-off back to the display. It is also intrinsically tied to the window's size
and the surface's properties, which is why resizing the window forces it to be
rebuilt. In OpenGL the equivalent buffers were hidden; Vulkan makes the swapchain
an explicit object you configure and own.

**How it appears in this project**
The whole of `src/swapchain.h` / `src/swapchain.cpp`. `Swapchain::create` chooses
the format, present mode, extent, and image count, then calls
`vkCreateSwapchainKHR` and retrieves the images. `recreate` rebuilds it on resize
(after `VulkanContext::waitIdle`). `src/main.cpp` calls `recreate()` when
`SdlContext::takeResized()` reports a resize.

**Further reading**
- [Vulkan Tutorial — Swap chain](https://vulkan-tutorial.com/Drawing_a_triangle/Presentation/Swap_chain)
- [Vulkan Guide — Swapchain](https://docs.vulkan.org/guide/latest/wsi.html)

______________________________________________________________________

## IMAGE_VIEW

**What it is**
A `VkImageView` — a description of *how to read or write* a `VkImage`: which
portion (mip levels, array layers), interpreted as which format, with which
channel ordering. The image holds the pixels; the view is the lens you look at
them through. You can have several different views of one image.

**Why it matters**
A raw `VkImage` is opaque memory with no agreed interpretation, so the pipeline
never binds one directly — it always binds a view. The indirection is what lets
the same image be used in different ways (e.g. a colour target now, a texture
later, or one layer of a cube map) without copying. For the swapchain it is small
but mandatory: each presentable image needs a view before a framebuffer (Chunk 4)
can attach it.

**How it appears in this project**
`Swapchain::create` builds one `VkImageView` per swapchain image with
`vkCreateImageView`: a 2D view of the colour aspect, one mip level, one layer,
identity channel swizzle. They are stored in `m_imageViews` and destroyed in
`Swapchain::destroy` (the images themselves belong to the swapchain).

**Further reading**
- [Vulkan Tutorial — Image views](https://vulkan-tutorial.com/Drawing_a_triangle/Presentation/Image_views)
- [Vulkan Spec — Image Views](https://docs.vulkan.org/spec/latest/chapters/resources.html#resources-image-views)

______________________________________________________________________

## PRESENT_MODE

**What it is**
The policy by which finished swapchain images are handed to the display. The two
that matter here:
- **FIFO** — a strict first-in-first-out queue, one image shown per vertical
  refresh. No tearing, and it is the only mode the spec guarantees exists. This
  is classic v-sync.
- **MAILBOX** — like FIFO but the queue holds only one waiting image; a newer one
  replaces it rather than queueing behind it. No tearing, lower latency, but it
  renders images that may never be shown (it burns more GPU/power and needs an
  extra image).

**Why it matters**
The present mode is the direct trade-off between latency, smoothness, power, and
tearing. FIFO is the safe, power-friendly default; MAILBOX trades efficiency for
responsiveness. There is also IMMEDIATE (present right away, tearing allowed),
which this project never selects. Choosing the mode is how you tune that balance.

**How it appears in this project**
`choosePresentMode` in `src/swapchain.cpp` prefers `VK_PRESENT_MODE_MAILBOX_KHR`
and falls back to `VK_PRESENT_MODE_FIFO_KHR`. On this machine MAILBOX is not
offered, so FIFO is used — printed in the swapchain log line.

**Further reading**
- [Vulkan Spec — VkPresentModeKHR](https://docs.vulkan.org/spec/latest/chapters/VK_KHR_surface/wsi.html#VkPresentModeKHR)
- [Vulkan Tutorial — Presentation mode](https://vulkan-tutorial.com/Drawing_a_triangle/Presentation/Swap_chain)

______________________________________________________________________

## SCREEN_TEARING

**What it is**
A visual artefact where the top and bottom parts of the screen show two
different frames at once, split by a horizontal seam. It happens when the image
being scanned out to the monitor is changed partway through a refresh, so the
display shows part of the old frame and part of the new one.

**Why it matters**
Tearing is the core problem the swapchain and present modes exist to manage. The
display refreshes at fixed intervals (the vertical blank); if you swap the
displayed image at any other moment, the seam appears. Synchronising the swap to
the vertical blank (as FIFO and MAILBOX both do) eliminates it. Understanding
tearing is what makes the present-mode choices meaningful rather than arbitrary.

**How it appears in this project**
Not a code symbol — it is the artefact avoided by choosing a v-synced present
mode in `choosePresentMode` (`src/swapchain.cpp`). Both FIFO and MAILBOX prevent
it; only the unused IMMEDIATE mode would allow it.

**Further reading**
- [Screen tearing (Wikipedia)](https://en.wikipedia.org/wiki/Screen_tearing)
- [Vertical blanking interval (Wikipedia)](https://en.wikipedia.org/wiki/Vertical_blanking_interval)

______________________________________________________________________

## DOUBLE_BUFFERING

**What it is**
Using two images: one being displayed (the *front* buffer) and one being drawn
into (the *back* buffer). When the back buffer is finished, the two are swapped.
The viewer only ever sees a complete image.

**Why it matters**
It is the minimum needed to avoid showing a half-drawn frame. With a single
buffer the viewer would watch the image being painted; with two, drawing always
happens off-screen and only finished frames are shown. The cost is that, with
strict v-sync, the GPU can stall waiting for the swap — which is what triple
buffering improves on.

**How it appears in this project**
`Swapchain::create` requests `minImageCount + 1` images. On this surface the
minimum is 2, giving 3 — so the project actually runs triple-buffered, but
double buffering is the concept this "+1" builds up from.

**Further reading**
- [Multiple buffering (Wikipedia)](https://en.wikipedia.org/wiki/Multiple_buffering)
- [Vulkan Tutorial — Swap chain](https://vulkan-tutorial.com/Drawing_a_triangle/Presentation/Swap_chain)

______________________________________________________________________

## TRIPLE_BUFFERING

**What it is**
Using three images instead of two: one on display and two the application can
cycle through, so it never has to wait for the displayed image to be released
before starting the next frame.

**Why it matters**
With double buffering and strict v-sync, if the application finishes early it
must sit idle until the next refresh frees the front buffer. A third image lets
it start the following frame immediately, keeping the GPU busy and reducing
latency — at the cost of one more image's worth of memory. MAILBOX present mode
relies on this extra image to always present the freshest frame.

**How it appears in this project**
The `minImageCount + 1` request in `Swapchain::create` resolves to 3 images on
this Mac (printed as "3 images" in the swapchain log), i.e. triple buffering.

**Further reading**
- [Multiple buffering — triple buffering (Wikipedia)](https://en.wikipedia.org/wiki/Multiple_buffering#Triple_buffering)
- [Vulkan Guide — Swapchain image count](https://docs.vulkan.org/guide/latest/wsi.html)

______________________________________________________________________

## COLOUR_FORMAT

**What it is**
A `VkFormat` describing how the colour of one pixel is stored: which channels are
present, in what order, at what bit depth, and how the stored numbers map to
values. For example `VK_FORMAT_B8G8R8A8_SRGB` is blue, green, red, alpha, each 8
bits, with the values interpreted in the sRGB encoding.

**Why it matters**
The format determines memory size, precision, channel order, and — crucially —
whether the hardware applies sRGB conversion when reading and writing. Picking the
wrong one gives swapped colours, banding, or an image that looks too dark or too
bright. The swapchain's format must be one the surface supports, so it is chosen
from a queried list rather than assumed.

**How it appears in this project**
`chooseSurfaceFormat` in `src/swapchain.cpp` prefers `VK_FORMAT_B8G8R8A8_SRGB`.
The chosen format is stored in `m_imageFormat`, used to create the image views,
printed at startup, and will be declared to the render pass in Chunk 4.

**Further reading**
- [Vulkan Spec — Formats](https://docs.vulkan.org/spec/latest/chapters/formats.html)
- [Understanding Vulkan image formats](https://www.khronos.org/blog/understanding-vulkan-image-formats)

______________________________________________________________________

## COLOUR_SPACE

**What it is**
A `VkColorSpaceKHR` describing how the numeric pixel values in a presented image
should be interpreted as actual light by the display — for the swapchain, almost
always `VK_COLOR_SPACE_SRGB_NONLINEAR_KHR` (standard sRGB). It is paired with the
colour format when choosing a surface format.

**Why it matters**
Human brightness perception is non-linear, and displays emit light non-linearly,
so a "gamma" encoding (sRGB) packs values to match perception and avoid banding
in dark tones. The colour space tells the display which encoding the values are
in; mismatch it and the whole image looks wrong. Format (how bits are stored) and
colour space (how those values become light) are two halves of one decision,
which is why Vulkan picks them together as a `VkSurfaceFormatKHR`.

**How it appears in this project**
`chooseSurfaceFormat` requires `VK_COLOR_SPACE_SRGB_NONLINEAR_KHR` alongside the
sRGB format. The pair is passed to `vkCreateSwapchainKHR` as `imageFormat` and
`imageColorSpace`.

**Further reading**
- [sRGB (Wikipedia)](https://en.wikipedia.org/wiki/SRGB)
- [Vulkan Spec — Color Spaces](https://docs.vulkan.org/spec/latest/chapters/VK_KHR_surface/wsi.html#VkColorSpaceKHR)

______________________________________________________________________

## Chunk 4 — Render Pass and Framebuffers

## RENDER_PASS

**What it is**
A `VkRenderPass` — a *description* of a rendering operation, not the act of
rendering. It declares which attachments (colour, depth, …) are involved, what
format and sample count each has, what happens to each at the start (load) and
end (store) of the pass, and how the rendering is divided into subpasses. It
holds no images itself.

**Why it matters**
Vulkan asks you to describe the whole structure of a render up front so the
driver — especially on tile-based mobile GPUs — can schedule memory and bandwidth
optimally: it knows in advance that depth will be cleared and discarded, that
colour will be kept to present, and when layout transitions must happen. It is
the classic Vulkan trade: more declaration from you, fewer surprises (and more
speed) for the driver. The render pass depends only on formats, so it outlives
window resizes.

**How it appears in this project**
`RenderPass::createRenderPass` in `src/render_pass.cpp` builds it from a colour
attachment (the swapchain format, cleared then stored for presentation) and a
depth attachment, one subpass, and one external dependency. The handle lives in
`m_renderPass` and is created once for the program's lifetime.

**Further reading**
- [Vulkan Tutorial — Render passes](https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Render_passes)
- [Vulkan Guide — Render passes](https://docs.vulkan.org/guide/latest/render_passes.html)

______________________________________________________________________

## ATTACHMENT

**What it is**
One image slot that a render pass reads from or writes to — typically a colour
attachment (the picture being drawn) or a depth/stencil attachment (the depth
buffer). Each attachment is declared with a format, a sample count, and *load*
and *store* operations: what to do with its contents at the start and end of the
pass (clear, preserve, or discard).

**Why it matters**
Load/store ops are a surprisingly important performance lever. `LOAD_OP_CLEAR`
lets the GPU wipe the attachment cheaply instead of reading old contents;
`STORE_OP_DONT_CARE` lets it throw a result away rather than writing it back to
memory (which is exactly right for a depth buffer you never reuse). Declaring
attachments up front is what makes the render pass description complete.

**How it appears in this project**
The two `VkAttachmentDescription`s in `RenderPass::createRenderPass`: colour
(`LOAD_OP_CLEAR` → `STORE_OP_STORE`, ending in `PRESENT_SRC_KHR`) and depth
(`LOAD_OP_CLEAR` → `STORE_OP_DONT_CARE`). The values written by the clear ops are
`m_clearValues` (dark blue-grey for colour, 1.0 for depth).

**Further reading**
- [Vulkan Spec — Render Pass attachments](https://docs.vulkan.org/spec/latest/chapters/renderpass.html)
- [Khronos — Load/store ops and tile-based GPUs](https://developer.samsung.com/galaxy-gamedev/resources/articles/renderpasses.html)

______________________________________________________________________

## FRAMEBUFFER

**What it is**
A `VkFramebuffer` — the concrete binding between a render pass's attachment slots
and actual image views. The render pass says "slot 0 is a colour attachment of
this format"; the framebuffer says "slot 0 *is this specific image view*". It is
created for a particular render pass and a particular size.

**Why it matters**
Separating the description (render pass) from the concrete images (framebuffer)
means one render pass can be reused with many framebuffers. That is exactly what
the swapchain needs: the same render pass, but a different framebuffer per
swapchain image, since you draw into a different image each frame. Because a
framebuffer is tied to image size and to specific images, it must be rebuilt
whenever the swapchain is.

**How it appears in this project**
`RenderPass::createFramebuffers` makes one `VkFramebuffer` per swapchain image,
each binding that image's colour view plus the shared depth view. They are
rebuilt in `recreate()` after a resize, and stored in `m_framebuffers`.

**Further reading**
- [Vulkan Tutorial — Framebuffers](https://vulkan-tutorial.com/Drawing_a_triangle/Drawing/Framebuffers)
- [Vulkan Spec — Framebuffers](https://docs.vulkan.org/spec/latest/chapters/renderpass.html#_framebuffers)

______________________________________________________________________

## SUBPASS

**What it is**
A phase within a render pass that reads and writes some subset of the pass's
attachments. Every render pass has at least one subpass; it can have several that
run in sequence, where a later subpass can read what an earlier one wrote at the
same pixel. Subpass *dependencies* declare the ordering and memory visibility
between them (and with work outside the pass).

**Why it matters**
Multi-subpass passes let tile-based GPUs keep intermediate results in fast
on-chip tile memory instead of writing them out to main memory and reading them
back — the basis of efficient deferred shading on mobile. Even with a single
subpass, the dependency mechanism is how the render pass expresses when layout
transitions and clears are allowed to happen relative to other work.

**How it appears in this project**
`RenderPass::createRenderPass` defines exactly one `VkSubpassDescription` (one
colour + one depth attachment) and one `VkSubpassDependency` from
`VK_SUBPASS_EXTERNAL` that orders the colour/depth writes correctly. The project
never needs more than one subpass.

**Further reading**
- [Vulkan Spec — Subpasses](https://docs.vulkan.org/spec/latest/chapters/renderpass.html)
- [Vulkan Guide — Subpasses](https://docs.vulkan.org/guide/latest/render_passes.html)

______________________________________________________________________

## DEPTH_BUFFER

**What it is**
An off-screen image, the same size as the colour target, that stores a depth
value (distance from the camera) for each pixel. As triangles are rasterised, the
GPU compares each incoming fragment's depth against the stored value and keeps the
nearer one. It is never shown on screen.

**Why it matters**
It solves the *hidden surface problem*: how to draw overlapping 3D geometry so
that nearer surfaces correctly cover farther ones, regardless of the order the
triangles are submitted. Without it you would have to sort everything back-to-
front yourself (the painter's algorithm) and still get it wrong for intersecting
shapes. The depth buffer makes correct occlusion automatic and per-pixel.
See Glossary: DEPTH_TESTING, PAINTERS_ALGORITHM

**How it appears in this project**
`RenderPass::createDepthResources` creates the depth `VkImage` (device-local,
`DEPTH_STENCIL_ATTACHMENT` usage), allocates and binds its memory, and makes a
depth `VkImageView`. The format is chosen by `chooseDepthFormat` (prefers
`D32_SFLOAT`). It is attached to every framebuffer and cleared to 1.0 each pass.

**Further reading**
- [Vulkan Tutorial — Depth buffering](https://vulkan-tutorial.com/Depth_buffering)
- [Z-buffering (Wikipedia)](https://en.wikipedia.org/wiki/Z-buffering)

______________________________________________________________________

## DEPTH_TESTING

**What it is**
The per-fragment test the GPU performs using the depth buffer: for each fragment
a triangle produces, compare its depth to the value already stored at that pixel;
if the fragment is nearer (passes the comparison, usually "less than"), shade it
and update the stored depth, otherwise discard it.

**Why it matters**
Depth testing is what actually *uses* the depth buffer to resolve occlusion. It
runs in hardware, per fragment, for free relative to doing it yourself, and it is
order-independent for opaque geometry. The comparison direction and the clear
value must agree: this project clears depth to 1.0 (far) and keeps nearer
fragments, so closer surfaces win.

**How it appears in this project**
The depth attachment and clear value are set up here in `src/render_pass.cpp`;
the pipeline that actually enables the depth test (`VkPipelineDepthStencilStateCreateInfo`)
is configured in Chunk 5. Conceptually the two halves meet at the depth buffer.

**Further reading**
- [Vulkan Spec — Depth test](https://docs.vulkan.org/spec/latest/chapters/fragops.html#fragops-depth)
- [LearnOpenGL — Depth testing](https://learnopengl.com/Advanced-OpenGL/Depth-testing)

______________________________________________________________________

## PAINTERS_ALGORITHM

**What it is**
A way to render overlapping objects by sorting them back-to-front and drawing them
in that order, so nearer objects paint over farther ones — like a painter laying
down the background before the foreground.

**Why it matters**
It is the conceptual predecessor to the depth buffer, and understanding its
failures explains why depth buffers exist. It needs a global sort every frame
(expensive as scenes grow) and it simply cannot handle mutually overlapping or
intersecting geometry — there is no correct order for two triangles that pierce
each other. The depth buffer replaces it with a cheap per-pixel test. (It is not
obsolete, though: transparent objects, which a depth buffer alone cannot order,
are still commonly drawn back-to-front.)

**How it appears in this project**
Not used in code — the project relies entirely on the depth buffer. It is
documented as the contrast that motivates DEPTH_BUFFER and DEPTH_TESTING.

**Further reading**
- [Painter's algorithm (Wikipedia)](https://en.wikipedia.org/wiki/Painter%27s_algorithm)
- [Hidden-surface determination (Wikipedia)](https://en.wikipedia.org/wiki/Hidden-surface_determination)

______________________________________________________________________

## Z_FIGHTING

**What it is**
A flickering, shimmering artefact where two surfaces are at almost the same depth
and the depth buffer cannot reliably decide which is in front. From frame to frame
(or pixel to pixel) the winner flips, producing a noisy stipple along the
coincident surfaces.

**Why it matters**
It is the most common depth-buffer artefact, and its cause teaches how depth
precision works: depth values are stored with finite precision that is unevenly
distributed — with a perspective projection, most precision sits near the camera
and very little far away. Pulling the near plane too close or pushing the far
plane too far stretches precision thin and triggers z-fighting in the distance.
Mitigations include a tighter near/far range, a float depth buffer, or a reversed-
Z depth setup.

**How it appears in this project**
Not directly visible yet (nothing is drawn), but the choices that fight it are
made here and in Chunk 8: a 32-bit float depth format (`chooseDepthFormat`) and,
later, a sensible near/far range in the perspective projection.

**Further reading**
- [Z-fighting (Wikipedia)](https://en.wikipedia.org/wiki/Z-fighting)
- [NVIDIA — Depth precision visualized](https://developer.nvidia.com/content/depth-precision-visualized)

______________________________________________________________________

## Chunk 5 — Shaders and the Graphics Pipeline

## SHADER

**What it is**
A small program that runs on the GPU, in parallel across many data elements, as
one stage of the graphics pipeline. The two in this project are the vertex shader
(runs per vertex) and the fragment shader (runs per fragment). They are written
in GLSL and compiled to SPIR-V.

**Why it matters**
Shaders are the *programmable* part of an otherwise fixed pipeline: they are where
you express how vertices are positioned and how surfaces are coloured and lit. The
GPU runs thousands of shader invocations simultaneously, which is why graphics
work is expressed as tiny per-element programs rather than loops on the CPU.

**How it appears in this project**
`shaders/mesh.vert` and `shaders/mesh.frag`, compiled to SPIR-V by the build and
loaded in `ShaderPipeline` (`src/shader_pipeline.cpp`) as `VkShaderModule`s that
become the pipeline's programmable stages.

**Further reading**
- [Vulkan Tutorial — Shader modules](https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Shader_modules)
- [LearnOpenGL — Shaders](https://learnopengl.com/Getting-started/Shaders)

______________________________________________________________________

## VERTEX_SHADER

**What it is**
The pipeline stage that runs once per input vertex. Its required output is the
vertex's clip-space position via the built-in `gl_Position`; it may also pass
extra per-vertex data (colour, normal, texture coordinates) down to later stages.

**Why it matters**
It is where geometry is transformed: from the model's own coordinates into the
final on-screen position. In later chunks this is where the model-view-projection
matrix is applied, turning a 3D model and a camera into 2D screen positions. Run
per vertex, it is far cheaper than doing the same work per pixel.

**How it appears in this project**
`shaders/mesh.vert`. In Chunk 5 it outputs three hardcoded corners selected by
`gl_VertexIndex`; from Chunk 7 it reads real vertex attributes, and from Chunk 8
it applies the MVP transform.

**Further reading**
- [Vulkan Spec — Vertex shaders](https://docs.vulkan.org/spec/latest/chapters/pipelines.html)
- [LearnOpenGL — Hello Triangle](https://learnopengl.com/Getting-started/Hello-Triangle)

______________________________________________________________________

## FRAGMENT_SHADER

**What it is**
The pipeline stage that runs once per fragment — each candidate pixel a triangle
covers — and outputs that fragment's colour (and implicitly contributes its
depth). Its inputs are values interpolated across the triangle from the vertex
shader's outputs.

**Why it matters**
It is where surface appearance is decided: texturing, lighting, and colour all
happen here. Because there are vastly more fragments than vertices, the fragment
shader is usually the pipeline's heaviest stage, so what you do per fragment
matters most for performance.

**How it appears in this project**
`shaders/mesh.frag`. In Chunk 5 it outputs one flat orange colour; Chunk 11 turns
it into a lit shader computing diffuse lighting per fragment.

**Further reading**
- [Vulkan Tutorial — Fragment shader](https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Shader_modules)
- [LearnOpenGL — Basic Lighting](https://learnopengl.com/Lighting/Basic-Lighting)

______________________________________________________________________

## FRAGMENT

**What it is**
A single candidate pixel produced by rasterising a triangle: it has a screen
position, an interpolated depth, and interpolated attributes, and it is processed
by the fragment shader. A fragment becomes a pixel only if it survives the depth
test and other per-fragment operations.

**Why it matters**
The distinction between *fragment* and *pixel* matters: many fragments can map to
the same pixel across overlapping triangles, and only one (or a blend) ends up
stored. Understanding fragments explains where per-pixel cost comes from and why
depth testing and overdraw are performance topics.

**How it appears in this project**
Not a code symbol — it is the unit the fragment shader (`shaders/mesh.frag`)
operates on, and what the depth test in the pipeline keeps or discards.

**Further reading**
- [Fragment (computer graphics) (Wikipedia)](https://en.wikipedia.org/wiki/Fragment_(computer_graphics))
- [Vulkan Spec — Fragment operations](https://docs.vulkan.org/spec/latest/chapters/fragops.html)

______________________________________________________________________

## GLSL

**What it is**
The OpenGL Shading Language — a C-like high-level language for writing shaders.
Vulkan does not consume GLSL directly; the GLSL source is compiled to SPIR-V
bytecode first (here by `glslc`).

**Why it matters**
It is the human-readable form shaders are authored in: familiar syntax, vector
and matrix types, and built-ins like `gl_Position`. Keeping authoring in GLSL but
shipping SPIR-V gives the best of both — readable source, portable bytecode.

**How it appears in this project**
`shaders/mesh.vert` and `shaders/mesh.frag` are GLSL (note the `#version 450`).
`CMakeLists.txt` compiles them with `Vulkan::glslc`.

**Further reading**
- [GLSL (Wikipedia)](https://en.wikipedia.org/wiki/OpenGL_Shading_Language)
- [Khronos GLSL specification](https://www.khronos.org/registry/OpenGL/specs/gl/GLSLangSpec.4.60.html)

______________________________________________________________________

## SPIR_V

**What it is**
A binary intermediate representation (bytecode) for shaders that Vulkan consumes
directly. GLSL (or HLSL) is compiled ahead of time into SPIR-V, which the GPU
driver then turns into native machine code when the pipeline is created.

**Why it matters**
Shipping bytecode instead of source means the driver does not have to embed a full
GLSL compiler, compilation is more predictable and faster at load time, and tools
can target one stable format from many source languages. It is a key part of
Vulkan's "explicit, low-overhead" philosophy.

**How it appears in this project**
`glslc` produces `shaders/compiled/mesh.vert.spv` and `mesh.frag.spv` during the
build. `ShaderPipeline::readFile` loads the bytecode and `createShaderModule`
wraps it in a `VkShaderModule`.

**Further reading**
- [SPIR-V (Wikipedia)](https://en.wikipedia.org/wiki/Standard_Portable_Intermediate_Representation)
- [Khronos — SPIR-V overview](https://www.khronos.org/spir/)

______________________________________________________________________

## PROGRAMMABLE_PIPELINE_STAGES

**What it is**
The stages of the graphics pipeline that run your shader code — principally the
vertex shader and fragment shader (plus optional geometry, tessellation, and mesh
stages). They sit between and around the fixed-function stages.

**Why it matters**
The split between programmable and fixed-function stages is the core shape of a
modern GPU pipeline: fixed-function hardware does the heavy, standardised work
(rasterisation, blending) at full speed, while programmable stages let you define
the parts that vary between applications (transforms, shading).

**How it appears in this project**
The two `VkPipelineShaderStageCreateInfo` entries in `ShaderPipeline` declare the
vertex and fragment stages and which module/entry point each uses.

**Further reading**
- [Vulkan Tutorial — Graphics pipeline introduction](https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Introduction)
- [Render pipeline overview (Khronos)](https://docs.vulkan.org/guide/latest/pipelines.html)

______________________________________________________________________

## FIXED_FUNCTION_PIPELINE

**What it is**
The non-programmable stages of the pipeline, configured rather than coded: input
assembly, rasterisation, depth/stencil testing, colour blending, and the
viewport/scissor transform. You set their parameters via state structs at pipeline
creation.

**Why it matters**
These operations are the same for every application and map to dedicated, highly
optimised hardware, so they are exposed as configuration, not code. Knowing which
parts are fixed-function (and merely configured) versus programmable tells you
where you can change behaviour and where you can only tune parameters.

**How it appears in this project**
All the `VkPipeline*StateCreateInfo` structs in `ShaderPipeline::ShaderPipeline`:
input assembly, rasteriser, multisample, depth/stencil, colour blend, viewport.

**Further reading**
- [Vulkan Tutorial — Fixed functions](https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions)
- [Fixed-function (Wikipedia)](https://en.wikipedia.org/wiki/Fixed-function)

______________________________________________________________________

## GRAPHICS_PIPELINE

**What it is**
A `VkPipeline` — a single immutable object that bundles *everything* needed to
turn draw calls into pixels: the shader stages plus every fixed-function setting
(vertex input, input assembly, rasteriser, depth/stencil, blending, viewport
policy) and a reference to the render pass it targets.

**Why it matters**
Vulkan bakes all this state into one object up front so that, at draw time, the
driver has no decisions left to make — binding a pipeline is cheap and there are
no surprise recompiles mid-frame (the stalls that plagued older APIs). The cost is
that almost any state change means building a different pipeline; the standard
escape hatch is *dynamic state* for a few frequently-changing settings.

**How it appears in this project**
Built in `ShaderPipeline::ShaderPipeline` via `vkCreateGraphicsPipelines`. Viewport
and scissor are left dynamic so the one pipeline survives window resizes; it is
bound each frame with `vkCmdBindPipeline` in `Renderer::recordCommandBuffer`.

**Further reading**
- [Vulkan Tutorial — Conclusion (pipeline)](https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Conclusion)
- [Vulkan Guide — Pipelines](https://docs.vulkan.org/guide/latest/pipelines.html)

______________________________________________________________________

## PIPELINE_LAYOUT

**What it is**
A `VkPipelineLayout` — the declaration of what external resources a pipeline's
shaders can access: which descriptor set layouts (uniforms, textures) and which
push-constant ranges. It is the interface between the pipeline and the data bound
to it.

**Why it matters**
Separating the layout from the pipeline lets the driver know the shape of the
shader's inputs independently of the shader code, and lets multiple pipelines
share a layout. It is required even when empty, because the pipeline must declare
its (possibly nonexistent) resource interface.

**How it appears in this project**
`ShaderPipeline` creates an **empty** `VkPipelineLayout` — the hardcoded triangle
reads nothing external. Chunk 8 fills it in with a descriptor set layout for the
uniform buffer (MVP matrices).

**Further reading**
- [Vulkan Spec — Pipeline layouts](https://docs.vulkan.org/spec/latest/chapters/descriptorsets.html#descriptorsets-pipelinelayout)
- [Vulkan Guide — Descriptor sets](https://docs.vulkan.org/guide/latest/descriptor_sets.html)

______________________________________________________________________

## RASTERISATION

**What it is**
The fixed-function step that converts a primitive (here, a triangle defined by
its clip-space vertices) into a set of fragments — figuring out which pixels the
triangle covers and interpolating the per-vertex outputs across them.

**Why it matters**
It is the bridge from geometry to pixels, the moment 3D becomes 2D coverage. It is
also where attribute interpolation happens, which is why a colour or normal set
per vertex produces a smooth gradient across the triangle's interior.

**How it appears in this project**
Performed by the GPU between the vertex and fragment shaders. Its parameters are
the `VkPipelineRasterizationStateCreateInfo` (and input assembly topology) in
`ShaderPipeline`. The `vkCmdDraw(cmd, 3, …)` call feeds it the triangle.

**Further reading**
- [Rasterisation (Wikipedia)](https://en.wikipedia.org/wiki/Rasterisation)
- [Scratchapixel — Rasterization](https://www.scratchapixel.com/lessons/3d-basic-rendering/rasterization-practical-implementation/overview-rasterization-algorithm.html)

______________________________________________________________________

## RASTERISER

**What it is**
The fixed-function hardware/stage that performs rasterisation, configured by a
`VkPipelineRasterizationStateCreateInfo`. Its settings include polygon fill mode,
face culling, front-face winding, and line width.

**Why it matters**
Its configuration decides which fragments even reach the fragment shader. Face
culling here (disabled in Chunk 5, enabled in Chunk 7) is a major optimisation:
skipping triangles that face away from the camera roughly halves fragment work
for closed meshes.

**How it appears in this project**
The `rasteriser` struct in `ShaderPipeline`: `POLYGON_MODE_FILL`, `CULL_MODE_NONE`
(no culling yet, since the hardcoded triangle's winding is not guaranteed),
counter-clockwise front face, line width 1.0.

**Further reading**
- [Vulkan Spec — Rasterization](https://docs.vulkan.org/spec/latest/chapters/primsrast.html)
- [Vulkan Tutorial — Fixed functions](https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions)

______________________________________________________________________

## CLIP_SPACE

**What it is**
The coordinate space the vertex shader outputs into via `gl_Position`, a 4D
homogeneous coordinate (x, y, z, w). The GPU clips primitives against the view
volume in this space, then divides x, y, z by w (the "perspective divide") to get
normalised device coordinates.

**Why it matters**
It is the agreed hand-off point between your shader and the fixed-function
pipeline: whatever transforms you apply, the vertex shader's job is to land
vertices in clip space. The w component is what makes perspective possible — the
divide by w is what makes distant things smaller (covered in Chunk 8).

**How it appears in this project**
`shaders/mesh.vert` writes `gl_Position` directly in clip space. In Chunk 5 the
hardcoded positions already have w = 1, so clip space equals NDC; Chunk 8
introduces a real projection matrix that produces a meaningful w.

**Further reading**
- [Vulkan coordinate systems (Khronos guide)](https://docs.vulkan.org/guide/latest/mapping_data_to_shaders.html)
- [LearnOpenGL — Coordinate systems](https://learnopengl.com/Getting-started/Coordinate-Systems)

______________________________________________________________________

## NDC_SPACE

**What it is**
Normalised device coordinates — the coordinate space after the perspective divide.
In Vulkan, x and y run from -1 to +1 across the framebuffer (with +Y pointing
*down*), and z runs from 0 to 1 (near to far). The viewport transform then maps
NDC to actual pixel coordinates.

**Why it matters**
NDC is the resolution-independent space in which "where on screen" is finally
defined; everything before it is about getting vertices here. Vulkan's
conventions differ from OpenGL's (OpenGL's Y points up and z is -1..1), which is a
classic source of upside-down images and depth bugs when porting.

**How it appears in this project**
The triangle's hardcoded coordinates in `shaders/mesh.vert` are effectively NDC
(w = 1). The +Y-down convention is why the vertex marked "top" uses a negative Y.
`Renderer`'s viewport maps NDC to the swapchain extent.

**Further reading**
- [Vulkan NDC and viewport (Sascha Willems)](https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport/)
- [Vulkan Spec — Coordinate transformations](https://docs.vulkan.org/spec/latest/chapters/vertexpostproc.html)

______________________________________________________________________

## VIEWPORT

**What it is**
The transform that maps normalised device coordinates to a rectangular region of
the framebuffer in pixels, including the depth range mapping (minDepth..maxDepth).
A `VkViewport` specifies x, y, width, height, and the depth range.

**Why it matters**
It is what ties resolution-independent NDC to actual pixels, so it must match the
render target's size — which is exactly why it changes on every window resize.
Making it dynamic state avoids rebuilding the pipeline each time the window
changes size.

**How it appears in this project**
Declared (count 1) but left dynamic in `ShaderPipeline`, then set each frame to
the swapchain extent with `vkCmdSetViewport` in `Renderer::recordCommandBuffer`.

**Further reading**
- [Vulkan Spec — Controlling the viewport](https://docs.vulkan.org/spec/latest/chapters/vertexpostproc.html#vertexpostproc-viewport)
- [Vulkan Tutorial — Fixed functions](https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions)

______________________________________________________________________

## SCISSOR

**What it is**
A rectangle that further restricts where fragments may be written: anything
outside the scissor rectangle is discarded, regardless of the viewport. A
`VkRect2D` defines it.

**Why it matters**
Where the viewport *scales* geometry into a region, the scissor simply *clips*
output to a rectangle — useful for UI panels, split-screen, or limiting redraw.
For a full-window render the scissor just matches the whole framebuffer, but it is
a mandatory part of the pipeline's viewport state.

**How it appears in this project**
Declared (count 1) and left dynamic in `ShaderPipeline`, set each frame to cover
the full swapchain extent with `vkCmdSetScissor` in `Renderer::recordCommandBuffer`.

**Further reading**
- [Vulkan Spec — Scissor test](https://docs.vulkan.org/spec/latest/chapters/fragops.html#fragops-scissor)
- [Vulkan Tutorial — Fixed functions](https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions)

______________________________________________________________________

## COLOUR_BLENDING

**What it is**
The fixed-function step that combines a fragment shader's output colour with the
colour already in the attachment, according to a blend equation and factors. When
disabled, the new colour simply replaces the old one.

**Why it matters**
Blending is how transparency and many compositing effects are achieved (e.g.
`src_alpha`/`one_minus_src_alpha` for alpha blending). For opaque geometry it is
turned off so colours overwrite directly, which is both correct and faster.

**How it appears in this project**
`ShaderPipeline`'s `blendAttachment` has `blendEnable = VK_FALSE` (opaque triangle)
with all four colour channels writable. Transparency is out of scope for this
project but the slot is configured.

**Further reading**
- [Vulkan Tutorial — Color blending](https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions)
- [LearnOpenGL — Blending](https://learnopengl.com/Advanced-OpenGL/Blending)

______________________________________________________________________

## Chunk 5 (early) — Frame-loop concepts

> These concepts belong to the spec's Chunk 6 ("Command Buffers and
> Synchronisation"), but the minimal frame loop was brought forward into Chunk 5
> so the triangle could actually be shown (see `docs/DECISIONS.md`). Chunk 6
> expands them with frames-in-flight, pipeline barriers, and layout transitions.

## COMMAND_BUFFER

**What it is**
A `VkCommandBuffer` — a recorded list of GPU commands (begin render pass, bind
pipeline, set viewport, draw, …) that is built on the CPU and then submitted to a
queue to be executed by the GPU.

**Why it matters**
Vulkan does not execute commands the moment you call them; you *record* them into
a buffer and *submit* the buffer. This separation lets work be recorded in advance
(even on multiple threads) and replayed efficiently, and it is the foundation of
Vulkan's low CPU overhead — the opposite of OpenGL's immediate-mode calls.

**How it appears in this project**
`Renderer` allocates one primary command buffer and re-records it each frame in
`recordCommandBuffer` (render pass + pipeline + viewport/scissor + `vkCmdDraw`),
then submits it in `drawFrame`.

**Further reading**
- [Vulkan Tutorial — Command buffers](https://vulkan-tutorial.com/Drawing_a_triangle/Drawing/Command_buffers)
- [Vulkan Guide — Command buffers](https://docs.vulkan.org/guide/latest/command_buffers.html)

______________________________________________________________________

## COMMAND_POOL

**What it is**
A `VkCommandPool` — the allocator that command buffers are created from. A pool is
tied to one queue family, and command buffers from it can only be submitted to
that family's queues.

**Why it matters**
Command buffers are allocated in bulk from a pool so the driver can manage their
memory cheaply; pools also make resetting many buffers at once efficient. Tying a
pool to a queue family is what guarantees a recorded buffer is compatible with the
queue it is submitted to.

**How it appears in this project**
`Renderer::createCommandResources` creates a pool for the graphics queue family
with the `RESET_COMMAND_BUFFER` flag (so the single buffer can be re-recorded each
frame), and allocates the command buffer from it.

**Further reading**
- [Vulkan Spec — Command pools](https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html#commandbuffers-pools)
- [Vulkan Tutorial — Command pools](https://vulkan-tutorial.com/Drawing_a_triangle/Drawing/Command_buffers)

______________________________________________________________________

## SEMAPHORE

**What it is**
A `VkSemaphore` — a synchronisation primitive that orders work **between GPU
operations**. A queue operation can be told to wait on a semaphore before starting
and to signal one when it finishes; the CPU is not involved.

**Why it matters**
The frame's steps run on the GPU and must not overlap incorrectly: presentation
must not start before rendering finishes, and rendering must not start before the
swapchain image is available. Semaphores express exactly these GPU-to-GPU
orderings without stalling the CPU.

**How it appears in this project**
`Renderer` uses `m_imageAvailable` (acquire → render ordering) and a per-image
`m_renderFinished` (render → present ordering). The submit waits on the first and
signals the second; the present waits on the second.

**Further reading**
- [Vulkan Tutorial — Semaphores](https://vulkan-tutorial.com/Drawing_a_triangle/Drawing/Rendering_and_presentation)
- [Vulkan Guide — Synchronization](https://docs.vulkan.org/guide/latest/synchronization.html)

______________________________________________________________________

## FENCE

**What it is**
A `VkFence` — a synchronisation primitive that lets the **CPU wait for the GPU**.
A queue submit can signal a fence when its work completes, and the CPU can block
on (or poll) that fence.

**Why it matters**
Semaphores order GPU work but the CPU cannot see them; a fence is how the CPU
knows a frame's GPU work is done so it can safely reuse that frame's resources
(command buffer, etc.). It is the GPU→CPU half of synchronisation.

**How it appears in this project**
`Renderer`'s `m_inFlight` fence. It is created already signalled (so the first
frame does not deadlock), waited on at the top of `drawFrame`, reset, and signalled
by `vkQueueSubmit` when the frame's GPU work completes.

**Further reading**
- [Vulkan Tutorial — Frames in flight](https://vulkan-tutorial.com/Drawing_a_triangle/Drawing/Frames_in_flight)
- [Vulkan Guide — Synchronization](https://docs.vulkan.org/guide/latest/synchronization.html)

______________________________________________________________________

## GPU_CPU_SYNC

**What it is**
The umbrella idea that the CPU and GPU run asynchronously and their work must be
explicitly coordinated: GPU-to-GPU ordering with semaphores, GPU-to-CPU signalling
with fences, and whole-device draining with `vkDeviceWaitIdle`.

**Why it matters**
The CPU records and submits frames while the GPU is still executing earlier ones;
without explicit synchronisation you would read or destroy resources the GPU is
still using, causing corruption or crashes. Vulkan makes this coordination the
application's responsibility (OpenGL hid it), which is both its burden and its
performance advantage.

**How it appears in this project**
The whole of `Renderer::drawFrame` (fence wait + semaphores) is the per-frame
case; `VulkanContext::waitIdle` is the coarse case used on resize and at shutdown.

**Further reading**
- [Vulkan Guide — Synchronization](https://docs.vulkan.org/guide/latest/synchronization.html)
- [Yet another blog explaining Vulkan synchronization (Maister)](https://themaister.net/blog/2019/08/14/yet-another-blog-explaining-vulkan-synchronization/)

______________________________________________________________________

## SUBMISSION_QUEUE

**What it is**
A queue (here, the graphics queue) that command buffers are submitted to for the
GPU to execute. `vkQueueSubmit` hands one or more command buffers to a queue,
along with the semaphores to wait on and signal; `vkQueuePresentKHR` submits a
present request to the present queue.

**Why it matters**
The queue is the actual hand-off point from CPU to GPU. Batching work into queue
submissions (rather than per-command calls) is central to Vulkan's efficiency, and
the semaphores/fences attached to a submission are how its execution is ordered
relative to everything else.

**How it appears in this project**
`Renderer::drawFrame` calls `vkQueueSubmit` on `m_context.graphicsQueue()` and
`vkQueuePresentKHR` on `m_context.presentQueue()`.

**Further reading**
- [Vulkan Spec — Queue submission](https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html#commandbuffers-submission)
- [Vulkan Guide — Queues](https://docs.vulkan.org/guide/latest/queues.html)

______________________________________________________________________

## FRAME_LOOP

**What it is**
The repeating per-frame cycle a real-time renderer runs: **acquire** a swapchain
image, **record** a command buffer that draws into it, **submit** that work to a
queue, and **present** the result — all coordinated by semaphores and a fence.

**Why it matters**
It is the concrete realisation of the event loop for a GPU renderer, and the place
all the synchronisation pieces come together. Getting its ordering and resize
handling right is what separates a renderer that runs smoothly from one that
flickers, tears, or crashes on resize.

**How it appears in this project**
`Renderer::drawFrame` implements one iteration; the `while (running)` loop in
`src/main.cpp` calls it each frame and triggers swapchain recreation when it
reports the swapchain is out of date.

**Further reading**
- [Vulkan Tutorial — Rendering and presentation](https://vulkan-tutorial.com/Drawing_a_triangle/Drawing/Rendering_and_presentation)
- [Game Programming Patterns — Game Loop](https://gameprogrammingpatterns.com/game-loop.html)

______________________________________________________________________

## Chunk 6 — Command Buffers and Synchronisation (completing the set)

> The frame loop itself was built in Chunk 5 (see the entries above). Chunk 6
> raises it to two frames in flight and completes the synchronisation concepts.

## SYNCHRONISATION

**What it is**
The umbrella discipline of controlling the order and visibility of work across the
GPU and CPU: which operations must finish before others start, and when one
operation's writes become visible to another. Vulkan's tools for it are
semaphores (GPU↔GPU), fences (GPU→CPU), pipeline barriers and subpass
dependencies (ordering + memory visibility within the GPU), and events.

**Why it matters**
Vulkan executes work asynchronously and does almost nothing implicitly, so
*correctness* depends on the application stating every ordering and visibility
requirement explicitly. Get it wrong and you get races: flickering, corruption,
or crashes that may only appear on some hardware. This explicitness is also the
source of Vulkan's performance — the driver never inserts conservative,
just-in-case stalls the way older APIs did.

**How it appears in this project**
Spread across the renderer and render pass: the per-frame fence + two semaphores
in `Renderer::drawFrame` (GPU↔GPU and GPU→CPU ordering), the subpass dependency
in `RenderPass::createRenderPass` (intra-GPU ordering + layout transitions), and
`VulkanContext::waitIdle` (the coarse, whole-device stall used on resize/shutdown).

**Further reading**
- [Vulkan Guide — Synchronization](https://docs.vulkan.org/guide/latest/synchronization.html)
- [Khronos — Synchronization examples](https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples)

______________________________________________________________________

## FRAMES_IN_FLIGHT

**What it is**
The number of frames the CPU is allowed to be working on before it must wait for
the GPU to catch up. With N frames in flight, there are N independent copies of
the per-frame resources (command buffer, image-available semaphore, in-flight
fence) so frame K+1 can be recorded while frame K is still executing on the GPU.

**Why it matters**
With only one frame in flight the CPU records a frame, submits it, then sits idle
waiting for the GPU to finish before starting the next — the two take turns and
both are underused. Allowing a second frame keeps both busy and smooths out
frame-time spikes. Going higher adds little throughput but increases input
latency (the CPU runs further ahead of what is shown), so two is the common
default.

**How it appears in this project**
`Renderer::kMaxFramesInFlight = 2`. Command buffers, `m_imageAvailable`, and
`m_inFlight` are arrays of that size; `m_currentFrame` cycles `0,1,0,1,…` and
indexes them in `drawFrame`. (The `renderFinished` semaphores are per swapchain
*image*, a separate count.)

**Further reading**
- [Vulkan Tutorial — Frames in flight](https://vulkan-tutorial.com/Drawing_a_triangle/Drawing/Frames_in_flight)
- [Vulkan Guide — Swapchain & frame pacing](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html)

______________________________________________________________________

## PIPELINE_BARRIER

**What it is**
An explicit synchronisation command (`vkCmdPipelineBarrier`) inserted into a
command buffer that says: work in these pipeline stages before the barrier must
complete, and their memory writes become visible, before work in these stages
after it begins. It can also carry image-layout transitions and queue-family
ownership transfers.

**Why it matters**
It is the fine-grained, in-command-buffer tool for GPU-side ordering and memory
visibility — the thing you reach for when one GPU operation depends on another's
output (e.g. render-to-texture then sample it). Subpass dependencies inside a
render pass are essentially the render pass's built-in form of the same mechanism.

**How it appears in this project**
No *explicit* `vkCmdPipelineBarrier` is used: the only ordering and layout work
needed is handled by the render pass's subpass dependency and attachment layouts
(`RenderPass::createRenderPass`). The concept is documented because it is the
general mechanism, and any render-to-texture or compute step added later would use
it directly. See Glossary: IMAGE_LAYOUT_TRANSITION, SUBPASS

**Further reading**
- [Vulkan Guide — Synchronization (barriers)](https://docs.vulkan.org/guide/latest/synchronization.html)
- [Vulkan Spec — Pipeline barriers](https://docs.vulkan.org/spec/latest/chapters/synchronization.html#synchronization-pipeline-barriers)

______________________________________________________________________

## IMAGE_LAYOUT_TRANSITION

**What it is**
Changing a `VkImage` from one *layout* to another. A layout is the internal memory
arrangement the GPU expects an image to be in for a particular use — e.g.
`COLOR_ATTACHMENT_OPTIMAL` for rendering into it, `PRESENT_SRC_KHR` for handing it
to the display, `SHADER_READ_ONLY_OPTIMAL` for sampling it as a texture.
Transitions happen via pipeline barriers or, inside a render pass, automatically.

**Why it matters**
The same image is laid out differently in memory depending on how it is being
used, and using it in the wrong layout is undefined behaviour. Making layouts
explicit lets the GPU store images in the most efficient form for each use (for
example, compressed for rendering) instead of one compromise format. It is one of
the more Vulkan-specific concepts with no direct OpenGL equivalent.

**How it appears in this project**
Handled implicitly by the render pass. Each attachment declares an `initialLayout`
and `finalLayout` (e.g. the colour attachment goes `UNDEFINED` →
`COLOR_ATTACHMENT_OPTIMAL` during the pass → `PRESENT_SRC_KHR` at the end), and the
subpass dependency schedules those transitions. No manual transition code is
needed in this project.

**Further reading**
- [Vulkan Tutorial — Image layout transitions](https://vulkan-tutorial.com/Texture_mapping/Images)
- [Vulkan Spec — Image layouts](https://docs.vulkan.org/spec/latest/chapters/resources.html#resources-image-layouts)

______________________________________________________________________

## Chunk 7 — Vertex and Index Buffers

## MESH

**What it is**
In 3D graphics, a mesh is a surface built from many small flat polygons — almost
always triangles — defined by a set of vertices and a list of which vertices form
each triangle. It is the standard way to represent the shape of an object.

**Why it matters**
GPUs are built to rasterise triangles extremely fast, so essentially all real-time
3D geometry is expressed as triangle meshes, however the object was originally
modelled. Understanding "a model is just vertices + triangles" demystifies
everything from a cube to a character: they differ only in how many vertices and
how they are connected.

**How it appears in this project**
The `Mesh` class (`src/mesh.h` / `src/mesh.cpp`) owns the GPU buffers for one
mesh. `Mesh::cube` builds a 24-vertex, 12-triangle cube; Chunk 9 will populate the
same class from an OBJ file.

**Further reading**
- [Polygon mesh (Wikipedia)](https://en.wikipedia.org/wiki/Polygon_mesh)
- [Triangle mesh (Wikipedia)](https://en.wikipedia.org/wiki/Triangle_mesh)

______________________________________________________________________

## VERTEX_ATTRIBUTES

**What it is**
The per-vertex pieces of data a vertex carries into the pipeline. Position is the
essential one; common others are normal, texture coordinate (UV), colour, and
tangent. Each attribute has a location, a format, and an offset within the vertex.

**Why it matters**
Attributes are the inputs the vertex shader reads and (interpolated) the fragment
shader receives — they are how per-vertex information drives shading. The pipeline
must be told each attribute's layout so the GPU can fetch it from the vertex
buffer; a mismatch between the struct, the attribute descriptions, and the shader
inputs is a frequent bug.

**How it appears in this project**
The `Vertex` struct (position, normal, uv) and `Vertex::attributeDescriptions()`
in `src/mesh.cpp`, which map each field to a shader `location` and `format`. The
pipeline consumes them in `ShaderPipeline`; the vertex shader declares matching
`layout(location = …) in` variables.

**Further reading**
- [Vulkan Tutorial — Vertex input description](https://vulkan-tutorial.com/Vertex_buffers/Vertex_input_description)
- [Vulkan Spec — Vertex input](https://docs.vulkan.org/spec/latest/chapters/fxvertex.html)

______________________________________________________________________

## VERTEX_BUFFER

**What it is**
A `VkBuffer` holding the array of vertices for a mesh. It is bound before drawing,
and the pipeline's binding/attribute descriptions tell the GPU how to read one
vertex out of it (stride and per-attribute layout).

**Why it matters**
It is the GPU-side home of geometry. Putting vertices in a buffer (rather than
hardcoding them in a shader, as Chunk 5 did) is what lets a program draw arbitrary
models, and storing it in device-local memory makes the GPU's per-vertex fetches
fast.

**How it appears in this project**
`Mesh::m_vertexBuffer`, created device-local and filled via staging in
`Mesh::uploadDeviceLocal`, and bound with `vkCmdBindVertexBuffers` in
`Mesh::bindAndDraw`. Its layout is described to the pipeline by
`Vertex::bindingDescription()` / `attributeDescriptions()`.

**Further reading**
- [Vulkan Tutorial — Vertex buffer creation](https://vulkan-tutorial.com/Vertex_buffers/Vertex_buffer_creation)
- [Vulkan Spec — Buffers](https://docs.vulkan.org/spec/latest/chapters/resources.html#resources-buffers)

______________________________________________________________________

## INDEX_BUFFER

**What it is**
A `VkBuffer` of integers that index into the vertex buffer, specifying which
vertices form each triangle. Drawing with `vkCmdDrawIndexed` reads indices in
order, three per triangle.

**Why it matters**
Vertices are usually shared between many triangles (a cube corner touches three
faces; a smooth surface, six or more). Without indices you would duplicate each
shared vertex once per triangle. Indices let you store each unique vertex once and
just reference it — saving memory and bandwidth, and letting the GPU's post-
transform vertex cache skip re-shading reused vertices.

**How it appears in this project**
`Mesh::m_indexBuffer` (36 indices for the cube's 12 triangles), bound with
`vkCmdBindIndexBuffer` and drawn with `vkCmdDrawIndexed` in `Mesh::bindAndDraw`.

**Further reading**
- [Vulkan Tutorial — Index buffer](https://vulkan-tutorial.com/Vertex_buffers/Index_buffer)
- [Vertex cache / indexed rendering (Wikipedia)](https://en.wikipedia.org/wiki/Polygon_mesh#Vertex-vertex_meshes)

______________________________________________________________________

## GPU_MEMORY

**What it is**
Memory that the GPU uses for its resources — buffers and images. On a discrete
GPU this is separate physical memory (VRAM) across a bus from system RAM; on an
integrated GPU (like the Apple M2) it is shared with system RAM but still exposed
through Vulkan's memory model. You allocate it explicitly with `vkAllocateMemory`
and bind resources to it.

**Why it matters**
Vulkan makes memory management the application's job — unlike OpenGL, which hid it.
You decide how much to allocate, of which type, and you are responsible for
binding and freeing it. This control is powerful (sub-allocation, aliasing,
pooling) but means you must understand memory types and the cost of where data
lives.

**How it appears in this project**
`VulkanContext::createBuffer` allocates and binds memory for every buffer;
`findMemoryType` chooses the type; the depth image (`RenderPass`) allocates memory
too. The M2's unified memory is why host-visible and device-local can be the same
physical RAM here.

**Further reading**
- [Vulkan Spec — Device memory](https://docs.vulkan.org/spec/latest/chapters/memory.html)
- [Vulkan Memory Allocator (AMD)](https://gpuopen.com/vulkan-memory-allocator/)

______________________________________________________________________

## MEMORY_TYPES

**What it is**
A GPU advertises several *memory types*, each a combination of a heap and property
flags such as `DEVICE_LOCAL` (fast for the GPU), `HOST_VISIBLE` (mappable by the
CPU), and `HOST_COHERENT` (CPU writes visible without manual flushing). When
allocating, you pick a type that both satisfies the resource's requirements and
has the properties you need.

**Why it matters**
Where a resource lives determines both performance and how you can write to it.
The fastest memory for the GPU is usually not writable by the CPU, and CPU-writable
memory is usually slower for the GPU — the tension that the staging-buffer pattern
resolves. Choosing the right memory type is a core Vulkan skill.

**How it appears in this project**
`VulkanContext::findMemoryType` scans `VkPhysicalDeviceMemoryProperties` for a type
matching a resource's `memoryTypeBits` and the requested property flags. It is
called for the depth image, staging buffers, and device-local buffers.

**Further reading**
- [Vulkan Tutorial — Memory requirements](https://vulkan-tutorial.com/Vertex_buffers/Vertex_buffer_creation)
- [Vulkan Spec — Memory types and heaps](https://docs.vulkan.org/spec/latest/chapters/memory.html#memory-device)

______________________________________________________________________

## HOST_VISIBLE_MEMORY

**What it is**
Memory with the `HOST_VISIBLE` property: it can be mapped into the CPU's address
space with `vkMapMemory` so the CPU can read or write it directly. Usually paired
with `HOST_COHERENT` so writes do not need explicit flushing.

**Why it matters**
It is the only memory the CPU can write to directly, so any data that originates on
the CPU must pass through it. The catch is that it is often not the fastest memory
for the GPU to read repeatedly, which is why it is typically used as a temporary
staging area rather than the final home for static geometry.

**How it appears in this project**
The staging buffer in `Mesh::uploadDeviceLocal` is allocated `HOST_VISIBLE |
HOST_COHERENT`, mapped, filled with `memcpy`, and unmapped — the CPU-writable
stepping stone toward device-local memory. See Glossary: STAGING_BUFFER

**Further reading**
- [Vulkan Spec — Host access to memory](https://docs.vulkan.org/spec/latest/chapters/memory.html#memory-device-hostaccess)
- [Vulkan Tutorial — Staging buffer](https://vulkan-tutorial.com/Vertex_buffers/Staging_buffer)

______________________________________________________________________

## DEVICE_LOCAL_MEMORY

**What it is**
Memory with the `DEVICE_LOCAL` property: the memory that is fastest for the GPU to
access. On a discrete card this is VRAM. It is frequently *not* host-visible, so
the CPU cannot write to it directly.

**Why it matters**
Static, frequently-read resources (vertex buffers, index buffers, textures) belong
in device-local memory for best performance. Because the CPU usually cannot write
it directly, getting data there requires copying from a host-visible staging
buffer — the reason the staging pattern exists.

**How it appears in this project**
The real vertex and index buffers in `Mesh::uploadDeviceLocal` are allocated
`DEVICE_LOCAL` with `TRANSFER_DST` usage, then filled by copying from the staging
buffer. The depth image is device-local too.

**Further reading**
- [Vulkan Tutorial — Staging buffer](https://vulkan-tutorial.com/Vertex_buffers/Staging_buffer)
- [Vulkan Spec — Memory properties](https://docs.vulkan.org/spec/latest/chapters/memory.html#VkMemoryPropertyFlagBits)

______________________________________________________________________

## STAGING_BUFFER

**What it is**
A temporary host-visible buffer used to get CPU data into device-local memory: the
CPU writes the data into the staging buffer, then a GPU copy command transfers it
into the device-local destination buffer, after which the staging buffer is
discarded.

**Why it matters**
It resolves the tension between "only host-visible memory is CPU-writable" and
"only device-local memory is fast for the GPU". The one-time double copy (CPU→
staging, then GPU staging→device-local) is well worth it for resources read every
frame thereafter. It is the standard idiom for uploading static geometry and
textures.

**How it appears in this project**
`Mesh::uploadDeviceLocal` implements the full pattern, and
`VulkanContext::copyBuffer` performs the GPU-side copy on a one-time command buffer
from the dedicated upload pool.

**Further reading**
- [Vulkan Tutorial — Staging buffer](https://vulkan-tutorial.com/Vertex_buffers/Staging_buffer)
- [Vulkan Guide — Transfer queues & staging](https://docs.vulkan.org/guide/latest/queues.html)

______________________________________________________________________

## WINDING_ORDER

**What it is**
The order — clockwise or counter-clockwise — in which a triangle's three vertices
are listed, as seen from a given side. It defines which side of the triangle is
its "front" face. A consistent winding across a mesh distinguishes outward-facing
from inward-facing surfaces.

**Why it matters**
It is how the GPU decides facing for back-face culling: with a chosen front-face
winding, triangles wound the other way are considered back-facing. Get the winding
(or the `frontFace` setting) wrong and a solid object renders inside-out — outer
faces vanish and you see its back interior. Vulkan's +Y-down NDC flips apparent
winding relative to OpenGL, a classic gotcha.

**How it appears in this project**
The cube in `Mesh::cube` is wound counter-clockwise as seen from outside. Chunk 7
had no projection (and so no Y-flip), so Vulkan saw that winding as clockwise on
screen and the pipeline set `frontFace = VK_FRONT_FACE_CLOCKWISE`. Chunk 8's
projection matrix negates Y (`camera.cpp`), which restores the counter-clockwise
appearance, so the pipeline now uses `VK_FRONT_FACE_COUNTER_CLOCKWISE`.
See Glossary: BACK_FACE_CULLING

**Further reading**
- [Back-face culling & winding (Wikipedia)](https://en.wikipedia.org/wiki/Back-face_culling)
- [Flipping the Vulkan viewport (Sascha Willems)](https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport/)

______________________________________________________________________

## BACK_FACE_CULLING

**What it is**
A fixed-function optimisation that discards triangles facing away from the camera
before they are rasterised, decided from each triangle's winding order relative to
the configured front face.

**Why it matters**
For a closed, opaque object you can never see its back-facing triangles — they are
always hidden by nearer front-facing ones — so rasterising them is wasted work.
Culling them roughly halves the fragments processed. It must be paired with correct
and consistent winding, or it culls the wrong half.

**How it appears in this project**
Enabled in `ShaderPipeline` with `cullMode = VK_CULL_MODE_BACK_BIT` and a
`frontFace` matching the cube's winding. Chunk 5's single triangle used
`CULL_MODE_NONE` because its winding was not guaranteed; with a real mesh, culling
is turned on. See Glossary: WINDING_ORDER

**Further reading**
- [Back-face culling (Wikipedia)](https://en.wikipedia.org/wiki/Back-face_culling)
- [Vulkan Spec — Basic polygon rasterization (facing)](https://docs.vulkan.org/spec/latest/chapters/primsrast.html#primsrast-polygons-basic)

______________________________________________________________________

## Chunk 8 — Uniform Buffers and MVP Transforms

## UNIFORM_BUFFER

**What it is**
A small buffer of read-only data that a shader reads, where the same values are
shared by every invocation of a draw call — every vertex, every fragment. Typical
contents are transforms, camera parameters, lighting constants: things that are
constant across the geometry but change from frame to frame.

**Why it matters**
Vertex attributes vary per vertex; uniforms are the complementary channel for data
that is *uniform* across the draw. Without them you would have to bake constants
into the shader (needing a recompile to change) or duplicate them into every
vertex. They are the standard way to feed per-frame state like the MVP matrices to
the GPU.

**How it appears in this project**
`UniformBuffers` (`src/uniform_buffer.h` / `.cpp`) creates one host-visible,
persistently-mapped buffer per frame in flight holding a `UniformBufferObject`
(model, view, proj). `Renderer::drawFrame` writes the current frame's matrices and
the vertex shader reads them through its `uniform` block. See Glossary: DESCRIPTOR,
MVP_MATRIX

**Further reading**
- [Vulkan Tutorial — Uniform buffers](https://vulkan-tutorial.com/Uniform_buffers/Descriptor_layout_and_buffer)
- [Vulkan Spec — Descriptor types (uniform buffer)](https://docs.vulkan.org/spec/latest/chapters/descriptorsets.html#descriptorsets-uniformbuffer)

______________________________________________________________________

## DESCRIPTOR

**What it is**
A small handle that points a shader at one external resource — a uniform buffer, a
texture, a sampler. It is the indirection that says "this shader binding refers to
*that* GPU resource", without the resource being named in the shader itself.

**Why it matters**
Shaders cannot hold raw GPU pointers; they declare abstract bindings (set/binding
numbers) and the descriptor is what wires a concrete resource to each binding at
draw time. This indirection is what lets the same pipeline draw with different
buffers or textures by binding different descriptors.

**How it appears in this project**
Each frame's `UniformBufferObject` buffer is referenced by a uniform-buffer
descriptor, written into a descriptor set in `UniformBuffers::createDescriptorSets`
via `vkUpdateDescriptorSets`. The vertex shader's `layout(set = 0, binding = 0)
uniform` block is the binding it satisfies. See Glossary: DESCRIPTOR_SET,
UNIFORM_BUFFER

**Further reading**
- [Vulkan Spec — Resource descriptors](https://docs.vulkan.org/spec/latest/chapters/descriptorsets.html)
- [Vulkan Tutorial — Descriptor layout and buffer](https://vulkan-tutorial.com/Uniform_buffers/Descriptor_layout_and_buffer)

______________________________________________________________________

## DESCRIPTOR_SET

**What it is**
A bound collection of descriptors — a group of resource references the shader can
read as a unit. Shaders address resources as `set N, binding M`: the set is this
collection, the binding is one slot within it.

**Why it matters**
Sets are the granularity at which resources are bound to a pipeline
(`vkCmdBindDescriptorSets`). Grouping resources that change at the same rate into
one set (e.g. per-frame data in set 0, per-material data in set 1) keeps binding
cheap. A set is allocated against a layout and filled with writes before use.

**How it appears in this project**
`UniformBuffers` allocates one descriptor set per frame in flight from its pool,
each pointing at that frame's uniform buffer. `Renderer::recordCommandBuffer` binds
the current frame's set to set 0 with `vkCmdBindDescriptorSets` before the draw.
See Glossary: DESCRIPTOR, DESCRIPTOR_POOL

**Further reading**
- [Vulkan Tutorial — Descriptor pool and sets](https://vulkan-tutorial.com/Uniform_buffers/Descriptor_pool_and_sets)
- [Vulkan Spec — Descriptor sets](https://docs.vulkan.org/spec/latest/chapters/descriptorsets.html#descriptorsets-sets)

______________________________________________________________________

## DESCRIPTOR_SET_LAYOUT

**What it is**
The blueprint that declares what a descriptor set contains: for each binding, its
number, descriptor type, count, and which shader stages can read it. It describes
the *shape* of a set without referring to any actual resource.

**Why it matters**
Both sides need to agree on the contract: the pipeline is built against a set
layout (it becomes part of the pipeline layout), and descriptor sets are allocated
from it. It is what lets Vulkan validate, at pipeline-creation time, that the
resources a shader expects match what will be bound.

**How it appears in this project**
`UniformBuffers::createDescriptorSetLayout` declares one binding: a uniform buffer
at binding 0, visible to the vertex stage. The layout is handed to `ShaderPipeline`
so the pipeline layout references it, and to the allocator for the sets.
See Glossary: PIPELINE_LAYOUT, DESCRIPTOR_SET

**Further reading**
- [Vulkan Spec — Descriptor set layouts](https://docs.vulkan.org/spec/latest/chapters/descriptorsets.html#descriptorsets-setlayout)
- [Vulkan Tutorial — Descriptor layout](https://vulkan-tutorial.com/Uniform_buffers/Descriptor_layout_and_buffer)

______________________________________________________________________

## DESCRIPTOR_POOL

**What it is**
The allocator that descriptor sets are carved out of. It is created with a budget —
how many sets, and how many descriptors of each type — and hands out sets until
that budget is exhausted.

**Why it matters**
Like command buffers from a command pool, descriptor sets are not created
individually; they come from a pool so the driver can manage their memory in bulk.
Sizing the pool correctly (enough sets and descriptors for everything you will
allocate) is a common setup step, and freeing the pool frees all its sets at once.

**How it appears in this project**
`UniformBuffers::createDescriptorPool` sizes a pool for one uniform-buffer
descriptor per frame in flight and `maxSets` equal to the frame count. The
destructor destroys the pool, which implicitly frees the sets. See Glossary:
DESCRIPTOR_SET

**Further reading**
- [Vulkan Spec — Descriptor pools](https://docs.vulkan.org/spec/latest/chapters/descriptorsets.html#descriptorsets-allocation)
- [Vulkan Tutorial — Descriptor pool and sets](https://vulkan-tutorial.com/Uniform_buffers/Descriptor_pool_and_sets)

______________________________________________________________________

## MVP_MATRIX

**What it is**
The chain of three matrices that takes a vertex from its own local space all the
way to the screen: **M**odel (object → world), **V**iew (world → camera), and
**P**rojection (camera → clip space). Multiplied together (P · V · M) and applied
to a vertex position, they produce the clip-space coordinate the GPU needs.

**Why it matters**
It is the backbone of all 3D rendering: nearly every vertex shader's first job is
the MVP transform. Splitting it into three stages keeps each concern separate — you
can move the object (M), move the camera (V), or change the lens (P) independently —
and each stage maps to one well-understood coordinate space.

**How it appears in this project**
`UniformBufferObject` carries the three matrices; `mesh.vert` computes
`gl_Position = proj * view * model * vec4(inPosition, 1.0)`. Model is identity for
now, view and projection come from `Camera`. See Glossary: MODEL_MATRIX,
VIEW_MATRIX, PROJECTION_MATRIX, COORDINATE_SPACES

**Further reading**
- [LearnOpenGL — Coordinate systems](https://learnopengl.com/Getting-started/Coordinate-Systems)
- [Songho — OpenGL transformation](https://www.songho.ca/opengl/gl_transform.html)

______________________________________________________________________

## MODEL_MATRIX

**What it is**
The matrix that places an object in the world: it transforms vertices from the
object's own local (object) space into shared world space, encoding the object's
translation, rotation, and scale.

**Why it matters**
It is what lets the same mesh appear at different positions, orientations, and sizes
without changing its vertex data — a cube modelled once around its own origin can be
dropped anywhere in the scene by varying its model matrix. Animation and instancing
are just model matrices changing over time or per object.

**How it appears in this project**
`UniformBufferObject::model`, set to the identity matrix in `Renderer::drawFrame`
(the cube sits where its vertices place it; no rotation yet — the spec defers that
to later chunks). See Glossary: MVP_MATRIX, OBJECT_SPACE, WORLD_SPACE

**Further reading**
- [LearnOpenGL — Transformations](https://learnopengl.com/Getting-started/Transformations)
- [Songho — OpenGL transformation (model)](https://www.songho.ca/opengl/gl_transform.html)

______________________________________________________________________

## VIEW_MATRIX

**What it is**
The matrix that transforms world space into view (camera) space: it repositions the
whole world so the camera ends up at the origin looking down a fixed axis. It is
the inverse of the camera's own world transform.

**Why it matters**
The GPU has no notion of "a camera"; rendering always happens as if the viewer were
at the origin. The view matrix is the trick that realises a movable camera — moving
the camera right is the same as moving the world left. It is usually built from an
eye position, a target, and an up vector.

**How it appears in this project**
`Camera::viewMatrix` returns `glm::lookAt(position, target, up)`. The camera is
positioned off to one side so three faces of the cube are visible. Chunk 10 will
update these from mouse input to orbit. See Glossary: VIEW_SPACE, WORLD_SPACE

**Further reading**
- [LearnOpenGL — Camera](https://learnopengl.com/Getting-started/Camera)
- [Songho — OpenGL camera / lookAt](https://www.songho.ca/opengl/gl_camera.html)

______________________________________________________________________

## PROJECTION_MATRIX

**What it is**
The matrix that transforms view space into clip space, defining how the 3D scene is
mapped onto a 2D image — including the field of view, aspect ratio, and the near/far
depth range. It is what makes the perspective divide (done by the GPU after the
vertex shader) produce foreshortening.

**Why it matters**
It encodes the "lens": a perspective projection makes distant things smaller, an
orthographic one keeps sizes constant. It also defines the visible frustum, so
anything outside the near/far planes or the field of view is clipped. Getting its
conventions right (depth range, Y direction) is essential under Vulkan.

**How it appears in this project**
`Camera::projectionMatrix(aspect)` returns `glm::perspective(...)` with `proj[1][1]
*= -1` to flip Y for Vulkan, and the build defines `GLM_FORCE_DEPTH_ZERO_TO_ONE`
for Vulkan's [0, 1] depth. Aspect is recomputed each frame from the swapchain
extent. See Glossary: PERSPECTIVE_PROJECTION, CLIP_SPACE, FIELD_OF_VIEW

**Further reading**
- [Songho — OpenGL projection matrix](https://www.songho.ca/opengl/gl_projectionmatrix.html)
- [Vulkan: setting up a proper projection matrix (Sascha Willems)](https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport/)

______________________________________________________________________

## PERSPECTIVE_PROJECTION

**What it is**
A projection in which objects farther from the camera appear smaller and parallel
lines appear to converge toward vanishing points — the way human vision and cameras
work. Its viewing volume is a frustum (a pyramid with the tip cut off).

**Why it matters**
It is what makes a rendered scene look three-dimensional and gives depth cues:
without it, a cube viewed straight-on is indistinguishable from a flat square. The
convergence and size falloff come from the perspective divide by the w component
that the projection matrix sets up.

**How it appears in this project**
`Camera::projectionMatrix` uses `glm::perspective`. The Chunk 8 checkpoint is
exactly this effect made visible: the cube shows foreshortening — near edges larger
than far edges — instead of looking flat. See Glossary: PROJECTION_MATRIX,
ORTHOGRAPHIC_PROJECTION, HOMOGENEOUS_COORDINATES

**Further reading**
- [Perspective projection (Wikipedia)](https://en.wikipedia.org/wiki/3D_projection#Perspective_projection)
- [Scratchapixel — The perspective projection matrix](https://www.scratchapixel.com/lessons/3d-basic-rendering/perspective-and-orthographic-projection-matrix/opengl-perspective-projection-matrix.html)

______________________________________________________________________

## ORTHOGRAPHIC_PROJECTION

**What it is**
A projection with no perspective: the viewing volume is a box, not a frustum, so
objects keep the same on-screen size regardless of distance and parallel lines stay
parallel. Depth still exists (for occlusion) but does not affect apparent size.

**Why it matters**
It is the right choice when measurements must be preserved — CAD, 2D/UI rendering,
isometric games, shadow maps. Contrasting it with perspective clarifies what the
perspective divide actually does: orthographic projection simply omits it.

**How it appears in this project**
Not used — the viewer wants a realistic 3D look, so it uses perspective. It is
documented here as the conceptual contrast the spec calls for; GLM offers it as
`glm::ortho` should a future mode want it. See Glossary: PERSPECTIVE_PROJECTION,
PROJECTION_MATRIX

**Further reading**
- [Orthographic projection (Wikipedia)](https://en.wikipedia.org/wiki/Orthographic_projection)
- [LearnOpenGL — Coordinate systems (orthographic vs perspective)](https://learnopengl.com/Getting-started/Coordinate-Systems)

______________________________________________________________________

## HOMOGENEOUS_COORDINATES

**What it is**
A coordinate system that adds one extra component (w) to a position, so a 3D point
becomes a 4D vector (x, y, z, w). A point with w = 1 is an ordinary position;
dividing all components by w recovers the 3D point. This extra dimension lets a
single 4×4 matrix express translation and perspective, which a 3×3 matrix cannot.

**Why it matters**
It is the mathematical machinery behind the whole transform pipeline: translations
become matrix multiplications (not just rotations/scales), and perspective is just a
matrix that puts a depth-dependent value into w, so the later divide-by-w shrinks
distant points. This is why shader positions are `vec4`, not `vec3`.

**How it appears in this project**
`mesh.vert` builds `vec4(inPosition, 1.0)` before multiplying by the MVP chain, and
`gl_Position` is a `vec4` in clip space; the GPU performs the perspective divide by
w to reach NDC. See Glossary: MVP_MATRIX, CLIP_SPACE, NDC_SPACE

**Further reading**
- [Homogeneous coordinates (Wikipedia)](https://en.wikipedia.org/wiki/Homogeneous_coordinates)
- [Scratchapixel — Homogeneous coordinates](https://www.scratchapixel.com/lessons/mathematics-physics-for-computer-graphics/geometry/homogeneous-coordinates.html)

______________________________________________________________________

## COORDINATE_SPACES

**What it is**
The sequence of reference frames a vertex passes through on its way to the screen:
object space → world space → view space → clip space → NDC space → screen space.
Each transform in the pipeline moves the vertex from one space to the next.

**Why it matters**
Knowing which space you are in is the key to reasoning about 3D rendering: lighting
is often done in world or view space, culling in clip space, and a bug usually means
a value is in the wrong space. The MVP chain is precisely the first three hops; the
GPU does the rest (perspective divide, viewport transform).

**How it appears in this project**
`mesh.vert` walks object → world (model) → view (view) → clip (proj); the GPU then
divides by w (→ NDC) and applies the dynamic viewport (→ screen). Each space has its
own glossary entry. See Glossary: OBJECT_SPACE, WORLD_SPACE, VIEW_SPACE, CLIP_SPACE,
NDC_SPACE, SCREEN_SPACE

**Further reading**
- [LearnOpenGL — Coordinate systems](https://learnopengl.com/Getting-started/Coordinate-Systems)
- [Songho — OpenGL transformation](https://www.songho.ca/opengl/gl_transform.html)

______________________________________________________________________

## OBJECT_SPACE

**What it is**
The local coordinate frame in which a mesh's vertices are defined, centred on the
object's own origin and independent of where the object sits in the scene. Also
called model space or local space.

**Why it matters**
Modelling in a local frame is what makes a mesh reusable: the artist (or generator)
positions vertices once relative to the object's own origin, and the model matrix
later places copies anywhere. Every mesh's vertex buffer holds object-space
positions.

**How it appears in this project**
The cube's vertices in `Mesh::cube` are object-space positions; `inPosition` in
`mesh.vert` is object space until the model matrix is applied. See Glossary:
MODEL_MATRIX, WORLD_SPACE, COORDINATE_SPACES

**Further reading**
- [LearnOpenGL — Coordinate systems (local space)](https://learnopengl.com/Getting-started/Coordinate-Systems)
- [Songho — OpenGL transformation](https://www.songho.ca/opengl/gl_transform.html)

______________________________________________________________________

## WORLD_SPACE

**What it is**
The single shared coordinate frame the whole scene lives in. The model matrix
transforms each object's vertices from its own object space into this common space,
so all objects, lights, and the camera can be reasoned about together.

**Why it matters**
It is the common ground that makes a scene a scene: positions only become
comparable (distances, lighting, camera placement) once everything is in the same
world frame. The camera's position and target are world-space quantities.

**How it appears in this project**
After `model` is applied in `mesh.vert`, the vertex is in world space. The cube
spans roughly x,y ∈ [−0.4, 0.4] and z ∈ [0.2, 0.8] there, and `Camera::position` /
`target` are world-space points. See Glossary: MODEL_MATRIX, VIEW_MATRIX,
COORDINATE_SPACES

**Further reading**
- [LearnOpenGL — Coordinate systems (world space)](https://learnopengl.com/Getting-started/Coordinate-Systems)
- [Songho — OpenGL transformation](https://www.songho.ca/opengl/gl_transform.html)

______________________________________________________________________

## VIEW_SPACE

**What it is**
The coordinate frame relative to the camera: the camera sits at the origin looking
down a fixed axis (−Z in GLM's convention), and everything is expressed relative to
it. Also called eye space or camera space. The view matrix transforms world space
into it.

**Why it matters**
Many operations are simplest here because the camera is at a known, fixed place —
view-space depth is just the (negated) z coordinate, and view-space lighting avoids
needing the camera position separately. It is the bridge between the movable world
and the fixed-viewer assumption the projection makes.

**How it appears in this project**
The product `view * model * position` in `mesh.vert` is a view-space coordinate,
produced by `Camera::viewMatrix` (`glm::lookAt`). See Glossary: VIEW_MATRIX,
PROJECTION_MATRIX, COORDINATE_SPACES

**Further reading**
- [LearnOpenGL — Coordinate systems (view space)](https://learnopengl.com/Getting-started/Coordinate-Systems)
- [Songho — OpenGL camera](https://www.songho.ca/opengl/gl_camera.html)

______________________________________________________________________

## SCREEN_SPACE

**What it is**
The final 2D coordinate frame in actual pixels of the framebuffer: x and y address
a pixel, with an associated depth value. NDC is mapped into it by the viewport
transform (and the scissor restricts which part is written).

**Why it matters**
It is where rasterisation happens — triangles are filled pixel by pixel in screen
space — and where the rendered image ultimately lives. The mapping from NDC depends
on the viewport, which is why a resize that changes the viewport changes how NDC
lands on pixels without touching any earlier space.

**How it appears in this project**
The dynamic viewport/scissor set in `Renderer::recordCommandBuffer` (sized to the
swapchain extent) define the NDC → screen mapping; the rasteriser then produces
fragments in screen space. See Glossary: NDC_SPACE, VIEWPORT, SCISSOR,
COORDINATE_SPACES

**Further reading**
- [LearnOpenGL — Coordinate systems (screen space)](https://learnopengl.com/Getting-started/Coordinate-Systems)
- [Vulkan Spec — Controlling the viewport](https://docs.vulkan.org/spec/latest/chapters/vertexpostproc.html#vertexpostproc-viewport)

______________________________________________________________________

## FIELD_OF_VIEW

**What it is**
The angular extent of the scene the camera takes in, usually given as a vertical
angle (FOV-Y). A wide field of view captures more of the scene but exaggerates
perspective; a narrow one captures less and flattens it (a telephoto look).

**Why it matters**
It is the main expressive control of a perspective camera — it sets the "zoom" and
strongly affects the feel of a scene. Together with the aspect ratio it determines
the shape of the view frustum, so it is a direct input to the projection matrix.

**How it appears in this project**
`Camera::fovYRadians` defaults to 45°, passed to `glm::perspective` in
`Camera::projectionMatrix`. Combined with the swapchain's aspect ratio it defines
the frustum. See Glossary: PERSPECTIVE_PROJECTION, PROJECTION_MATRIX

**Further reading**
- [Field of view in video games (Wikipedia)](https://en.wikipedia.org/wiki/Field_of_view_in_video_games)
- [LearnOpenGL — Coordinate systems (projection / FoV)](https://learnopengl.com/Getting-started/Coordinate-Systems)

______________________________________________________________________

## Chunk 9 — OBJ Loading

## OBJ_FORMAT

**What it is**
Wavefront OBJ is a simple, human-readable text format for 3D geometry. Each line is
one record: `v x y z` a vertex position, `vn` a normal, `vt` a texture coordinate,
and `f` a face listing, per corner, indices into those three lists in the form
`position/texcoord/normal`. A companion `.mtl` file can describe materials.

**Why it matters**
Its plainness is its strength for learning: you can open the file and read the exact
numbers the GPU will draw. It is near-universally supported as an interchange format
for static meshes, which is why it is the natural first loader. Its limitations
(no animation, scene graph, or robust material model) are also why richer pipelines
move to formats like glTF or FBX.

**How it appears in this project**
`Mesh::fromObjFile` (`src/mesh.cpp`) parses `assets/mesh.obj` with tinyobjloader and
builds GPU buffers from it; the chunk's decision to use OBJ over the source FBX is
recorded in `docs/DECISIONS.md`. See Glossary: MESH, VERTEX_DEDUPLICATION

**Further reading**
- [Wavefront .obj file (Wikipedia)](https://en.wikipedia.org/wiki/Wavefront_.obj_file)
- [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader)

______________________________________________________________________

## VERTEX_DEDUPLICATION

**What it is**
The process of collapsing repeated vertices into a single shared one and rebuilding
the face list to index it. OBJ addresses position, normal, and UV through three
independent index streams, so the same fully-specified corner recurs many times
across adjacent faces; deduplication produces one unified vertex array plus an index
buffer that points into it.

**Why it matters**
The GPU's indexed-draw model wants exactly one index per vertex, and a vertex cache
that rewards reuse. Without deduplication every triangle would carry three full,
often-duplicated vertices — wasting memory and bandwidth and defeating the cache.
It is the bridge between OBJ's storage model and the GPU's.

**How it appears in this project**
`Mesh::fromObjFile` keys an `std::unordered_map<Vertex, uint32_t>` on each rebuilt
corner (using `Vertex::operator==` and a `std::hash<Vertex>` specialization): a new
corner is appended and remembered, a repeat reuses its index. For the sample model
this collapsed 9072 corners to 2829 unique vertices. See Glossary: INDEX_BUFFER,
OBJ_FORMAT

**Further reading**
- [Vulkan Tutorial — Loading models (vertex dedup)](https://vulkan-tutorial.com/Loading_models)
- [Vertex cache optimisation (Wikipedia)](https://en.wikipedia.org/wiki/Triangle_strip#Vertex_cache)

______________________________________________________________________

## UV_COORDINATES

**What it is**
A 2D coordinate pair (conventionally named u and v) stored per vertex that addresses
a position on a texture image, where (0, 0) and (1, 1) are opposite corners of the
image regardless of its pixel resolution. They are the texture-space counterpart of
a vertex's 3D position.

**Why it matters**
They are how a flat 2D image is mapped onto a 3D surface: the rasteriser interpolates
the UVs across each triangle, and the fragment shader samples the texture at the
interpolated coordinate. Without per-vertex UVs there is no correspondence between
the model's surface and an image.

**How it appears in this project**
The `Vertex::uv` field is parsed from the OBJ's `vt` records and carried through to
the GPU, but never sampled — textures are out of scope. It is kept so the vertex
format and pipeline are complete and stable. See Glossary: UV_MAPPING,
VERTEX_ATTRIBUTES

**Further reading**
- [UV mapping (Wikipedia)](https://en.wikipedia.org/wiki/UV_mapping)
- [LearnOpenGL — Textures](https://learnopengl.com/Getting-started/Textures)

______________________________________________________________________

## UV_MAPPING

**What it is**
The process of assigning UV coordinates to a mesh's vertices — effectively unwrapping
the 3D surface flat onto a 2D plane so a texture image can be painted onto it. The
result is the per-vertex UVs the renderer later interpolates and samples.

**Why it matters**
It is the authoring step that makes texturing possible and is where most texture
artefacts originate (stretching, seams, wasted texture area). Understanding that
texturing is "unwrap, then sample" demystifies why models ship with UVs and why a
bad unwrap looks wrong however good the texture is.

**How it appears in this project**
Not performed here — the project does no texturing. It is documented for context, as
the reason the OBJ carries `vt` data and the `Vertex` reserves a `uv` field. See
Glossary: UV_COORDINATES

**Further reading**
- [UV mapping (Wikipedia)](https://en.wikipedia.org/wiki/UV_mapping)
- [Blender Manual — UV editing](https://docs.blender.org/manual/en/latest/modeling/meshes/uv/index.html)

______________________________________________________________________

## SMOOTH_NORMALS

**What it is**
A normal scheme where each vertex has a single normal averaged from the faces that
share it, so the normal varies continuously across the surface. Interpolated across
triangles, it makes a faceted mesh shade as a smooth, curved surface.

**Why it matters**
It is what lets a low-polygon mesh look rounded under lighting — a sphere of a few
hundred faces can appear smooth. The choice between smooth and flat normals is purely
a shading decision; the geometry is identical. OBJ encodes it by whether adjacent
faces reference the same `vn` (smooth) or distinct ones (flat).

**How it appears in this project**
If the OBJ supplies normals, `Mesh::fromObjFile` uses them as authored — so a
smoothly-normalled export shades smoothly once lighting arrives in Chunk 11. The
sample model ships with normals. See Glossary: FLAT_NORMALS, VERTEX_ATTRIBUTES

**Further reading**
- [Normal (geometry) (Wikipedia)](https://en.wikipedia.org/wiki/Normal_(geometry))
- [LearnOpenGL — Basic lighting (normals)](https://learnopengl.com/Lighting/Basic-Lighting)

______________________________________________________________________

## FLAT_NORMALS

**What it is**
A normal scheme where every vertex of a triangle shares that triangle's single
face normal, so each face shades as one uniform plane and the mesh looks faceted.
The face normal is the normalized cross product of two of the triangle's edges.

**Why it matters**
It is the right look for genuinely faceted objects (a gem, a crystal, low-poly art)
and the natural fallback when a file has no normals at all — far better than leaving
geometry unlit. Contrasting it with smooth normals clarifies that shading appearance
comes from the normals, not the polygon count.

**How it appears in this project**
`Mesh::fromObjFile` computes a per-triangle flat normal from the winding and assigns
it to all three corners *only* when the OBJ lacks normals; the sample model has its
own, so the fallback is dormant but present for robustness. See Glossary:
SMOOTH_NORMALS, WINDING_ORDER

**Further reading**
- [Shading — flat vs smooth (Wikipedia)](https://en.wikipedia.org/wiki/Shading#Flat_shading)
- [Scratchapixel — Shading normals](https://www.scratchapixel.com/lessons/3d-basic-rendering/introduction-to-shading/shading-normals.html)

______________________________________________________________________

## ASSET_PIPELINE

**What it is**
The chain that takes content from the form an artist authors it in to the form a
program consumes at runtime: exporting from a DCC tool, converting between formats,
processing (triangulating, deduplicating, generating normals, compressing), and
loading. In a full engine it is often an automated, cached build step of its own.

**Why it matters**
Authoring formats and runtime formats have different goals (editability vs fast
loading and GPU-friendliness), so something must bridge them. Recognising loading as
one end of a broader pipeline is the seed of understanding how real engines manage
content at scale.

**How it appears in this project**
A minimal instance of one: the source FBX is exported to OBJ (the conversion step),
`assets/mesh.obj` is baked findable via the `ASSET_DIR` compile definition, and
`Mesh::fromObjFile` triangulates, deduplicates, and uploads it (the processing +
load steps). See Glossary: OBJ_FORMAT, VERTEX_DEDUPLICATION

**Further reading**
- [Asset pipeline overview (Game Programming Patterns / general)](https://en.wikipedia.org/wiki/Digital_asset_management)
- [glTF — runtime 3D asset delivery](https://www.khronos.org/gltf/)

______________________________________________________________________

## Chunk 10 — Orbit Camera and Input

## ORBIT_CAMERA

**What it is**
A camera control scheme where the camera moves on the surface of an imaginary sphere
centred on a focal point, always looking inward at that point. The user spins it
around the subject (orbit), slides the focal point (pan), and changes the sphere's
radius (zoom), rather than flying the camera freely.

**Why it matters**
It is the natural way to inspect a single object — model viewers, CAD, 3D editors all
use it — because the subject stays centred and you cannot get lost. It maps cleanly
onto three intuitive mouse gestures and, expressed in spherical coordinates, is
simple and robust to implement.

**How it appears in this project**
The whole of `Camera` (`src/camera.h` / `.cpp`): `orbit`/`pan`/`zoom` driven by mouse
input gathered in `SdlContext`, applied each frame in `main`. This is the chunk that
makes the viewer interactive. See Glossary: SPHERICAL_COORDINATES, CAMERA_TARGET

**Further reading**
- [Arcball / orbit camera (Wikipedia)](https://en.wikipedia.org/wiki/Virtual_trackball)
- [Learn OpenGL — Camera](https://learnopengl.com/Getting-started/Camera)

______________________________________________________________________

## SPHERICAL_COORDINATES

**What it is**
A way to specify a point by a distance from an origin (radius) and two angles, rather
than by x/y/z. Here: distance from the target, an azimuth angle around the vertical
axis, and an elevation angle above the horizontal plane. A direction-and-distance
description instead of a Cartesian one.

**Why it matters**
They are the natural language of an orbit camera: "spin around" is just changing the
azimuth, "look from higher" is changing the elevation, "move closer" is changing the
radius — each gesture maps to one number. Converting to Cartesian (x/y/z) when needed
is a short, standard formula.

**How it appears in this project**
`Camera::distance`, `azimuthRadians`, `elevationRadians` hold the state;
`sphericalDirection()` in `camera.cpp` converts an (azimuth, elevation) pair to a unit
Cartesian direction, and `Camera::position()` scales it by distance and adds the
target. See Glossary: AZIMUTH, ELEVATION, ORBIT_CAMERA

**Further reading**
- [Spherical coordinate system (Wikipedia)](https://en.wikipedia.org/wiki/Spherical_coordinate_system)
- [Scratchapixel — Spherical coordinates](https://www.scratchapixel.com/lessons/mathematics-physics-for-computer-graphics/geometry/spherical-coordinates-and-trigonometric-functions.html)

______________________________________________________________________

## AZIMUTH

**What it is**
The horizontal angle of the orbit: how far the camera has swung around the vertical
(Y) axis, like a compass bearing. Increasing it walks the camera around the subject
while keeping the same height and distance.

**Why it matters**
It is one of the two angles that pin down the orbit camera's direction. Driving it
from horizontal mouse movement gives the familiar "drag left/right to turn around the
object" gesture. Unlike elevation it has no poles, so it can wrap freely without any
clamp.

**How it appears in this project**
`Camera::azimuthRadians`, changed by the horizontal component of a left-drag in
`Camera::orbit`. See Glossary: ELEVATION, SPHERICAL_COORDINATES

**Further reading**
- [Azimuth (Wikipedia)](https://en.wikipedia.org/wiki/Azimuth)
- [Spherical coordinate system (Wikipedia)](https://en.wikipedia.org/wiki/Spherical_coordinate_system)

______________________________________________________________________

## ELEVATION

**What it is**
The vertical angle of the orbit: how far above (or below) the horizontal plane the
camera sits, measured from the target. Zero looks at the subject side-on; approaching
+90° looks down from directly overhead.

**Why it matters**
It is the second of the two orbit angles, driven by vertical mouse movement for the
"drag up/down to look over or under" gesture. Crucially it must be *clamped* short of
the poles: at exactly ±90° the view direction aligns with the up vector and the
camera maths degenerates (gimbal lock).

**How it appears in this project**
`Camera::elevationRadians`, changed by the vertical component of a left-drag in
`Camera::orbit` and clamped there to ±89° (`kMaxElevation`). See Glossary: AZIMUTH,
GIMBAL_LOCK

**Further reading**
- [Horizontal coordinate system — altitude (Wikipedia)](https://en.wikipedia.org/wiki/Horizontal_coordinate_system)
- [Spherical coordinate system (Wikipedia)](https://en.wikipedia.org/wiki/Spherical_coordinate_system)

______________________________________________________________________

## CAMERA_TARGET

**What it is**
The point in world space the camera looks at and orbits around — the centre of the
orbit sphere. The view matrix is built to aim the camera straight at it, and orbiting
keeps it fixed while panning slides it.

**Why it matters**
It is the anchor of an orbit camera: orbit and zoom are defined relative to it, and
panning *is* moving it. Choosing a good initial target (the model's centre) is what
keeps the subject framed; moving it is how the user re-centres on a detail.

**How it appears in this project**
`Camera::target`, initialised to the model's bounding-box centre in `main` and slid by
`Camera::pan`. It is the look-at point passed to `glm::lookAt` in `viewMatrix`.
See Glossary: ORBIT_CAMERA, VIEW_MATRIX

**Further reading**
- [glm::lookAt — target/eye/up](https://glm.g-truc.net/0.9.9/api/a00247.html)
- [Learn OpenGL — Camera (look at)](https://learnopengl.com/Getting-started/Camera)

______________________________________________________________________

## GIMBAL_LOCK

**What it is**
The loss of a degree of rotational freedom that happens when two rotation axes line
up. For a camera built from azimuth + elevation it strikes at the poles: looking
straight up or down, the "spin around" axis collapses onto the view direction and the
orientation becomes ambiguous, often snapping or flipping.

**Why it matters**
It is the classic failure mode of Euler-angle orientation. A viewer that flips over
when you drag past vertical feels broken. There are heavier fixes (quaternions), but
for an orbit camera the simplest, sufficient defence is to clamp elevation so it never
reaches the pole.

**How it appears in this project**
Avoided by clamping `elevationRadians` to ±89° in `Camera::orbit`, so the view
direction never aligns with the up vector and `glm::lookAt` never degenerates. The
deliberate choice of clamping over quaternions is documented in `docs/DECISIONS.md`.
See Glossary: ELEVATION

**Further reading**
- [Gimbal lock (Wikipedia)](https://en.wikipedia.org/wiki/Gimbal_lock)
- [Euler angles (Wikipedia)](https://en.wikipedia.org/wiki/Euler_angles)

______________________________________________________________________

## DELTA_TIME

**What it is**
The elapsed wall-clock time of the previous frame, in seconds — the gap between one
frame and the next. It is measured each iteration of the loop and fed into any
time-based update so motion is expressed as "per second" rather than "per frame".

**Why it matters**
Frame rate varies (30, 60, 144 fps; stalls during a resize). Anything that moves a
fixed amount *per frame* would then run at different speeds on different machines.
Multiplying rates by delta time decouples motion from frame rate — the foundation of
consistent real-time behaviour. See Glossary: FRAME_RATE_INDEPENDENCE

**How it appears in this project**
Measured in `main`'s loop with `std::chrono::steady_clock` and passed to
`Camera::update(dt)`, which uses it for the exponential easing of the camera toward
its goal. See Glossary: FRAME_RATE_INDEPENDENCE, ORBIT_CAMERA

**Further reading**
- [Fix Your Timestep! (Gaffer On Games)](https://gafferongames.com/post/fix_your_timestep/)
- [Delta timing (Wikipedia)](https://en.wikipedia.org/wiki/Delta_timing)

______________________________________________________________________

## FRAME_RATE_INDEPENDENCE

**What it is**
The property that an animation or control behaves identically regardless of the frame
rate it runs at — the same input or elapsed time produces the same motion at 30 fps as
at 144 fps. Achieved by driving motion from elapsed time or from input displacement,
never from a per-frame constant.

**Why it matters**
Without it, a program feels different on every machine and even moment to moment as
the frame rate fluctuates — too fast here, sluggish there, jittery during hitches. It
is what makes movement feel smooth and predictable, which is exactly the Chunk 10
checkpoint ("continuous and clean with no jitter at any frame rate").

**How it appears in this project**
Two ways. Orbit and pan are *displacement-based* — driven by how many pixels the mouse
moved — so the total motion equals the total drag whatever the frame rate. The camera's
easing toward its goal is *time-based*, using `alpha = 1 − e^(−k·dt)` in
`Camera::update`, which closes the same fraction of the gap per unit time at any frame
rate. See Glossary: DELTA_TIME, ORBIT_CAMERA

**Further reading**
- [Fix Your Timestep! (Gaffer On Games)](https://gafferongames.com/post/fix_your_timestep/)
- [Exponential smoothing (Wikipedia)](https://en.wikipedia.org/wiki/Exponential_smoothing)

______________________________________________________________________

## Chunk 11 — Diffuse Lighting

## SURFACE_NORMAL

**What it is**
A unit vector that points straight out from a surface, perpendicular to it — the
direction the surface "faces" at a given point. For a mesh it is stored per vertex
and interpolated across each triangle.

**Why it matters**
Lighting is fundamentally about the angle between a surface and the light, and the
normal is what encodes that facing. Almost every lighting calculation starts from the
normal; without it a surface has no notion of toward or away from a light. Whether
normals are shared (smooth) or per-face (flat) decides how the surface shades.

**How it appears in this project**
`Vertex::normal`, loaded from the OBJ in Chunk 9, transformed to world space in
`mesh.vert` and used by `mesh.frag` for the Lambert term. See Glossary: NORMAL_MATRIX,
LAMBERT_DIFFUSE, SMOOTH_NORMALS

**Further reading**
- [Normal (geometry) (Wikipedia)](https://en.wikipedia.org/wiki/Normal_(geometry))
- [LearnOpenGL — Basic lighting](https://learnopengl.com/Lighting/Basic-Lighting)

______________________________________________________________________

## NORMAL_MATRIX

**What it is**
The matrix used to transform normals from object space to world space: the transpose
of the inverse of the model matrix's 3×3 part. It is generally *not* the same as the
model matrix.

**Why it matters**
Transforming a normal with the model matrix works only when that matrix has no
non-uniform scale; with non-uniform scale, the model matrix tilts normals so they are
no longer perpendicular to the surface, breaking lighting. The transpose-inverse
construction cancels the scale's effect on direction, keeping normals true. It is a
classic subtle bug to use the wrong matrix.

**How it appears in this project**
`UniformBufferObject::normalMatrix`, built in `Renderer::drawFrame` as
`transpose(inverse(mat3(model)))` and applied in `mesh.vert`. With the current
identity model it equals identity, but the correct form is in place for future
transforms. Stored as a `mat4` to avoid std140 `mat3` padding. See Glossary:
SURFACE_NORMAL, MODEL_MATRIX

**Further reading**
- [LearnOpenGL — Basic lighting (the normal matrix)](https://learnopengl.com/Lighting/Basic-Lighting)
- [Normal matrix (scratchapixel / general)](https://www.scratchapixel.com/lessons/mathematics-physics-for-computer-graphics/geometry/transforming-normals.html)

______________________________________________________________________

## LIGHTING_MODEL

**What it is**
A set of equations that approximate how light interacts with a surface to produce its
final colour. Real light transport is enormously complex; a lighting model trades
physical accuracy for a cheap, plausible result that runs in real time.

**Why it matters**
It is the bridge between geometry and appearance — the reason a surface looks like
matte plastic, shiny metal, or chalk. Understanding that real-time shading is a chosen
approximation (not ground truth) frames every later technique as "a better model",
from Phong to physically based rendering.

**How it appears in this project**
The shading in `mesh.frag` — ambient + Lambert diffuse from one directional light — is
a minimal lighting model. The structure (sum of light terms, modulated by surface
colour) is the seed the rest build on. See Glossary: LAMBERT_DIFFUSE, AMBIENT_LIGHT,
PHONG_LIGHTING

**Further reading**
- [Shading (Wikipedia)](https://en.wikipedia.org/wiki/Shading)
- [LearnOpenGL — Lighting](https://learnopengl.com/Lighting/Colors)

______________________________________________________________________

## LAMBERT_DIFFUSE

**What it is**
A model of the matte component of reflection: light hitting a surface scatters equally
in all directions, so its brightness depends only on the angle between the surface
normal and the light, not on the viewer's position. The brightness is `max(dot(N, L),
0)` for unit normal N and light direction L.

**Why it matters**
It captures the dominant look of most non-shiny surfaces (paper, chalk, unpolished
wood) and is the workhorse term of real-time lighting. The cosine falloff — surfaces
facing the light squarely are brightest, grazing angles dim — is what gives 3D objects
their readable, rounded shading.

**How it appears in this project**
`float diffuse = max(dot(N, L), 0.0)` in `mesh.frag`, modulating the orange albedo by
the light colour. The `max(…, 0)` discards light from behind the surface. See
Glossary: DOT_PRODUCT_GEOMETRY, SURFACE_NORMAL, DIRECTIONAL_LIGHT

**Further reading**
- [Lambertian reflectance (Wikipedia)](https://en.wikipedia.org/wiki/Lambertian_reflectance)
- [LearnOpenGL — Basic lighting (diffuse)](https://learnopengl.com/Lighting/Basic-Lighting)

______________________________________________________________________

## AMBIENT_LIGHT

**What it is**
A constant, uniform amount of light added to every surface regardless of orientation.
It is a crude stand-in for indirect light — the glow bouncing around a scene that
fills in shadows even where no direct light reaches.

**Why it matters**
With diffuse light alone, surfaces facing away from the light go pure black, which
looks harsh and unreal — real shadows are dim, not void, because light bounces. A
small ambient term restores that, cheaply. Its crudeness (it ignores geometry
entirely) is exactly what global illumination later improves on.

**How it appears in this project**
`ubo.ambientColour` (a dim blue-grey), added to the diffuse term in `mesh.frag` so the
model's shadow side stays visible — the checkpoint's "dark but not black". See
Glossary: GLOBAL_ILLUMINATION, LIGHTING_MODEL

**Further reading**
- [Shading — ambient lighting (Wikipedia)](https://en.wikipedia.org/wiki/Shading#Ambient_lighting)
- [LearnOpenGL — Basic lighting (ambient)](https://learnopengl.com/Lighting/Basic-Lighting)

______________________________________________________________________

## DIRECTIONAL_LIGHT

**What it is**
A light defined purely by a direction, with no position — its rays are parallel and
its intensity does not fall off with distance, as if the source were infinitely far
away. The sun is the canonical example.

**Why it matters**
It is the simplest and cheapest light: every surface sees the same light direction, so
shading needs only the normal and that one constant vector. It is the natural first
light to implement and the right model for sunlight or any distant source.

**How it appears in this project**
`ubo.lightDirection`, a fixed world-space vector set in `Renderer::drawFrame`. Because
it lives in world space and never references the camera, orbiting leaves the lighting
put — the checkpoint's "light fixed in world space". See Glossary: POINT_LIGHT,
SPOT_LIGHT, LAMBERT_DIFFUSE

**Further reading**
- [LearnOpenGL — Light casters (directional)](https://learnopengl.com/Lighting/Light-casters)
- [Directional light (general CG)](https://en.wikipedia.org/wiki/Shading#Directional_lighting)

______________________________________________________________________

## POINT_LIGHT

**What it is**
A light that emits from a single position in all directions, with intensity that falls
off with distance (attenuation). A bare lightbulb is the everyday example. (Documented
for contrast; not implemented here.)

**Why it matters**
It is the next light up from directional: it adds a position, a per-surface light
direction (toward the bulb), and distance attenuation. Knowing how it differs from a
directional light clarifies what "no position, parallel rays" actually buys in
simplicity.

**How it appears in this project**
Not implemented — the project uses one directional light. It is documented so the
directional light's simplifications are understood by contrast. See Glossary:
DIRECTIONAL_LIGHT, SPOT_LIGHT

**Further reading**
- [LearnOpenGL — Light casters (point light)](https://learnopengl.com/Lighting/Light-casters)
- [Light attenuation (Wikipedia)](https://en.wikipedia.org/wiki/Attenuation)

______________________________________________________________________

## SPOT_LIGHT

**What it is**
A point light restricted to a cone: it has a position, a direction, and a cone angle,
lighting only surfaces inside the cone (often with a soft edge). A torch or stage
spotlight is the model. (Documented for contrast; not implemented here.)

**Why it matters**
It is the most constrained of the three standard lights, layering a cone test on top of
a point light. Seeing the progression directional → point → spot shows lighting as
composable pieces (direction, position, attenuation, cone) rather than unrelated
special cases.

**How it appears in this project**
Not implemented. Documented to complete the directional/point/spot trio the spec asks
for. See Glossary: DIRECTIONAL_LIGHT, POINT_LIGHT

**Further reading**
- [LearnOpenGL — Light casters (spotlight)](https://learnopengl.com/Lighting/Light-casters)
- [Spotlight (CG concept)](https://en.wikipedia.org/wiki/Shading#Spotlight)

______________________________________________________________________

## PHONG_LIGHTING

**What it is**
A classic lighting model that sums three terms: ambient (constant fill), diffuse
(matte, view-independent), and specular (a shiny highlight that depends on the viewer's
angle). Together they approximate a lit, slightly glossy surface.

**Why it matters**
It was the standard real-time lighting model for decades and remains the textbook
mental model for shading. This project implements its ambient + diffuse parts;
recognising that it is "Phong minus specular" places the work on a well-known map and
points to the obvious next step.

**How it appears in this project**
`mesh.frag` computes ambient + diffuse — two of Phong's three terms. Specular is
deliberately omitted (documented below). See Glossary: SPECULAR_LIGHT, LAMBERT_DIFFUSE,
AMBIENT_LIGHT

**Further reading**
- [Phong reflection model (Wikipedia)](https://en.wikipedia.org/wiki/Phong_reflection_model)
- [LearnOpenGL — Basic lighting](https://learnopengl.com/Lighting/Basic-Lighting)

______________________________________________________________________

## SPECULAR_LIGHT

**What it is**
The shiny highlight a surface reflects toward the viewer — bright where the reflected
light direction lines up with the eye, fading quickly away from it. Unlike diffuse, it
depends on the viewer's position, and its tightness sets how glossy the surface looks.
(Documented for completeness; not implemented here.)

**Why it matters**
It is what distinguishes a polished or wet surface from a matte one, and the one Phong
term that needs the view direction. Understanding it explains why diffuse-only shading
looks chalky, and why adding it would require feeding the camera position into the
fragment shader.

**How it appears in this project**
Not implemented — shading stops at ambient + diffuse, giving a matte look. Documented
as the missing Phong term. See Glossary: PHONG_LIGHTING, LAMBERT_DIFFUSE

**Further reading**
- [Specular highlight (Wikipedia)](https://en.wikipedia.org/wiki/Specular_highlight)
- [LearnOpenGL — Basic lighting (specular)](https://learnopengl.com/Lighting/Basic-Lighting)

______________________________________________________________________

## DOT_PRODUCT_GEOMETRY

**What it is**
The geometric meaning of the dot product of two vectors: for unit vectors it equals the
cosine of the angle between them. It is +1 when they point the same way, 0 when
perpendicular, −1 when opposite — a single number measuring alignment.

**Why it matters**
It is the mathematical heart of diffuse lighting: "how much does this surface face the
light" is exactly `dot(normal, lightDir)` for unit vectors. The same operation answers
countless CG questions (backface tests, field-of-view checks, projections), so its
geometric reading is one of the most reused tools in graphics.

**How it appears in this project**
`dot(N, L)` in `mesh.frag` gives the Lambert diffuse factor directly — both vectors are
normalised, so it is the cosine of the surface-to-light angle. See Glossary:
LAMBERT_DIFFUSE, SURFACE_NORMAL

**Further reading**
- [Dot product (Wikipedia)](https://en.wikipedia.org/wiki/Dot_product)
- [Scratchapixel — Geometry (dot product)](https://www.scratchapixel.com/lessons/mathematics-physics-for-computer-graphics/geometry/math-operations-on-points-and-vectors.html)

______________________________________________________________________

## PER_VERTEX_LIGHTING

**What it is**
Computing the lighting equation once per vertex in the vertex shader, then
interpolating the resulting *colour* across the triangle. Also called Gouraud shading.
Cheaper, but highlights and curvature between vertices are lost to linear interpolation.

**Why it matters**
It is the cheaper of the two shading granularities and was standard on older hardware.
Comparing it with per-fragment lighting explains a whole class of artefacts (banded or
washed-out highlights on coarse meshes) and the cost/quality trade-off that still
guides shader design.

**How it appears in this project**
Not used — the project lights per fragment. It is documented as the contrasting
approach. See Glossary: PER_FRAGMENT_LIGHTING, SMOOTH_NORMALS

**Further reading**
- [Gouraud shading (Wikipedia)](https://en.wikipedia.org/wiki/Gouraud_shading)
- [LearnOpenGL — Advanced lighting (Gouraud vs Phong)](https://learnopengl.com/Advanced-Lighting/Advanced-Lighting)

______________________________________________________________________

## PER_FRAGMENT_LIGHTING

**What it is**
Computing the lighting equation once per fragment in the fragment shader, using the
interpolated *normal* rather than an interpolated colour. Also called Phong shading
(distinct from the Phong lighting model). Smoother and more accurate, at higher cost.

**Why it matters**
Interpolating normals and lighting each pixel gives smooth, correctly curved shading
and crisp highlights even on coarse geometry — the visible quality jump over per-vertex
lighting. It is the standard approach on modern hardware and the natural home for
richer per-pixel effects.

**How it appears in this project**
`mesh.vert` outputs the world-space normal; `mesh.frag` renormalises it and evaluates
the Lambert term per fragment — so the smooth-normalled model shades smoothly. See
Glossary: PER_VERTEX_LIGHTING, SURFACE_NORMAL

**Further reading**
- [Phong shading (Wikipedia)](https://en.wikipedia.org/wiki/Phong_shading)
- [LearnOpenGL — Basic lighting](https://learnopengl.com/Lighting/Basic-Lighting)

______________________________________________________________________

## GLOBAL_ILLUMINATION

**What it is**
Lighting that accounts for light bouncing between surfaces — indirect light — not just
the direct path from source to surface. It captures effects like colour bleeding,
soft shadows, and the fill light in shadowed areas. It is expensive and usually
approximated. (Conceptual here; not implemented.)

**Why it matters**
It is what the crude constant ambient term stands in for. Knowing that ambient is a
flat fake for a rich, geometry-dependent phenomenon explains both why simple shading
looks slightly off and what the high end of rendering (path tracing, baked GI, probes)
is actually solving.

**How it appears in this project**
Not implemented; approximated by the constant ambient term in `mesh.frag`. Documented
as what that term gestures at. See Glossary: AMBIENT_LIGHT, LIGHTING_MODEL

**Further reading**
- [Global illumination (Wikipedia)](https://en.wikipedia.org/wiki/Global_illumination)
- [Scratchapixel — Global illumination](https://www.scratchapixel.com/lessons/3d-basic-rendering/global-illumination-path-tracing.html)

______________________________________________________________________

## Chunk 12 — Polish and Completeness

## RESOURCE_LIFETIME

**What it is**
The span between when a resource is created and when it is destroyed, and the rules
governing it. In graphics programming this especially means GPU resources (buffers,
images, pipelines, descriptor pools) and the constraints on when they may be freed —
notably, never while the GPU might still be using them.

**Why it matters**
Vulkan does almost no lifetime management for you: every object you create you must
destroy, exactly once, at a safe time. Free a resource too early (while a frame still
references it) and you get a crash or corruption; free it too late or not at all and
you leak. Disciplined lifetime management — RAII plus synchronising with the GPU
before destruction — is what keeps an explicit API like Vulkan correct.

**How it appears in this project**
Every Vulkan-owning class follows RAII: it creates its objects in the constructor and
destroys them in the destructor, and is non-copyable so ownership is unique. The GPU
is brought idle (`vkDeviceWaitIdle`) before teardown — in `Renderer`'s destructor and
in `main` after the loop — so nothing is freed mid-flight. See Glossary:
VULKAN_TEARDOWN_ORDER, GPU_CPU_SYNC

**Further reading**
- [RAII (cppreference)](https://en.cppreference.com/w/cpp/language/raii)
- [Vulkan Spec — Object lifetime](https://docs.vulkan.org/spec/latest/chapters/fundamentals.html#fundamentals-objectmodel-lifetime)

______________________________________________________________________

## VULKAN_TEARDOWN_ORDER

**What it is**
The requirement that Vulkan objects be destroyed in the reverse of the order they
depend on each other: a parent object must outlive its children. The device outlives
everything made from it; the swapchain's image views outlive the framebuffers built
on them; the instance outlives the surface and the device; the window outlives the
surface.

**Why it matters**
Destroying a parent while a child still references it is undefined behaviour — a
likely crash or a validation error. Because the dependency graph is strict and
one-directional, getting the destruction order right is not optional. It is the single
most common source of teardown bugs in Vulkan programs.

**How it appears in this project**
Encoded structurally rather than by hand: the RAII objects in `main` are declared in
dependency order (`SdlContext` → `VulkanContext` → `Swapchain` → `RenderPass` →
`UniformBuffers` → `ShaderPipeline` → `Renderer` → `Mesh`), so C++ destroys them in
exact reverse. `VulkanContext`'s own destructor mirrors the discipline internally
(device, then surface, then debug messenger, then instance). See Glossary:
RESOURCE_LIFETIME, LOGICAL_DEVICE

**Further reading**
- [Vulkan Spec — Object lifetime](https://docs.vulkan.org/spec/latest/chapters/fundamentals.html#fundamentals-objectmodel-lifetime)
- [Vulkan Tutorial — Cleanup](https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Base_code)
