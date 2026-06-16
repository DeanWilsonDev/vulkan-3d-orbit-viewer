#pragma once

// Renderer — owns the per-frame machinery and runs one frame of the
// acquire → record → submit → present cycle. It keeps two frames "in flight" so
// the CPU can record the next frame while the GPU is still finishing the current
// one, instead of the two stalling on each other.
// See Glossary: COMMAND_BUFFER, FRAME_LOOP, FRAMES_IN_FLIGHT, SYNCHRONISATION
//
// (The spec assigns command buffers + synchronisation to Chunk 6 and a separate
// renderer file is not in its layout; this file is a documented deviation —
// the frame loop was brought forward to Chunk 5 for visible output, and Chunk 6
// expanded it to two frames in flight. See docs/DECISIONS.md.)

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <vector>

class VulkanContext;
class Swapchain;
class RenderPass;
class ShaderPipeline;
class Mesh;
class Camera;
class UniformBuffers;

class Renderer {
public:
    Renderer(VulkanContext& context, const Swapchain& swapchain);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Draw and present one frame, rendering the given mesh as seen by the camera.
    // The current frame's MVP matrices are written into `uniforms` and the matching
    // descriptor set is bound. Returns true if the swapchain is out of date /
    // suboptimal and the caller should recreate it (and call onSwapchainRecreated).
    // See Glossary: FRAME_LOOP, MVP_MATRIX
    bool drawFrame(Swapchain& swapchain, RenderPass& renderPass, ShaderPipeline& pipeline,
                   const Mesh& mesh, const Camera& camera, UniformBuffers& uniforms);

    // Rebuild the per-image synchronisation after the swapchain was recreated
    // (the image count could change). See Glossary: SEMAPHORE
    void onSwapchainRecreated(const Swapchain& swapchain);

    // How many frames the CPU may be working on ahead of the GPU. Two is the
    // usual sweet spot: enough to keep both busy, not so many that input latency
    // grows. See Glossary: FRAMES_IN_FLIGHT
    static constexpr uint32_t kMaxFramesInFlight = 2;

private:
    void createCommandResources();
    void createSyncObjects(uint32_t imageCount);
    void destroySyncObjects();
    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex,
                             Swapchain& swapchain, RenderPass& renderPass,
                             ShaderPipeline& pipeline, const Mesh& mesh,
                             VkDescriptorSet descriptorSet);

    VulkanContext& m_context;

    // A command pool allocates command buffers; one command buffer per frame in
    // flight, so a frame being recorded never touches a buffer the GPU is still
    // reading. See Glossary: COMMAND_POOL, COMMAND_BUFFER
    VkCommandPool                                       m_commandPool = VK_NULL_HANDLE;
    std::array<VkCommandBuffer, kMaxFramesInFlight>     m_commandBuffers{};

    // Synchronisation, split by what each primitive coordinates:
    //  - imageAvailable (per frame): the GPU waits for the acquired swapchain
    //    image to be free before drawing into it.
    //  - renderFinished (per swapchain image): present waits for that image's
    //    drawing to finish. Per-image, so it is never reused while the
    //    presentation engine may still be waiting on it.
    //  - inFlight (per frame): the CPU waits on this fence so it does not start
    //    reusing a frame's resources before that frame's GPU work is done.
    // See Glossary: SEMAPHORE, FENCE, GPU_CPU_SYNC, SYNCHRONISATION
    std::array<VkSemaphore, kMaxFramesInFlight> m_imageAvailable{};
    std::vector<VkSemaphore>                    m_renderFinished;   // indexed by swapchain image
    std::array<VkFence, kMaxFramesInFlight>     m_inFlight{};

    // Which in-flight frame slot (0..kMaxFramesInFlight-1) the next frame uses.
    uint32_t m_currentFrame = 0;
};
