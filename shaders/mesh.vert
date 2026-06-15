#version 450
// Vertex shader — runs once per vertex on the GPU. Its job is to output the
// vertex's position in clip space via the built-in gl_Position.
// See Glossary: VERTEX_SHADER, SHADER, GLSL

// Per-vertex inputs, matching the pipeline's vertex attribute descriptions
// (locations 0/1/2 = position/normal/uv). See Glossary: VERTEX_ATTRIBUTES
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;   // unused until lighting (Chunk 11)
layout(location = 2) in vec2 inUV;       // carried but not sampled in this project

void main() {
    // Chunk 7 has no transforms yet, so the vertex position is used directly as a
    // clip-space coordinate (w = 1, so clip space == NDC). The cube data is
    // already placed inside the clip volume. The model-view-projection transform
    // arrives in Chunk 8. See Glossary: CLIP_SPACE, NDC_SPACE
    gl_Position = vec4(inPosition, 1.0);
}
