#pragma once

// RenderPass — owns the render pass plus the concrete render targets it operates
// on: the depth buffer and one framebuffer per swapchain image. The render pass
// itself only *describes* a rendering operation (which attachments exist, what
// happens to them); the framebuffers bind that description to actual images.
// See Glossary: RENDER_PASS, FRAMEBUFFER
//
// The render pass depends only on attachment formats, so it survives a window
// resize. The depth image and framebuffers depend on the swapchain's size and
// images, so they are rebuilt by recreate() whenever the swapchain is rebuilt.
//
// Nothing is drawn in this chunk — these objects are the stage the renderer will
// later draw onto. The clear colour is configured here but only takes visible
// effect once a frame is actually recorded and presented (Chunk 6).

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <vector>

class VulkanContext;
class Swapchain;

class RenderPass {
public:
    // Build the render pass and its render targets for the given swapchain.
    // Throws std::runtime_error on failure.
    RenderPass(VulkanContext& context, Swapchain& swapchain);

    // Destroy framebuffers, depth resources, and the render pass.
    ~RenderPass();

    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;

    // Rebuild the size-dependent targets (depth image + framebuffers) after the
    // swapchain has been recreated. The render pass object itself is reused.
    // See Glossary: FRAMEBUFFER
    void recreate();

    // Accessors used by the frame loop in later chunks.
    VkRenderPass  handle() const { return m_renderPass; }
    VkFramebuffer framebuffer(uint32_t imageIndex) const { return m_framebuffers[imageIndex]; }
    VkFormat      depthFormat() const { return m_depthFormat; }

    // The values written into the attachments by the render pass's CLEAR load
    // op: index 0 is the colour clear, index 1 is the depth clear.
    // See Glossary: ATTACHMENT
    const std::array<VkClearValue, 2>& clearValues() const { return m_clearValues; }

private:
    void createRenderPass();
    void createDepthResources();
    void createFramebuffers();
    void destroyTargets();   // depth + framebuffers (everything recreate() rebuilds)

    VulkanContext& m_context;
    Swapchain&     m_swapchain;

    VkRenderPass m_renderPass = VK_NULL_HANDLE;  // the rendering-structure description (size-independent)

    VkFormat       m_depthFormat{};                     // chosen depth attachment format
    VkImage        m_depthImage = VK_NULL_HANDLE;       // the depth buffer image
    VkDeviceMemory m_depthMemory = VK_NULL_HANDLE;      // GPU memory backing the depth image
    VkImageView    m_depthView = VK_NULL_HANDLE;        // view used to attach the depth image

    std::vector<VkFramebuffer> m_framebuffers;          // one per swapchain image

    std::array<VkClearValue, 2> m_clearValues{};        // [0] colour, [1] depth
};
