#pragma once

// Renderer — owns the per-frame machinery and runs one frame of the
// acquire → record → submit → present cycle. This is the minimal frame loop:
// a single command buffer and a single in-flight frame. Chunk 6 expands it to
// two frames in flight with the full synchronisation treatment; the concepts are
// introduced here because Chunk 5 needs to actually show its triangle.
// See Glossary: COMMAND_BUFFER, FRAME_LOOP
//
// (The spec assigns command buffers + synchronisation to Chunk 6 and a separate
// renderer file is not in its layout; this file is a documented deviation made
// to give Chunk 5 visible output — see docs/DECISIONS.md.)

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

class VulkanContext;
class Swapchain;
class RenderPass;
class ShaderPipeline;

class Renderer {
public:
    Renderer(VulkanContext& context, const Swapchain& swapchain);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Draw and present one frame. Returns true if the swapchain is out of date /
    // suboptimal and the caller should recreate it (and call onSwapchainRecreated).
    // See Glossary: FRAME_LOOP
    bool drawFrame(Swapchain& swapchain, RenderPass& renderPass, ShaderPipeline& pipeline);

    // Rebuild the per-image synchronisation after the swapchain was recreated
    // (the image count could change). See Glossary: SEMAPHORE
    void onSwapchainRecreated(const Swapchain& swapchain);

private:
    void createCommandResources();
    void createSyncObjects(uint32_t imageCount);
    void destroySyncObjects();
    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex,
                             Swapchain& swapchain, RenderPass& renderPass,
                             ShaderPipeline& pipeline);

    VulkanContext& m_context;

    // A command pool allocates command buffers; one command buffer records this
    // frame's draw commands. See Glossary: COMMAND_POOL, COMMAND_BUFFER
    VkCommandPool   m_commandPool = VK_NULL_HANDLE;
    VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;

    // Synchronisation. imageAvailable: GPU waits for the swapchain image to be
    // free before drawing. renderFinished: present waits for drawing to finish
    // (one per swapchain image, the robust pattern). inFlight: the CPU waits on
    // this fence so it does not start a new frame before the last one's GPU work
    // is done. See Glossary: SEMAPHORE, FENCE, GPU_CPU_SYNC
    VkSemaphore              m_imageAvailable = VK_NULL_HANDLE;
    std::vector<VkSemaphore> m_renderFinished;   // indexed by swapchain image
    VkFence                  m_inFlight = VK_NULL_HANDLE;
};
