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
