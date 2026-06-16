#include "renderer.h"

#include "camera.h"
#include "mesh.h"
#include "render_pass.h"
#include "shader_pipeline.h"
#include "swapchain.h"
#include "uniform_buffer.h"
#include "vulkan_context.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>

Renderer::Renderer(VulkanContext& context, const Swapchain& swapchain)
    : m_context(context) {
    createCommandResources();
    createSyncObjects(swapchain.imageCount());
}

Renderer::~Renderer() {
    // The GPU may still be working; wait before destroying anything it uses.
    m_context.waitIdle();
    destroySyncObjects();
    // Destroying the pool frees every command buffer allocated from it.
    if (m_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_context.device(), m_commandPool, nullptr);
    }
}

void Renderer::createCommandResources() {
    // The command pool is tied to the graphics queue family — buffers allocated
    // from it can only be submitted to that family's queues. The RESET flag lets
    // us re-record the single command buffer each frame instead of allocating a
    // fresh one. See Glossary: COMMAND_POOL
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = *m_context.queueFamilies().graphics;
    if (vkCreateCommandPool(m_context.device(), &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateCommandPool failed");
    }

    // Allocate one primary command buffer per frame in flight — the kind that
    // can be submitted to a queue directly. See Glossary: COMMAND_BUFFER
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = kMaxFramesInFlight;
    if (vkAllocateCommandBuffers(m_context.device(), &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("vkAllocateCommandBuffers failed");
    }
}

void Renderer::createSyncObjects(uint32_t imageCount) {
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    // The in-flight fence is created already signalled so each frame slot's very
    // first wait returns immediately instead of deadlocking. See Glossary: FENCE
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    // Per-frame primitives: one imageAvailable semaphore and one inFlight fence
    // for each frame that may be in flight at once. See Glossary: FRAMES_IN_FLIGHT
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        // imageAvailable signals (GPU-side) when the acquired swapchain image is
        // ready to be rendered into. See Glossary: SEMAPHORE
        if (vkCreateSemaphore(m_context.device(), &semInfo, nullptr, &m_imageAvailable[i]) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateSemaphore (imageAvailable) failed");
        }
        if (vkCreateFence(m_context.device(), &fenceInfo, nullptr, &m_inFlight[i]) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateFence failed");
        }
    }

    // One renderFinished semaphore per swapchain image. Tying it to the image
    // (not the frame) avoids reusing a semaphore the presentation engine might
    // still be waiting on. See Glossary: SEMAPHORE
    m_renderFinished.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i) {
        if (vkCreateSemaphore(m_context.device(), &semInfo, nullptr, &m_renderFinished[i]) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateSemaphore (renderFinished) failed");
        }
    }
}

void Renderer::destroySyncObjects() {
    for (VkSemaphore s : m_renderFinished) {
        vkDestroySemaphore(m_context.device(), s, nullptr);
    }
    m_renderFinished.clear();
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        if (m_imageAvailable[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_context.device(), m_imageAvailable[i], nullptr);
            m_imageAvailable[i] = VK_NULL_HANDLE;
        }
        if (m_inFlight[i] != VK_NULL_HANDLE) {
            vkDestroyFence(m_context.device(), m_inFlight[i], nullptr);
            m_inFlight[i] = VK_NULL_HANDLE;
        }
    }
}

void Renderer::onSwapchainRecreated(const Swapchain& swapchain) {
    // The caller has already waited for the device to be idle. Rebuild the sync
    // objects in case the image count changed.
    destroySyncObjects();
    createSyncObjects(swapchain.imageCount());
}

void Renderer::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex,
                                   Swapchain& swapchain, RenderPass& renderPass,
                                   ShaderPipeline& pipeline, const Mesh& mesh,
                                   VkDescriptorSet descriptorSet) {
    // Recording starts here: every vkCmd* call below appends a command to be run
    // later on the GPU, rather than executing now. See Glossary: COMMAND_BUFFER
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS) {
        throw std::runtime_error("vkBeginCommandBuffer failed");
    }

    const VkExtent2D extent = swapchain.extent();

    // Begin the render pass: this performs the attachments' load ops (clearing
    // colour + depth) and binds the framebuffer for this image.
    // See Glossary: RENDER_PASS, FRAMEBUFFER
    VkRenderPassBeginInfo rpBegin{};
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass = renderPass.handle();
    rpBegin.framebuffer = renderPass.framebuffer(imageIndex);
    rpBegin.renderArea.offset = {0, 0};
    rpBegin.renderArea.extent = extent;
    rpBegin.clearValueCount = static_cast<uint32_t>(renderPass.clearValues().size());
    rpBegin.pClearValues = renderPass.clearValues().data();
    vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

    // Bind the pipeline that turns our draw call into shaded fragments.
    // See Glossary: GRAPHICS_PIPELINE
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle());

    // Set the viewport and scissor now (they were left dynamic in the pipeline),
    // sized to the current swapchain extent so resizing needs no pipeline
    // rebuild. See Glossary: VIEWPORT, SCISSOR
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Bind this frame's descriptor set (the MVP uniform buffer) to set 0, so the
    // vertex shader can read the matrices. It must use the same pipeline layout the
    // pipeline was built with. See Glossary: DESCRIPTOR_SET, PIPELINE_LAYOUT
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout(),
                            0, 1, &descriptorSet, 0, nullptr);

    // Bind the mesh's vertex + index buffers and draw it (indexed). The
    // hardcoded triangle of Chunk 5 is gone; geometry now comes from GPU buffers.
    // See Glossary: VERTEX_BUFFER, INDEX_BUFFER, MESH
    mesh.bindAndDraw(cmd);

    vkCmdEndRenderPass(cmd);
    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        throw std::runtime_error("vkEndCommandBuffer failed");
    }
}

