#include "mesh.h"

#include "vulkan_context.h"

#include <cstring>
#include <stdexcept>

VkVertexInputBindingDescription Vertex::bindingDescription() {
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;                              // we use a single vertex buffer, bound at slot 0
    binding.stride = sizeof(Vertex);                  // bytes from one vertex to the next
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;  // advance per vertex (not per instance)
    return binding;
}

std::array<VkVertexInputAttributeDescription, 3> Vertex::attributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 3> attrs{};

    // location 0: position (vec3). offsetof tells the GPU where in each vertex
    // this attribute begins. See Glossary: VERTEX_ATTRIBUTES
    attrs[0].location = 0;
    attrs[0].binding = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;     // three 32-bit floats
    attrs[0].offset = offsetof(Vertex, position);

    // location 1: normal (vec3) — consumed from Chunk 11 (lighting).
    attrs[1].location = 1;
    attrs[1].binding = 0;
    attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset = offsetof(Vertex, normal);

    // location 2: uv (vec2) — carried but not sampled in this project.
    attrs[2].location = 2;
    attrs[2].binding = 0;
    attrs[2].format = VK_FORMAT_R32G32_SFLOAT;        // two 32-bit floats
    attrs[2].offset = offsetof(Vertex, uv);

    return attrs;
}

Mesh::Mesh(VulkanContext& context, const std::vector<Vertex>& vertices,
           const std::vector<uint32_t>& indices)
    : m_context(context), m_indexCount(static_cast<uint32_t>(indices.size())) {
    // Both buffers are uploaded the same way: stage in host-visible memory, then
    // copy into device-local memory for fast GPU access. See Glossary: STAGING_BUFFER
    uploadDeviceLocal(vertices.data(), sizeof(Vertex) * vertices.size(),
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, m_vertexBuffer, m_vertexMemory);
    uploadDeviceLocal(indices.data(), sizeof(uint32_t) * indices.size(),
                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT, m_indexBuffer, m_indexMemory);
}

Mesh::~Mesh() {
    vkDestroyBuffer(m_context.device(), m_indexBuffer, nullptr);
    vkFreeMemory(m_context.device(), m_indexMemory, nullptr);
    vkDestroyBuffer(m_context.device(), m_vertexBuffer, nullptr);
    vkFreeMemory(m_context.device(), m_vertexMemory, nullptr);
}

void Mesh::uploadDeviceLocal(const void* data, VkDeviceSize size, VkBufferUsageFlags usage,
                             VkBuffer& buffer, VkDeviceMemory& memory) {
    const VkDevice device = m_context.device();

    // 1. A staging buffer in host-visible memory the CPU can write to directly.
    //    HOST_COHERENT means writes are visible to the GPU without a manual flush.
    //    See Glossary: STAGING_BUFFER, HOST_VISIBLE_MEMORY
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    m_context.createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           staging, stagingMemory);

    // 2. Map it, copy the data in, unmap.
    void* mapped = nullptr;
    vkMapMemory(device, stagingMemory, 0, size, 0, &mapped);
    std::memcpy(mapped, data, static_cast<size_t>(size));
    vkUnmapMemory(device, stagingMemory);

    // 3. The real buffer in device-local memory — fast for the GPU, but not
    //    writable by the CPU, which is exactly why we needed the staging step.
    //    TRANSFER_DST lets it receive the copy. See Glossary: DEVICE_LOCAL_MEMORY
    m_context.createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer, memory);

    // 4. Copy staging → device-local on the GPU, then drop the staging buffer.
    m_context.copyBuffer(staging, buffer, size);
    vkDestroyBuffer(device, staging, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
}

