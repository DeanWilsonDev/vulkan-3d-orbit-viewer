#include "uniform_buffer.h"

#include "vulkan_context.h"

#include <cstring>
#include <stdexcept>

UniformBuffers::UniformBuffers(VulkanContext& context, uint32_t frameCount)
    : m_context(context) {
    createDescriptorSetLayout();
    createBuffers(frameCount);
    createDescriptorPool(frameCount);
    createDescriptorSets(frameCount);
}

UniformBuffers::~UniformBuffers() {
    const VkDevice device = m_context.device();
    // Descriptor sets are freed implicitly when the pool is destroyed, so we only
    // destroy the pool and the layout, then the buffers + their memory.
    if (m_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_pool, nullptr);
    }
    if (m_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_layout, nullptr);
    }
    for (size_t i = 0; i < m_buffers.size(); ++i) {
        // Mapped memory does not need an explicit unmap before free, but freeing
        // is required. The buffer and its backing memory are freed separately.
        vkDestroyBuffer(device, m_buffers[i], nullptr);
        vkFreeMemory(device, m_memories[i], nullptr);
    }
}

void UniformBuffers::createDescriptorSetLayout() {
    // One binding: a single uniform buffer at binding 0, read by both the vertex
    // shader (MVP transform, normal to world space) and — as of Chunk 11 — the
    // fragment shader (the light parameters for shading). descriptorCount is 1
    // because the shader declares one block, not an array.
    // See Glossary: DESCRIPTOR_SET_LAYOUT, DESCRIPTOR
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 1;
    info.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(m_context.device(), &info, nullptr, &m_layout) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateDescriptorSetLayout failed");
    }
}

void UniformBuffers::createBuffers(uint32_t frameCount) {
    const VkDeviceSize size = sizeof(UniformBufferObject);
    m_buffers.resize(frameCount);
    m_memories.resize(frameCount);
    m_mapped.resize(frameCount);

    for (uint32_t i = 0; i < frameCount; ++i) {
        // Host-visible + coherent so the CPU can write the matrices each frame and
        // the GPU sees them without a manual flush. Uniform buffers are tiny and
        // rewritten every frame, so the staging-to-device-local dance the mesh
        // uses would be pure overhead here. See Glossary: UNIFORM_BUFFER, HOST_VISIBLE_MEMORY
        m_context.createBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                               m_buffers[i], m_memories[i]);
        // Map once and keep the pointer for the object's lifetime ("persistent
        // mapping") — re-mapping every frame would be wasted work.
        vkMapMemory(m_context.device(), m_memories[i], 0, size, 0, &m_mapped[i]);
    }
}

void UniformBuffers::createDescriptorPool(uint32_t frameCount) {
    // The pool must have room for one uniform-buffer descriptor per frame, and be
    // able to hand out frameCount sets in total. See Glossary: DESCRIPTOR_POOL
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = frameCount;

    VkDescriptorPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.poolSizeCount = 1;
    info.pPoolSizes = &poolSize;
    info.maxSets = frameCount;
    if (vkCreateDescriptorPool(m_context.device(), &info, nullptr, &m_pool) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateDescriptorPool failed");
    }
}

void UniformBuffers::createDescriptorSets(uint32_t frameCount) {
    // Allocate one set per frame, every one using the same layout.
    std::vector<VkDescriptorSetLayout> layouts(frameCount, m_layout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_pool;
    allocInfo.descriptorSetCount = frameCount;
    allocInfo.pSetLayouts = layouts.data();

    m_descriptorSets.resize(frameCount);
    if (vkAllocateDescriptorSets(m_context.device(), &allocInfo, m_descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("vkAllocateDescriptorSets failed");
    }

    // Point each set at its frame's buffer. Allocation makes an empty set; this
    // write is what actually binds the resource into it. See Glossary: DESCRIPTOR_SET
    for (uint32_t i = 0; i < frameCount; ++i) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_buffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_descriptorSets[i];
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;
        vkUpdateDescriptorSets(m_context.device(), 1, &write, 0, nullptr);
    }
}

void UniformBuffers::update(uint32_t frame, const UniformBufferObject& ubo) {
    std::memcpy(m_mapped[frame], &ubo, sizeof(ubo));
}
