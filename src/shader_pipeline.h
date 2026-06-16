#pragma once

// ShaderPipeline — loads the compiled shaders and builds the graphics pipeline:
// the single immutable object that bundles the shader stages together with every
// fixed-function setting (vertex input, rasteriser, depth test, blending, …) the
// GPU needs to turn draw calls into pixels. See Glossary: GRAPHICS_PIPELINE
//
// The pipeline depends on the render pass and the shaders, but NOT on the window
// size: viewport and scissor are left dynamic and set per-frame, so the pipeline
// survives a resize and never needs rebuilding here.

#include <vulkan/vulkan.h>

class VulkanContext;

class ShaderPipeline {
public:
    // Build the pipeline for the given render pass. descriptorSetLayout is the
    // layout the shaders' uniforms are declared against (set 0); it becomes part
    // of the pipeline layout so descriptor sets can be bound at draw time. Throws
    // std::runtime_error if the shaders cannot be loaded or the pipeline cannot be
    // created.
    ShaderPipeline(VulkanContext& context, VkRenderPass renderPass,
                   VkDescriptorSetLayout descriptorSetLayout);

    // Destroy the pipeline and its layout.
    ~ShaderPipeline();

    ShaderPipeline(const ShaderPipeline&) = delete;
    ShaderPipeline& operator=(const ShaderPipeline&) = delete;

    VkPipeline       handle() const { return m_pipeline; }
    VkPipelineLayout layout() const { return m_layout; }

private:
    VulkanContext& m_context;

    // The pipeline layout declares what external resources (descriptor sets,
    // push constants) the shaders read. As of Chunk 8 it references one descriptor
    // set layout — the per-frame MVP uniform buffer. The layout object itself is
    // owned by UniformBuffers; the pipeline only borrows it. See Glossary: PIPELINE_LAYOUT
    VkPipelineLayout m_layout = VK_NULL_HANDLE;

    // The compiled, immutable graphics pipeline. See Glossary: GRAPHICS_PIPELINE
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};
