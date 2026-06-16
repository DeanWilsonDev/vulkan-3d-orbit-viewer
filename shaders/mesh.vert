#version 450
// Vertex shader — runs once per vertex on the GPU. It outputs the vertex's clip-space
// position (gl_Position) and, as of Chunk 11, its world-space normal for the fragment
// shader to light. See Glossary: VERTEX_SHADER, SHADER, GLSL

// The uniform buffer: data uploaded once per draw that every vertex shares. The
// block layout must match the C++ UniformBufferObject struct exactly (std140).
// "set = 0, binding = 0" matches the descriptor set layout the pipeline was built
// with. See Glossary: UNIFORM_BUFFER, DESCRIPTOR, MVP_MATRIX
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;          // object space → world space
    mat4 view;           // world space → view (camera) space
    mat4 proj;           // view space → clip space
    mat4 normalMatrix;   // object-space normals → world space (mat3 stored as mat4)
    vec4 lightDirection; // xyz = direction toward the light (unused here)
    vec4 lightColour;    // unused here — read by the fragment shader
    vec4 ambientColour;  // unused here — read by the fragment shader
} ubo;

// Per-vertex inputs, matching the pipeline's vertex attribute descriptions
// (locations 0/1/2 = position/normal/uv). See Glossary: VERTEX_ATTRIBUTES
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;       // carried but not sampled in this project

// Passed to the fragment shader and interpolated across the triangle, so lighting is
// evaluated per fragment rather than per vertex. See Glossary: PER_FRAGMENT_LIGHTING
layout(location = 0) out vec3 outWorldNormal;

void main() {
    // The MVP chain, read right to left: place the object in the world (model),
    // view it from the camera (view), then project that 3D view onto clip space
    // (proj). The position is a vec4 with w = 1 — a homogeneous coordinate.
    // See Glossary: MVP_MATRIX, HOMOGENEOUS_COORDINATES, COORDINATE_SPACES, CLIP_SPACE
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);

    // Transform the normal into world space with the dedicated normal matrix — not
    // the model matrix, which would skew normals under non-uniform scale. It is
    // normalised again in the fragment shader after interpolation.
    // See Glossary: SURFACE_NORMAL, NORMAL_MATRIX, WORLD_SPACE
    outWorldNormal = mat3(ubo.normalMatrix) * inNormal;
}