void Mesh::bindAndDraw(VkCommandBuffer cmd) const {
    // Bind the vertex buffer to binding slot 0 (matching the pipeline's binding
    // description) and the index buffer, then draw using the indices.
    // See Glossary: VERTEX_BUFFER, INDEX_BUFFER
    VkBuffer vertexBuffers[] = {m_vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmd, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Draw m_indexCount indices: the GPU reads each index, fetches that vertex,
    // and assembles triangles — reusing shared corners instead of duplicating
    // them. See Glossary: INDEX_BUFFER
    vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);
}

Mesh Mesh::cube(VulkanContext& context) {
    // A unit-ish cube placed directly in clip space (no transforms exist yet in
    // Chunk 7). It is centred at z = 0.5 with z in [0.2, 0.8] so the whole cube
    // sits inside Vulkan's clip volume (NDC z runs 0..1); x and y span ±0.4.
    // 24 vertices — four per face — so each face can carry its own flat normal.
    // See Glossary: MESH, WINDING_ORDER
    const float a = 0.4f;     // half-extent in x and y
    const float zn = 0.2f;    // near face z
    const float zf = 0.8f;    // far face z

    // Each face: four corners in counter-clockwise order as seen from *outside*
    // the cube, with the face's outward normal. UVs are a simple 0..1 square.
    // See Glossary: WINDING_ORDER, BACK_FACE_CULLING
    const std::vector<Vertex> vertices = {
        // Front face (−Z, the near face), normal (0,0,−1)
        {{-a, -a, zn}, {0, 0, -1}, {0, 0}}, {{ a, -a, zn}, {0, 0, -1}, {1, 0}},
        {{ a,  a, zn}, {0, 0, -1}, {1, 1}}, {{-a,  a, zn}, {0, 0, -1}, {0, 1}},
        // Back face (+Z), normal (0,0,1)
        {{ a, -a, zf}, {0, 0, 1}, {0, 0}}, {{-a, -a, zf}, {0, 0, 1}, {1, 0}},
        {{-a,  a, zf}, {0, 0, 1}, {1, 1}}, {{ a,  a, zf}, {0, 0, 1}, {0, 1}},
        // Left face (−X), normal (−1,0,0)
        {{-a, -a, zf}, {-1, 0, 0}, {0, 0}}, {{-a, -a, zn}, {-1, 0, 0}, {1, 0}},
        {{-a,  a, zn}, {-1, 0, 0}, {1, 1}}, {{-a,  a, zf}, {-1, 0, 0}, {0, 1}},
        // Right face (+X), normal (1,0,0)
        {{ a, -a, zn}, {1, 0, 0}, {0, 0}}, {{ a, -a, zf}, {1, 0, 0}, {1, 0}},
        {{ a,  a, zf}, {1, 0, 0}, {1, 1}}, {{ a,  a, zn}, {1, 0, 0}, {0, 1}},
        // Bottom face (−Y), normal (0,−1,0)
        {{-a, -a, zf}, {0, -1, 0}, {0, 0}}, {{ a, -a, zf}, {0, -1, 0}, {1, 0}},
        {{ a, -a, zn}, {0, -1, 0}, {1, 1}}, {{-a, -a, zn}, {0, -1, 0}, {0, 1}},
        // Top face (+Y), normal (0,1,0)
        {{-a,  a, zn}, {0, 1, 0}, {0, 0}}, {{ a,  a, zn}, {0, 1, 0}, {1, 0}},
        {{ a,  a, zf}, {0, 1, 0}, {1, 1}}, {{-a,  a, zf}, {0, 1, 0}, {0, 1}},
    };

    // Two triangles per face, sharing the diagonal: (0,1,2) and (0,2,3) keep the
    // same winding as the quad. See Glossary: INDEX_BUFFER
    std::vector<uint32_t> indices;
    indices.reserve(36);
    for (uint32_t face = 0; face < 6; ++face) {
        const uint32_t base = face * 4;
        indices.insert(indices.end(),
                       {base + 0, base + 1, base + 2, base + 0, base + 2, base + 3});
    }

    return Mesh(context, vertices, indices);
}
