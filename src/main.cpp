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
#include "mesh.h"
#include "uniform_buffer.h"
#include "camera.h"

#include <SDL3/SDL_video.h>   // SDL_GetWindowSizeInPixels (minimised-window check)

#include <glm/glm.hpp>        // camera-framing maths on the model's bounds

#include <chrono>             // per-frame delta time for the orbit camera
#include <cmath>
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

int main(int argc, char** argv) {
    try {
        // An optional command-line argument selects the model to load; with none we
        // fall back to the bundled sample. e.g. `vulkan-orbit-viewer assets/torus.obj`.
        const char* modelPath = (argc > 1) ? argv[1] : ASSET_DIR "/mesh.obj";

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

        // --- Chapter 5: the mesh ---------------------------------------------
        // Geometry loaded from an OBJ file on disk (Chunk 9), replacing Chunk 7's
        // hardcoded cube. tinyobjloader parses it and Mesh deduplicates the
        // vertices into GPU buffers; it also reads the MTL's diffuse texture name.
        // The path comes from the command line (or the bundled default).
        // See Glossary: MESH, OBJ_FORMAT, VERTEX_BUFFER, INDEX_BUFFER
        Mesh model = Mesh::fromObjFile(vulkan, modelPath);

        // --- Chapter 6: the diffuse texture ----------------------------------
        // The model's diffuse map, loaded from the path its MTL declared (or a 1×1
        // white fallback if it has none). Created before the descriptors so they can
        // bind it. See Glossary: TEXTURE, DIFFUSE_MAP, SAMPLER
        Texture texture(vulkan, model.diffuseTexturePath());

        // --- Chapter 7: uniform buffers and descriptors ----------------------
        // The per-frame MVP/light uniform buffers, plus the descriptor set layout /
        // pool / sets that bind them (binding 0) and the diffuse texture (binding 1).
        // Created before the pipeline because the pipeline layout is built against
        // this descriptor set layout. See Glossary: UNIFORM_BUFFER, DESCRIPTOR_SET,
        // DESCRIPTOR_SET_LAYOUT, DESCRIPTOR_POOL, COMBINED_IMAGE_SAMPLER
        UniformBuffers uniforms(vulkan, Renderer::kMaxFramesInFlight,
                                texture.view(), texture.sampler());

        // --- Chapter 8: the graphics pipeline --------------------------------
        // Compiled shaders plus all the fixed-function state, baked into one
        // immutable pipeline object built for the render pass above. It references
        // the descriptor set layout so the uniforms + texture can be bound.
        // See Glossary: GRAPHICS_PIPELINE
        ShaderPipeline pipeline(vulkan, renderPass.handle(), uniforms.layout());

        // --- Chapter 9: command buffers and synchronisation ------------------
        // Command buffers + the semaphores/fences that drive each frame, with two
        // frames in flight so the CPU records ahead while the GPU draws.
        // See Glossary: COMMAND_BUFFER, FRAME_LOOP, FRAMES_IN_FLIGHT, SYNCHRONISATION
        Renderer renderer(vulkan, swapchain);

        // --- Chapter 10: the orbit camera ------------------------------------
        // The viewpoint: an orbit camera circling the model. We frame it on the
        // loaded model's bounding box so any asset — at any scale or origin — fills
        // the view, then snap() settles it so the first frame is already in place.
        // The main loop drives it from the mouse (orbit/pan/zoom) below.
        // See Glossary: ORBIT_CAMERA, VIEW_MATRIX, PROJECTION_MATRIX
        Camera camera;
        const glm::vec3 modelCenter = model.boundsCenter();
        const float modelRadius = model.boundsRadius();
        // Distance at which a sphere of modelRadius just fits the vertical field of
        // view (d = r / sin(fov/2)), with a margin so it does not touch the edges.
        const float fitDistance = modelRadius / std::sin(camera.fovYRadians * 0.5f) * 1.3f;
        camera.target = modelCenter;
        camera.distance = fitDistance;
        camera.sceneRadius = modelRadius;          // sizes the near/far planes each frame
        camera.minDistance = modelRadius * 1.1f;   // closest zoom: just outside the mesh
        camera.maxDistance = fitDistance * 8.0f;   // farthest zoom
        camera.snap();                             // jump the displayed state to this framing

        // --- The main loop ---------------------------------------------------
        // Each iteration is one frame: measure elapsed time, handle input, move the
        // camera, then draw and present. The swapchain and its size-dependent
        // resources are rebuilt whenever the window resizes or the swapchain reports
        // itself out of date. See Glossary: EVENT_LOOP, FRAME_LOOP
        bool running = true;
        auto previousTime = std::chrono::steady_clock::now();
        while (running) {
            // Delta time: how long the previous frame took, in seconds. The camera
            // uses it to move at a rate independent of frame rate.
            // See Glossary: DELTA_TIME, FRAME_RATE_INDEPENDENCE
            const auto now = std::chrono::steady_clock::now();
            const float dt = std::chrono::duration<float>(now - previousTime).count();
            previousTime = now;

            // Handle every input event that arrived since the previous frame.
            // See Glossary: EVENT_POLLING
            running = sdl.processEvents();
            if (!running) break;

            // Feed this frame's mouse input to the camera: left-drag orbits,
            // right-drag pans, the wheel zooms. update() then eases the camera
            // toward the new goal. See Glossary: ORBIT_CAMERA
            const SdlContext::MouseInput input = sdl.takeMouseInput();
            if (input.leftDown)  camera.orbit(input.dx, input.dy);
            if (input.rightDown) camera.pan(input.dx, input.dy);
            if (input.scroll != 0.0f) camera.zoom(input.scroll);
            camera.update(dt);

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
                needsRecreate = renderer.drawFrame(swapchain, renderPass, pipeline, model,
                                                   camera, uniforms);
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
