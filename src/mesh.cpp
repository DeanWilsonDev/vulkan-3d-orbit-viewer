#include "mesh.h"

#include "vulkan_context.h"

#include <tiny_obj_loader.h>
#include <glm/gtx/hash.hpp>   // hashing for glm::vec types (GLM_ENABLE_EXPERIMENTAL)

// stb_image is header-only: defining STB_IMAGE_IMPLEMENTATION in exactly one
// translation unit (here) compiles its implementation. See Glossary: TEXTURE
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <unordered_map>

// Hash for Vertex, so it can be a key in the deduplication map below. It combines
// the per-attribute hashes (GLM provides std::hash for its vector types) with the
// shift-xor recipe from the Vulkan tutorial. See Glossary: VERTEX_DEDUPLICATION
namespace std {
template <>
struct hash<Vertex> {
    size_t operator()(const Vertex& v) const {
        size_t h1 = hash<glm::vec3>()(v.position);
        size_t h2 = hash<glm::vec3>()(v.normal);
        size_t h3 = hash<glm::vec2>()(v.uv);
        return ((h1 ^ (h2 << 1)) >> 1) ^ (h3 << 1);
    }
};
} // namespace std

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
           const std::vector<uint32_t>& indices, const std::string& diffuseTexturePath)
    : m_context(context), m_indexCount(static_cast<uint32_t>(indices.size())),
      m_diffuseTexturePath(diffuseTexturePath) {
    // Compute the object-space bounding box so the camera can be framed on the
    // mesh regardless of its scale or origin (the cube and a loaded OBJ differ
    // wildly here). See Glossary: ASSET_PIPELINE
    if (!vertices.empty()) {
        m_boundsMin = m_boundsMax = vertices[0].position;
        for (const Vertex& v : vertices) {
            m_boundsMin = glm::min(m_boundsMin, v.position);
            m_boundsMax = glm::max(m_boundsMax, v.position);
        }
    }

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

Mesh Mesh::fromObjFile(VulkanContext& context, const std::string& path) {
    // triangulate = true turns any quads or n-gons in the file into triangles,
    // which is all the GPU draws. (The sample model is quad-based, so this matters.)
    // See Glossary: OBJ_FORMAT
    tinyobj::ObjReaderConfig config;
    config.triangulate = true;

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(path, config)) {
        const std::string err = reader.Error();
        throw std::runtime_error("failed to load OBJ '" + path + "': " +
                                 (err.empty() ? "unknown error" : err));
    }
    // Warnings (e.g. a missing material texture we do not use) are informational
    // here, so route them to stdout — stderr is reserved for Vulkan validation.
    if (!reader.Warning().empty()) {
        std::cout << "[obj] " << reader.Warning();
    }

    const tinyobj::attrib_t& attrib = reader.GetAttrib();
    const std::vector<tinyobj::shape_t>& shapes = reader.GetShapes();

    // The OBJ's `mtllib` is parsed too; pull the first material's diffuse map name
    // (MTL `map_Kd`) and resolve it relative to the OBJ's own directory, since MTL
    // paths are relative to the model. Left empty if there is no diffuse map.
    // See Glossary: MTL_FORMAT, DIFFUSE_MAP
    std::string diffuseTexturePath;
    for (const tinyobj::material_t& mat : reader.GetMaterials()) {
        if (!mat.diffuse_texname.empty()) {
            const size_t slash = path.find_last_of("/\\");
            const std::string dir = (slash == std::string::npos) ? "" : path.substr(0, slash + 1);
            diffuseTexturePath = dir + mat.diffuse_texname;
            break;
        }
    }

    // OBJ indexes positions, normals, and UVs in three independent streams; the GPU
    // wants one stream of fully-formed vertices addressed by a single index. We
    // rebuild each corner's vertex and deduplicate identical ones through this map,
    // so a shared corner is stored once. See Glossary: VERTEX_DEDUPLICATION
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::unordered_map<Vertex, uint32_t> uniqueVertices;

    auto positionAt = [&](const tinyobj::index_t& idx) {
        return glm::vec3(attrib.vertices[3 * idx.vertex_index + 0],
                         attrib.vertices[3 * idx.vertex_index + 1],
                         attrib.vertices[3 * idx.vertex_index + 2]);
    };

    size_t cornerCount = 0;
    for (const tinyobj::shape_t& shape : shapes) {
        const std::vector<tinyobj::index_t>& meshIndices = shape.mesh.indices;
        // After triangulation every face is a triangle, so corners come in threes.
        for (size_t i = 0; i + 2 < meshIndices.size(); i += 3) {
            const tinyobj::index_t corners[3] = {meshIndices[i], meshIndices[i + 1],
                                                 meshIndices[i + 2]};

            // Flat-normal fallback: if the file has no normals, derive one per
            // triangle from its winding so the mesh is still flat-shaded rather
            // than unlit. See Glossary: FLAT_NORMALS, SMOOTH_NORMALS
            const bool haveNormals = corners[0].normal_index >= 0 &&
                                     corners[1].normal_index >= 0 &&
                                     corners[2].normal_index >= 0;
            glm::vec3 flatNormal(0.0f, 0.0f, 1.0f);
            if (!haveNormals) {
                const glm::vec3 p0 = positionAt(corners[0]);
                const glm::vec3 edge1 = positionAt(corners[1]) - p0;
                const glm::vec3 edge2 = positionAt(corners[2]) - p0;
                const glm::vec3 n = glm::cross(edge1, edge2);
                const float len = glm::length(n);
                if (len > 0.0f) flatNormal = n / len;
            }

            for (const tinyobj::index_t& idx : corners) {
                Vertex v{};
                v.position = positionAt(idx);
                v.normal = idx.normal_index >= 0
                               ? glm::vec3(attrib.normals[3 * idx.normal_index + 0],
                                           attrib.normals[3 * idx.normal_index + 1],
                                           attrib.normals[3 * idx.normal_index + 2])
                               : flatNormal;
                // Flip V: OBJ texture coordinates put V=0 at the bottom of the image
                // (OpenGL convention), but Vulkan samples with V=0 at the top, so the
                // texture would appear upside-down without this. See Glossary: UV_COORDINATES
                v.uv = idx.texcoord_index >= 0
                           ? glm::vec2(attrib.texcoords[2 * idx.texcoord_index + 0],
                                       1.0f - attrib.texcoords[2 * idx.texcoord_index + 1])
                           : glm::vec2(0.0f);

                ++cornerCount;
                const auto it = uniqueVertices.find(v);
                if (it != uniqueVertices.end()) {
                    indices.push_back(it->second);
                } else {
                    const uint32_t newIndex = static_cast<uint32_t>(vertices.size());
                    uniqueVertices.emplace(v, newIndex);
                    vertices.push_back(v);
                    indices.push_back(newIndex);
                }
            }
        }
    }

    if (vertices.empty()) {
        throw std::runtime_error("OBJ '" + path + "' contained no triangle geometry");
    }

    // Report how much deduplication saved — a closed mesh shares most corners, so
    // the unique count should be well below the corner count.
    std::cout << "[obj] loaded " << path << ": " << cornerCount << " corners -> "
              << vertices.size() << " unique vertices, " << (indices.size() / 3)
              << " triangles"
              << (diffuseTexturePath.empty() ? "" : (", diffuse map " + diffuseTexturePath))
              << "\n";

    return Mesh(context, vertices, indices, diffuseTexturePath);
}

