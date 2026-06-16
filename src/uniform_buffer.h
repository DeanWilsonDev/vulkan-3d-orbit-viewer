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

// The data the shaders read once per draw. The field order and types must match the
// `uniform` block in mesh.vert / mesh.frag exactly, under std140 layout rules.
// mat4 is 64 bytes / 16-byte aligned and vec4 is 16 / 16, so everything packs
// back-to-back with no padding. The lighting vectors are vec4 (not vec3) precisely
// to dodge std140's awkward vec3 alignment; their w components are unused.
// See Glossary: UNIFORM_BUFFER, MVP_MATRIX, NORMAL_MATRIX, DIRECTIONAL_LIGHT
struct UniformBufferObject {
    glm::mat4 model;          // object space → world space
    glm::mat4 view;           // world space → view (camera) space
    glm::mat4 proj;           // view space → clip space

    // The normal matrix: transpose(inverse(mat3(model))), stored as a mat4 to avoid
    // std140's per-column mat3 padding. The shader takes its mat3. It transforms
    // normals correctly even under non-uniform scale. See Glossary: NORMAL_MATRIX
    glm::mat4 normalMatrix;

    // A single directional light, fixed in world space. direction points *toward*
    // the light (so dot(normal, direction) is positive on lit faces); colour is its
    // intensity; ambient is a constant fill so shadowed faces are not pure black.
    // See Glossary: DIRECTIONAL_LIGHT, AMBIENT_LIGHT
    glm::vec4 lightDirection; // xyz = direction toward the light (normalised); w unused
    glm::vec4 lightColour;    // rgb = light colour/intensity;                   w unused
    glm::vec4 ambientColour;  // rgb = ambient fill;                             w unused
};

class UniformBuffers {
public:
    // Allocate one buffer + descriptor set for each of `frameCount` frames in
    // flight. Each set binds the per-frame uniform buffer (binding 0) and the shared
    // diffuse texture (binding 1, via textureView + textureSampler). The buffers are
    // host-visible and stay mapped for the object's whole life, so updates are a
    // plain memcpy. Throws std::runtime_error on failure.
    UniformBuffers(VulkanContext& context, uint32_t frameCount,
                   VkImageView textureView, VkSampler textureSampler);
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
    void createDescriptorSets(uint32_t frameCount, VkImageView textureView, VkSampler textureSampler);

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
