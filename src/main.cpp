// Vulkan Orbit Viewer — main.cpp
//
// This file is the table of contents for the whole project. Read top to bottom,
// it tells the story of the program: open a window, then loop forever handling
// input and — in later chunks — drawing a mesh you can orbit, pan, and zoom,
// until the user closes the window.
//
// Chunk 1 built the first beat: a resizable window driven by an event loop.
// Chunk 2 brought Vulkan up — instance, device, queues, surface. Chunk 3 added
// the swapchain; Chunk 4 the render pass, depth buffer, and framebuffers. Chunk 5
// is the payoff: a graphics pipeline (compiled shaders + fixed-function state)
// and a minimal frame loop that draws a hardcoded triangle and presents it — the
// project's first visible output. See Glossary: WINDOWING_SYSTEM, EVENT_LOOP,
// VULKAN_INSTANCE, SWAPCHAIN, RENDER_PASS, GRAPHICS_PIPELINE, FRAME_LOOP

#include "sdl_context.h"
#include "vulkan_context.h"
#include "swapchain.h"
#include "render_pass.h"
#include "shader_pipeline.h"
#include "renderer.h"

#include <SDL3/SDL_video.h>   // SDL_GetWindowSizeInPixels (minimised-window check)

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

        // --- Chapter 4: render pass and framebuffers -------------------------
        // Describe the rendering operation (colour + depth attachments) and bind
        // it to concrete images (depth buffer + per-image framebuffers). These
        // are the targets the renderer will draw onto. See Glossary: RENDER_PASS,
        // FRAMEBUFFER, DEPTH_BUFFER
        RenderPass renderPass(vulkan, swapchain);

        // --- Chapter 5: the graphics pipeline --------------------------------
        // Compiled shaders plus all the fixed-function state, baked into one
        // immutable pipeline object built for the render pass above.
        // See Glossary: GRAPHICS_PIPELINE
        ShaderPipeline pipeline(vulkan, renderPass.handle());

        // --- Chapter 6: command buffers and synchronisation ------------------
        // Command buffers + the semaphores/fences that drive each frame, with two
        // frames in flight so the CPU records ahead while the GPU draws.
        // See Glossary: COMMAND_BUFFER, FRAME_LOOP, FRAMES_IN_FLIGHT, SYNCHRONISATION
        Renderer renderer(vulkan, swapchain);

        // --- The main loop ---------------------------------------------------
        // Each iteration is one frame: handle input, then draw and present. The
        // swapchain and its size-dependent resources are rebuilt whenever the
        // window resizes or the swapchain reports itself out of date.
        // See Glossary: EVENT_LOOP, FRAME_LOOP
        bool running = true;
        while (running) {
            // Handle every input event that arrived since the previous frame.
            // See Glossary: EVENT_POLLING
            running = sdl.processEvents();
            if (!running) break;

            // While the window is minimised its drawable size is zero, which is
            // an invalid render target — skip drawing until it is restored.
            int pxW = 0, pxH = 0;
            SDL_GetWindowSizeInPixels(sdl.window(), &pxW, &pxH);
            if (pxW == 0 || pxH == 0) continue;

            // Draw + present one frame. A resize event, or an out-of-date result
            // from the draw, means the swapchain (and its dependent resources)
            // must be rebuilt before the next frame. See Glossary: SWAPCHAIN
            bool needsRecreate = sdl.takeResized();
            if (!needsRecreate) {
                needsRecreate = renderer.drawFrame(swapchain, renderPass, pipeline);
            }
            if (needsRecreate) {
                vulkan.waitIdle();          // ensure nothing is using the old resources
                swapchain.recreate();
                renderPass.recreate();      // depth + framebuffers follow the swapchain
                renderer.onSwapchainRecreated(swapchain);
            }
        }

        // The loop has exited; wait for the GPU to finish its last frame before
        // the RAII destructors start tearing Vulkan objects down.
        vulkan.waitIdle();
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
