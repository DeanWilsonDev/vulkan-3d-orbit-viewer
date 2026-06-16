#include "uniform_buffer.h"

#include "vulkan_context.h"

#include <cstring>
#include <stdexcept>

UniformBuffers::UniformBuffers(VulkanContext& context, uint32_t frameCount,
                               VkImageView textureView, VkSampler textureSampler)
    : m_context(context) {
    createDescriptorSetLayout();
    createBuffers(frameCount);
    createDescriptorPool(frameCount);
    createDescriptorSets(frameCount, textureView, textureSampler);
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
    // Binding 0: the uniform buffer, read by the vertex shader (MVP, normal) and the
    // fragment shader (light). descriptorCount is 1 — one block, not an array.
    // See Glossary: DESCRIPTOR_SET_LAYOUT, DESCRIPTOR
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    // Binding 1: the diffuse texture as a combined image sampler — one descriptor that
    // packages the image view and the sampler together, read only by the fragment
    // shader. See Glossary: COMBINED_IMAGE_SAMPLER, SAMPLER, TEXTURE
    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 1;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    const VkDescriptorSetLayoutBinding bindings[] = {uboBinding, samplerBinding};
    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 2;
    info.pBindings = bindings;
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
    // The pool needs room for one uniform-buffer descriptor and one combined image
    // sampler per frame, and to hand out frameCount sets in total. Each descriptor
    // type the sets use needs its own pool size. See Glossary: DESCRIPTOR_POOL
    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = frameCount;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = frameCount;

    VkDescriptorPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.poolSizeCount = 2;
    info.pPoolSizes = poolSizes;
    info.maxSets = frameCount;
    if (vkCreateDescriptorPool(m_context.device(), &info, nullptr, &m_pool) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateDescriptorPool failed");
    }
}

void UniformBuffers::createDescriptorSets(uint32_t frameCount, VkImageView textureView,
                                          VkSampler textureSampler) {
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

    // Point each set at its frame's buffer (binding 0) and the shared texture
    // (binding 1). Allocation makes empty sets; these writes bind the resources in.
    // The texture is the same for every frame; only the uniform buffer differs.
    // See Glossary: DESCRIPTOR_SET, COMBINED_IMAGE_SAMPLER
    for (uint32_t i = 0; i < frameCount; ++i) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_buffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textureView;
        imageInfo.sampler = textureSampler;

        VkWriteDescriptorSet writes[2]{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = m_descriptorSets[i];
        writes[0].dstBinding = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo = &bufferInfo;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = m_descriptorSets[i];
        writes[1].dstBinding = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_context.device(), 2, writes, 0, nullptr);
    }
}

void UniformBuffers::update(uint32_t frame, const UniformBufferObject& ubo) {
    std::memcpy(m_mapped[frame], &ubo, sizeof(ubo));
}
