#include "shader_pipeline.h"

#include "mesh.h"            // Vertex binding/attribute descriptions
#include "vulkan_context.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// SHADER_DIR is injected by CMake and points at the directory holding the
// compiled SPIR-V (shaders/compiled). Baking the path in keeps the program
// runnable from any working directory for this local course build.
#ifndef SHADER_DIR
#define SHADER_DIR "shaders/compiled"
#endif

namespace {

// Read a compiled SPIR-V file into a byte buffer. SPIR-V is binary, so the file
// is opened in binary mode; we seek to the end first to size the buffer in one
// allocation. See Glossary: SPIR_V
std::vector<char> readFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open shader file: " + path);
    }
    const size_t size = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(size));
    return buffer;
}

// Wrap a blob of SPIR-V bytecode in a VkShaderModule — the handle the pipeline
// references. The module is a thin container; the real compile-to-machine-code
// happens when the pipeline is built. See Glossary: SPIR_V, SHADER
VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size();  // size in bytes
    // SPIR-V is a stream of 32-bit words; the data is suitably aligned because
    // std::vector's allocation satisfies the strictest fundamental alignment.
    info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &info, nullptr, &module) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateShaderModule failed");
    }
    return module;
}

} // namespace

ShaderPipeline::ShaderPipeline(VulkanContext& context, VkRenderPass renderPass,
                               VkDescriptorSetLayout descriptorSetLayout)
    : m_context(context) {
    const VkDevice device = m_context.device();

    // Load and wrap the two shader stages. Modules are only needed during
    // pipeline creation, so they are destroyed at the end of this function.
    // See Glossary: VERTEX_SHADER, FRAGMENT_SHADER
    VkShaderModule vert = createShaderModule(device, readFile(std::string(SHADER_DIR) + "/mesh.vert.spv"));
    VkShaderModule frag = createShaderModule(device, readFile(std::string(SHADER_DIR) + "/mesh.frag.spv"));

    // Describe each programmable stage: which module, which entry point, and
    // which point in the pipeline it plugs into. See Glossary: PROGRAMMABLE_PIPELINE_STAGES
    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vert;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = frag;
    fragStage.pName = "main";

    const VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

    // Vertex input: describe how the pipeline pulls vertices out of the bound
    // vertex buffer — the binding (stride / input rate) and each attribute's
    // location, format, and offset, taken from the Vertex struct.
    // See Glossary: VERTEX_BUFFER, VERTEX_ATTRIBUTES
    const VkVertexInputBindingDescription bindingDesc = Vertex::bindingDescription();
    const auto attributeDescs = Vertex::attributeDescriptions();
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescs.size());
    vertexInput.pVertexAttributeDescriptions = attributeDescs.data();

    // Input assembly: how vertices are grouped into primitives. A triangle list
    // makes every 3 vertices one triangle. See Glossary: RASTERISATION
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Viewport + scissor: declared as one of each, but their actual values are
    // left dynamic (set per-frame in the command buffer). This decouples the
    // pipeline from the window size. See Glossary: VIEWPORT, SCISSOR
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Rasteriser: turns triangles into fragments. Fill the interior, draw both
    // faces (no culling — the hardcoded triangle's winding is not guaranteed),
    // standard 1px line width. See Glossary: RASTERISER, RASTERISATION
    VkPipelineRasterizationStateCreateInfo rasteriser{};
    rasteriser.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasteriser.polygonMode = VK_POLYGON_MODE_FILL;
    // Cull back faces: triangles facing away from the camera are skipped, roughly
    // halving fragment work for a closed mesh. Our cube's faces are wound
    // counter-clockwise when seen from outside, and the projection matrix's Y-flip
    // (camera.cpp) preserves that winding on screen — so COUNTER_CLOCKWISE marks
    // the front faces. (Chunk 7 used CLOCKWISE because there was no projection and
    // thus no Y-flip; adding the MVP in Chunk 8 inverted it.)
    // See Glossary: BACK_FACE_CULLING, WINDING_ORDER
    rasteriser.cullMode = VK_CULL_MODE_BACK_BIT;
    rasteriser.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasteriser.lineWidth = 1.0f;

    // Multisampling: disabled (one sample per pixel). See Glossary: RASTERISATION
    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth/stencil: enable the depth test against our depth attachment. Keep
    // fragments that are nearer (LESS) and write their depth. See Glossary: DEPTH_TESTING
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    // Colour blending: disabled, so a fragment's colour overwrites whatever is in
    // the attachment. The write mask still enables all four channels.
    // See Glossary: COLOUR_BLENDING
    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colourBlend{};
    colourBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colourBlend.attachmentCount = 1;
    colourBlend.pAttachments = &blendAttachment;

    // The states we will set dynamically in the command buffer instead of baking
    // into the pipeline. See Glossary: VIEWPORT, SCISSOR
    const VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    // The pipeline layout: references the one descriptor set layout (the per-frame
    // MVP uniform buffer at set 0). Still no push constants. This is the contract
    // that lets vkCmdBindDescriptorSets attach matching sets at draw time.
    // See Glossary: PIPELINE_LAYOUT, DESCRIPTOR_SET_LAYOUT
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &descriptorSetLayout;
    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_layout) != VK_SUCCESS) {
        throw std::runtime_error("vkCreatePipelineLayout failed");
    }

    // Assemble the whole pipeline as a single object. Vulkan compiles all of this
    // into one immutable GPU state block up front, which is why state changes are
    // cheap at draw time. See Glossary: GRAPHICS_PIPELINE, FIXED_FUNCTION_PIPELINE
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasteriser;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colourBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_layout;
    pipelineInfo.renderPass = renderPass;  // the pipeline is built for this render pass
    pipelineInfo.subpass = 0;              // and this subpass within it

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateGraphicsPipelines failed");
    }

    // Modules are baked into the pipeline now; the handles are no longer needed.
    vkDestroyShaderModule(device, frag, nullptr);
    vkDestroyShaderModule(device, vert, nullptr);

    std::cout << "[vulkan] graphics pipeline created\n";
}

ShaderPipeline::~ShaderPipeline() {
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_context.device(), m_pipeline, nullptr);
    }
    if (m_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_context.device(), m_layout, nullptr);
    }
}