// ============================ Texture ====================================

Texture::Texture(VulkanContext& context, const std::string& path) : m_context(context) {
    int width = 1, height = 1, channels = 0;
    // The fallback for a model with no diffuse map: a single white texel. Sampling
    // it returns white, so the lit shading shows the model uncoloured rather than
    // leaving the sampler binding empty (which is invalid). See Glossary: TEXTURE
    stbi_uc whitePixel[4] = {255, 255, 255, 255};
    stbi_uc* pixels = whitePixel;
    bool loaded = false;

    if (!path.empty()) {
        // Force four channels (RGBA) so the data always matches an 8-bit RGBA format,
        // whether or not the source image had an alpha channel. stb flips nothing;
        // the OBJ's UVs already match the image orientation. See Glossary: TEXEL
        pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (pixels == nullptr) {
            throw std::runtime_error("failed to load texture '" + path + "': " +
                                     stbi_failure_reason());
        }
        loaded = true;
        std::cout << "[texture] loaded " << path << ": " << width << "x" << height << "\n";
    }

    uploadPixels(pixels, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    if (loaded) stbi_image_free(pixels);

    createSampler();
}

Texture::~Texture() {
    const VkDevice device = m_context.device();
    if (m_sampler != VK_NULL_HANDLE) vkDestroySampler(device, m_sampler, nullptr);
    if (m_view != VK_NULL_HANDLE) vkDestroyImageView(device, m_view, nullptr);
    if (m_image != VK_NULL_HANDLE) vkDestroyImage(device, m_image, nullptr);
    if (m_memory != VK_NULL_HANDLE) vkFreeMemory(device, m_memory, nullptr);
}

void Texture::uploadPixels(const unsigned char* pixels, uint32_t width, uint32_t height) {
    const VkDeviceSize size = VkDeviceSize(width) * height * 4;  // 4 bytes per RGBA texel

    // 1. Stage the pixels in a host-visible buffer — same pattern as vertex upload.
    //    See Glossary: STAGING_BUFFER
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    m_context.createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           staging, stagingMemory);
    void* mapped = nullptr;
    vkMapMemory(m_context.device(), stagingMemory, 0, size, 0, &mapped);
    std::memcpy(mapped, pixels, static_cast<size_t>(size));
    vkUnmapMemory(m_context.device(), stagingMemory);

    // 2. Device-local image in an *sRGB* format: the texture stores colour authored
    //    in sRGB, and an sRGB format makes the GPU convert each sample to linear so
    //    the lighting maths is done in linear space (and the sRGB swapchain converts
    //    back on output). See Glossary: TEXTURE, SRGB_COLOUR_SPACE
    m_context.createImage(width, height, VK_FORMAT_R8G8B8A8_SRGB,
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                          m_image, m_memory);

    // 3. Layout dance: make it writable, copy the pixels in, then make it readable
    //    by the fragment shader. See Glossary: IMAGE_LAYOUT_TRANSITION
    m_context.transitionImageLayout(m_image, VK_IMAGE_LAYOUT_UNDEFINED,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    m_context.copyBufferToImage(staging, m_image, width, height);
    m_context.transitionImageLayout(m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(m_context.device(), staging, nullptr);
    vkFreeMemory(m_context.device(), stagingMemory, nullptr);

    // 4. An image view, the handle a descriptor/shader actually reads through.
    //    See Glossary: IMAGE_VIEW
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(m_context.device(), &viewInfo, nullptr, &m_view) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateImageView (texture) failed");
    }
}

void Texture::createSampler() {
    // The sampler is separate from the image: it describes *how* texels are read —
    // the filtering between samples and the wrapping outside [0,1]. See Glossary: SAMPLER
    VkSamplerCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    // Linear (bilinear) filtering blends the four nearest texels, smoothing the image
    // both when magnified and minified. See Glossary: TEXTURE_FILTERING, BILINEAR_FILTERING
    info.magFilter = VK_FILTER_LINEAR;
    info.minFilter = VK_FILTER_LINEAR;
    // Repeat (tile) coordinates outside [0,1]. Harmless here since the model's UVs
    // stay in range, but it is the sensible default. See Glossary: TEXTURE_WRAPPING
    info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    // Anisotropic filtering is a device feature we did not enable, so leave it off.
    info.anisotropyEnable = VK_FALSE;
    info.maxAnisotropy = 1.0f;
    info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    info.unnormalizedCoordinates = VK_FALSE;  // UVs are normalised 0..1
    // Single mip level — this project does not generate mipmaps, so the LOD range is
    // pinned to 0. See Glossary: MIPMAPS, TRILINEAR_FILTERING
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.minLod = 0.0f;
    info.maxLod = 0.0f;
    if (vkCreateSampler(m_context.device(), &info, nullptr, &m_sampler) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateSampler failed");
    }
}
