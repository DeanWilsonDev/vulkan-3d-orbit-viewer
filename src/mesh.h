#pragma once

// Mesh — a piece of geometry living in GPU memory: a vertex buffer (the corners)
// and an index buffer (which corners form each triangle), both in fast
// device-local memory. See Glossary: MESH, VERTEX_BUFFER, INDEX_BUFFER
//
// Chunk 7 uploads a hardcoded cube. Chunk 9 will load the same buffers from an
// OBJ file instead; the GPU side stays identical.

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <vector>

class VulkanContext;

// One vertex and everything it carries. The fields here are the per-vertex
// "attributes": where the vertex is, which way its surface faces, and where it
// maps into a texture. See Glossary: VERTEX_ATTRIBUTES
struct Vertex {
    glm::vec3 position;   // object-space position
    glm::vec3 normal;     // surface direction, used for lighting from Chunk 11
    glm::vec2 uv;         // texture coordinate, carried but not sampled in this project

    // How the pipeline reads one vertex from the buffer: the stride (bytes per
    // vertex) and that the data advances per-vertex (not per-instance).
    // See Glossary: VERTEX_BUFFER
    static VkVertexInputBindingDescription bindingDescription();

    // How each attribute is found within a vertex: its shader location, format,
    // and byte offset. See Glossary: VERTEX_ATTRIBUTES
    static std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions();
};

class Mesh {
public:
    // Upload the given geometry to the GPU via the staging-buffer pattern.
    Mesh(VulkanContext& context, const std::vector<Vertex>& vertices,
         const std::vector<uint32_t>& indices);
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    // Bind this mesh's vertex + index buffers and issue the indexed draw into the
    // given command buffer. See Glossary: INDEX_BUFFER
    void bindAndDraw(VkCommandBuffer cmd) const;

    // Build the hardcoded cube used in Chunk 7. (C++17 guaranteed copy elision
    // lets this return by value despite Mesh being non-copyable.)
    static Mesh cube(VulkanContext& context);

private:
    // Create a device-local buffer of `usage` and fill it from `data` through a
    // temporary host-visible staging buffer. See Glossary: STAGING_BUFFER
    void uploadDeviceLocal(const void* data, VkDeviceSize size, VkBufferUsageFlags usage,
                           VkBuffer& buffer, VkDeviceMemory& memory);

    VulkanContext& m_context;

    VkBuffer       m_vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexMemory = VK_NULL_HANDLE;
    VkBuffer       m_indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_indexMemory = VK_NULL_HANDLE;
    uint32_t       m_indexCount = 0;
};
