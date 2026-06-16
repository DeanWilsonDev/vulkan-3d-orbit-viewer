#include "sdl_context.h"

#include <stdexcept>
#include <string>

SdlContext::SdlContext(const std::string& title, int width, int height) {
    // Bring up SDL's video subsystem. This connects to the platform windowing
    // system and must succeed before any window can exist. SDL3 reports success
    // with a bool return (true on success), unlike SDL2's "0 means success".
    // See Glossary: WINDOWING_SYSTEM
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }

    // Create the OS window. SDL3 dropped the explicit x/y position arguments
    // SDL2 had — the windowing system decides placement. SDL_WINDOW_RESIZABLE
    // lets the user resize the window; from Chunk 3 we will respond to resize
    // events by recreating the Vulkan swapchain. SDL_WINDOW_VULKAN tells SDL to
    // load the Vulkan loader library and prepare the window so a Vulkan surface
    // can later be created for it (done in VulkanContext).
    // See Glossary: SDL3, SURFACE
    m_window = SDL_CreateWindow(title.c_str(), width, height,
                                SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
    if (m_window == nullptr) {
        // Window creation failed: shut the subsystem we just started back down
        // before throwing, so a failed constructor leaves nothing leaked.
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }
}

SdlContext::~SdlContext() {
    // Tear down in the exact reverse of construction: the window first, then the
    // subsystem that created it. This reverse-order discipline becomes critical
    // once Vulkan objects — which have strict parent/child lifetimes — appear.
    if (m_window != nullptr) {
        SDL_DestroyWindow(m_window);
    }
    SDL_Quit();
}

bool SdlContext::processEvents() {
    SDL_Event event;
    // Poll rather than block on the event queue: a real-time renderer must keep
    // producing frames even when no input arrives, so we take whatever events
    // are already waiting and return immediately instead of sleeping until one
    // shows up. See Glossary: EVENT_POLLING
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            // The application as a whole was asked to quit (e.g. Cmd+Q on macOS,
            // or the last window being closed). See Glossary: EVENT_LOOP
            case SDL_EVENT_QUIT:
                return false;

            // This specific window's close button (the title-bar X) was clicked.
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                return false;

            // The window's size in actual pixels changed (a drag-resize, a
            // maximise, or a move between displays of different scale). The
            // swapchain is sized to the window, so it must be rebuilt; we record
            // that here and let the main loop act on it. See Glossary: SWAPCHAIN
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                m_resized = true;
                break;

            // Mouse motion: accumulate the relative movement (xrel/yrel are deltas
            // in pixels since the last motion event). Summing them means a frame
            // sees the total movement even if several motion events arrived.
            // See Glossary: ORBIT_CAMERA
            case SDL_EVENT_MOUSE_MOTION:
                m_mouseDX += event.motion.xrel;
                m_mouseDY += event.motion.yrel;
                break;

            // Mouse buttons: track left/right as live up/down state, which the
            // camera reads to decide whether a drag is an orbit or a pan.
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                const bool down = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
                if (event.button.button == SDL_BUTTON_LEFT) m_leftDown = down;
                else if (event.button.button == SDL_BUTTON_RIGHT) m_rightDown = down;
                break;
            }

            // Wheel: accumulate vertical ticks for zoom.
            case SDL_EVENT_MOUSE_WHEEL:
                m_scrollY += event.wheel.y;
                break;

            default:
                // Every other event is ignored.
                break;
        }
    }
    // No quit request was seen this frame, so the application keeps running.
    return true;
}

bool SdlContext::takeResized() {
    // Hand the caller the pending-resize state and clear it, so a single resize
    // triggers exactly one swapchain rebuild.
    bool was = m_resized;
    m_resized = false;
    return was;
}

SdlContext::MouseInput SdlContext::takeMouseInput() {
    MouseInput input;
    input.dx = m_mouseDX;
    input.dy = m_mouseDY;
    input.scroll = m_scrollY;
    input.leftDown = m_leftDown;
    input.rightDown = m_rightDown;
    // Consume the per-frame deltas so each movement is applied exactly once; the
    // button states are levels, not events, so they persist across frames.
    m_mouseDX = 0.0f;
    m_mouseDY = 0.0f;
    m_scrollY = 0.0f;
    return input;
}