bool Renderer::drawFrame(Swapchain& swapchain, RenderPass& renderPass, ShaderPipeline& pipeline,
                         const Mesh& mesh, const Camera& camera, UniformBuffers& uniforms) {
    const VkDevice device = m_context.device();
    const uint64_t noTimeout = std::numeric_limits<uint64_t>::max();

    // All per-frame resources are indexed by the current in-flight slot, so two
    // consecutive frames use independent command buffers and fences and never
    // collide. See Glossary: FRAMES_IN_FLIGHT
    const uint32_t frame = m_currentFrame;
    VkCommandBuffer cmd = m_commandBuffers[frame];

    // Wait until this slot's previous frame has finished on the GPU, so we do not
    // overwrite resources it is still reading. With two slots, slot N only blocks
    // on the frame from two iterations ago, so the CPU keeps running ahead.
    // See Glossary: FENCE
    vkWaitForFences(device, 1, &m_inFlight[frame], VK_TRUE, noTimeout);

    // Acquire the next image to draw into; the GPU will signal imageAvailable
    // when it is actually ready. See Glossary: SWAPCHAIN, SEMAPHORE
    uint32_t imageIndex = 0;
    VkResult acquire = vkAcquireNextImageKHR(device, swapchain.handle(), noTimeout,
                                             m_imageAvailable[frame], VK_NULL_HANDLE, &imageIndex);
    if (acquire == VK_ERROR_OUT_OF_DATE_KHR) {
        // The swapchain no longer matches the surface (e.g. just resized). Tell
        // the caller to recreate it; we drew nothing this frame.
        return true;
    }
    if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("vkAcquireNextImageKHR failed");
    }

    // Only reset the fence once we are committed to submitting work that will
    // re-signal it; resetting earlier would deadlock if we returned above.
    vkResetFences(device, 1, &m_inFlight[frame]);

    // Update this frame's MVP matrices and upload them to its uniform buffer
    // before recording the draw that reads them. The model matrix is identity for
    // now (the cube sits where its vertices put it — no rotation yet); the camera
    // supplies view and projection, the latter using the current aspect ratio so
    // the cube never stretches when the window resizes. See Glossary: MVP_MATRIX
    const VkExtent2D extent = swapchain.extent();
    const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view = camera.viewMatrix();
    ubo.proj = camera.projectionMatrix(aspect);
    uniforms.update(frame, ubo);

    vkResetCommandBuffer(cmd, 0);
    recordCommandBuffer(cmd, imageIndex, swapchain, renderPass, pipeline, mesh,
                        uniforms.descriptorSet(frame));

    // Submit the recorded work to the graphics queue. It waits on imageAvailable
    // at the colour-output stage (so vertex work can start earlier) and signals
    // renderFinished for this image when done, plus the inFlight fence for the
    // CPU. See Glossary: SUBMISSION_QUEUE, SEMAPHORE, FENCE
    VkSemaphore waitSemaphores[] = {m_imageAvailable[frame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signalSemaphores[] = {m_renderFinished[imageIndex]};

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = waitSemaphores;
    submit.pWaitDstStageMask = waitStages;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = signalSemaphores;
    if (vkQueueSubmit(m_context.graphicsQueue(), 1, &submit, m_inFlight[frame]) != VK_SUCCESS) {
        throw std::runtime_error("vkQueueSubmit failed");
    }

    // Present the finished image: the presentation engine waits on renderFinished
    // before showing it. See Glossary: PRESENT_MODE, SWAPCHAIN
    VkSwapchainKHR swapchains[] = {swapchain.handle()};
    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = signalSemaphores;
    present.swapchainCount = 1;
    present.pSwapchains = swapchains;
    present.pImageIndices = &imageIndex;

    VkResult presentResult = vkQueuePresentKHR(m_context.presentQueue(), &present);

    // Advance to the next in-flight slot for the following frame. Done after a
    // successful submit so the slots rotate 0,1,0,1,…. See Glossary: FRAMES_IN_FLIGHT
    m_currentFrame = (m_currentFrame + 1) % kMaxFramesInFlight;

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        return true;  // recreate the swapchain
    }
    if (presentResult != VK_SUCCESS) {
        throw std::runtime_error("vkQueuePresentKHR failed");
    }
    return false;
}
