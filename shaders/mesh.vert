#version 450
// Vertex shader — runs once per vertex on the GPU. Its job is to output the
// vertex's position in clip space via the built-in gl_Position.
// See Glossary: VERTEX_SHADER, SHADER, GLSL

// The uniform buffer: data uploaded once per draw that every vertex shares. The
// block layout (three mat4s, in this order) must match the C++ UniformBufferObject
// struct exactly. "set = 0, binding = 0" matches the descriptor set layout the
// pipeline was built with. See Glossary: UNIFORM_BUFFER, DESCRIPTOR, MVP_MATRIX
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;   // object space → world space
    mat4 view;    // world space → view (camera) space
    mat4 proj;    // view space → clip space
} ubo;

// Per-vertex inputs, matching the pipeline's vertex attribute descriptions
// (locations 0/1/2 = position/normal/uv). See Glossary: VERTEX_ATTRIBUTES
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;   // unused until lighting (Chunk 11)
layout(location = 2) in vec2 inUV;       // carried but not sampled in this project

void main() {
    // The MVP chain, read right to left: place the object in the world (model),
    // view it from the camera (view), then project that 3D view onto clip space
    // (proj). The position is a vec4 with w = 1 — a homogeneous coordinate — so the
    // matrices can encode translation and the perspective divide can happen.
    // See Glossary: MVP_MATRIX, HOMOGENEOUS_COORDINATES, COORDINATE_SPACES, CLIP_SPACE
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
}
