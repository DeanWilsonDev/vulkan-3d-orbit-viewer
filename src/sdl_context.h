#pragma once

// SdlContext — owns the operating-system window and the stream of input events
// that flow from it. Everything that touches SDL lives behind this class so the
// rest of the program never speaks to the windowing system directly: it only
// ever sees a clean window handle and a single "should we keep running?" signal.
//
// Keeping window management separate from rendering is deliberate. A window is
// an operating-system concern (X11/Wayland/Cocoa/Win32); rendering is a Vulkan
// concern. By drawing the boundary here, the Vulkan code we add in later chunks
// never needs to know which platform it is running on.
// See Glossary: WINDOWING_SYSTEM, SDL3

#include <SDL3/SDL.h>   // SDL3 public API: window creation, the event queue, input
#include <string>

class SdlContext {
public:
    // Initialise SDL and open a window of the given pixel size with the given
    // title bar text. Throws std::runtime_error if SDL or the window fail to
    // come up, so callers fail loudly instead of limping on with a null window.
    SdlContext(const std::string& title, int width, int height);

    // Destroy the window and shut SDL down, in reverse order of creation.
    ~SdlContext();

    // This object owns a unique OS resource (the window handle), so copying it
    // is forbidden — two copies would each try to destroy the same window.
    SdlContext(const SdlContext&) = delete;
    SdlContext& operator=(const SdlContext&) = delete;

    // Drain every event that has arrived since the previous frame and reduce
    // them to one answer: should the application keep running? Returns false
    // once the user has asked to close the window or quit the app.
    // See Glossary: EVENT_LOOP, EVENT_POLLING
    bool processEvents();

    // The raw SDL window handle. From Chunk 2 we hand this to SDL so it can
    // create a Vulkan surface for the window. Exposed read-only.
    SDL_Window* window() const { return m_window; }

private:
    SDL_Window* m_window = nullptr;   // The OS window; owned and destroyed by this object.
};
