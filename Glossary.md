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
