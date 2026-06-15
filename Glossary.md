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
