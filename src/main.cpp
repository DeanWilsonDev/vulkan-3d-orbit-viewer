// Vulkan Orbit Viewer — main.cpp
//
// This file is the table of contents for the whole project. Read top to bottom,
// it tells the story of the program: open a window, then loop forever handling
// input and — in later chunks — drawing a mesh you can orbit, pan, and zoom,
// until the user closes the window.
//
// Chunk 1 built the first beat: a resizable window driven by an event loop.
// Chunk 2 brought Vulkan up — instance, device, queues, and the surface tying
// Vulkan to the window. Chunk 3 adds the swapchain: the chain of images the
// renderer will present to the screen, rebuilt whenever the window resizes.
// Still nothing is drawn. See Glossary: WINDOWING_SYSTEM, EVENT_LOOP,
// VULKAN_INSTANCE, SWAPCHAIN

#include "sdl_context.h"
#include "vulkan_context.h"
#include "swapchain.h"

#include <cstdlib>
#include <exception>
#include <iostream>

// Validation layers are expensive runtime checks, so they are enabled only in
// debug builds. NDEBUG is defined by CMake in Release/RelWithDebInfo builds.
// See Glossary: VALIDATION_LAYERS
#ifdef NDEBUG
constexpr bool kEnableValidation = false;
#else
constexpr bool kEnableValidation = true;
#endif

int main() {
    try {
        // --- Chapter 1: the window -------------------------------------------
        // Open the operating-system window. Everything the user sees lives
        // inside it, and everything Vulkan eventually draws will be presented
        // to it. The title here is what shows in the title bar.
        // See Glossary: SDL3
        SdlContext sdl("Vulkan Orbit Viewer", 1280, 720);

        // --- Chapter 2: Vulkan foundation ------------------------------------
        // Bring Vulkan up for this window: instance, debug messenger, surface,
        // a chosen GPU, and a logical device with graphics + present queues.
        // Nothing is drawn yet — this is the groundwork the renderer stands on.
        // See Glossary: VULKAN_INSTANCE, LOGICAL_DEVICE, SURFACE
        VulkanContext vulkan(sdl.window(), kEnableValidation);
        std::cout << "Vulkan initialised successfully.\n";

        // --- Chapter 3: the swapchain ----------------------------------------
        // The images the renderer will draw into and present to the window. It
        // is sized to the window, so the loop below rebuilds it on resize.
        // See Glossary: SWAPCHAIN
        Swapchain swapchain(vulkan, sdl.window());

        // --- The main loop ---------------------------------------------------
        // A real-time application is a loop, and each iteration is one frame.
        // For now the loop handles input and reacts to resizes; in later chunks
        // it will also update the camera and render. It runs until
        // processEvents() reports that the user wants to quit.
        // See Glossary: EVENT_LOOP
        bool running = true;
        while (running) {
            // Handle every input event that arrived since the previous frame.
            // See Glossary: EVENT_POLLING
            running = sdl.processEvents();

            // If the window changed size, the swapchain images no longer match
            // it and must be rebuilt before the next frame would be drawn.
            // See Glossary: SWAPCHAIN
            if (sdl.takeResized()) {
                swapchain.recreate();
            }
        }
    } catch (const std::exception& e) {
        // Any failure during setup (SDL init or window creation) is thrown and
        // caught here. Report it and exit non-zero so a shell or CI run can see
        // that the program failed rather than succeeded.
        std::cerr << "Fatal: " << e.what() << '\n';
        return EXIT_FAILURE;
    }

    // Clean exit: by the time control reaches here, SdlContext's destructor has
    // already destroyed the window and shut SDL down.
    return EXIT_SUCCESS;
}
