#pragma once

// UniformBuffers — the bridge that gets the per-frame MVP matrices from the CPU
// into the vertex shader. It owns three things that work together:
//
//   * one uniform buffer per frame in flight — a small chunk of GPU-visible
//     memory holding the matrices that every vertex of a draw shares;
//   * a descriptor set layout — the blueprint declaring "the shader expects one
//     uniform buffer at set 0, binding 0";
//   * a descriptor pool and one descriptor set per frame — the bound handles that
//     actually point a shader at a specific buffer.
//
// One buffer/descriptor-set *per frame in flight* (not per swapchain image): the
// CPU must be free to write next frame's matrices without touching memory the GPU
// is still reading for the frame before. See Glossary: UNIFORM_BUFFER, DESCRIPTOR,
// DESCRIPTOR_SET, DESCRIPTOR_SET_LAYOUT, DESCRIPTOR_POOL, FRAMES_IN_FLIGHT

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

class VulkanContext;

// The data the vertex shader reads once per draw. The field order and types must
// match the `uniform` block in mesh.vert exactly. Each mat4 is 64 bytes and
// 16-byte aligned, so the three pack back-to-back with no std140 padding needed.
// See Glossary: UNIFORM_BUFFER, MVP_MATRIX, MODEL_MATRIX, VIEW_MATRIX, PROJECTION_MATRIX
struct UniformBufferObject {
    glm::mat4 model;   // object space → world space
    glm::mat4 view;    // world space → view (camera) space
    glm::mat4 proj;    // view space → clip space
};

class UniformBuffers {
public:
    // Allocate one buffer + descriptor set for each of `frameCount` frames in
    // flight. The buffers are host-visible and stay mapped for the object's whole
    // life, so updates are a plain memcpy. Throws std::runtime_error on failure.
    UniformBuffers(VulkanContext& context, uint32_t frameCount);
    ~UniformBuffers();

    UniformBuffers(const UniformBuffers&) = delete;
    UniformBuffers& operator=(const UniformBuffers&) = delete;

    // Copy `ubo` into the given frame's buffer. HOST_COHERENT memory makes the
    // write visible to the GPU without an explicit flush. See Glossary: UNIFORM_BUFFER
    void update(uint32_t frame, const UniformBufferObject& ubo);

    // The layout the pipeline is built against, and the set to bind for a frame.
    VkDescriptorSetLayout layout() const { return m_layout; }
    VkDescriptorSet       descriptorSet(uint32_t frame) const { return m_descriptorSets[frame]; }

private:
    void createDescriptorSetLayout();
    void createBuffers(uint32_t frameCount);
    void createDescriptorPool(uint32_t frameCount);
    void createDescriptorSets(uint32_t frameCount);

    VulkanContext& m_context;

    VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
    VkDescriptorPool      m_pool = VK_NULL_HANDLE;

    // Per-frame, all the same length (frameCount). m_mapped[i] is a persistent
    // pointer into m_buffers[i]'s memory. See Glossary: HOST_VISIBLE_MEMORY
    std::vector<VkBuffer>        m_buffers;
    std::vector<VkDeviceMemory>  m_memories;
    std::vector<void*>           m_mapped;
    std::vector<VkDescriptorSet> m_descriptorSets;
};
