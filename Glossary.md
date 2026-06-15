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
