// Vulkan Orbit Viewer — main.cpp
//
// This file is the table of contents for the whole project. Read top to bottom,
// it tells the story of the program: open a window, then loop forever handling
// input and — in later chunks — drawing a mesh you can orbit, pan, and zoom,
// until the user closes the window.
//
// Chunk 1 builds only the first beat of that story: a resizable window driven by
// an event loop. There is no Vulkan here yet. Windowing is kept deliberately
// separate from rendering, so this chapter knows nothing about the GPU.
// See Glossary: WINDOWING_SYSTEM, EVENT_LOOP

#include "sdl_context.h"

#include <cstdlib>
#include <exception>
#include <iostream>

int main() {
    try {
        // --- Chapter 1: the window -------------------------------------------
        // Open the operating-system window. Everything the user sees lives
        // inside it, and everything Vulkan eventually draws will be presented
        // to it. The title here is what shows in the title bar.
        // See Glossary: SDL3
        SdlContext sdl("Vulkan Orbit Viewer", 1280, 720);

        // --- The main loop ---------------------------------------------------
        // A real-time application is a loop, and each iteration is one frame.
        // For now the loop only handles input; in later chunks it will also
        // update the camera and render. It runs until processEvents() reports
        // that the user wants to quit. See Glossary: EVENT_LOOP
        bool running = true;
        while (running) {
            // Handle every input event that arrived since the previous frame.
            // This is the only work the loop does in Chunk 1.
            // See Glossary: EVENT_POLLING
            running = sdl.processEvents();
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
