#version 450
// Vertex shader — runs once per vertex on the GPU. Its job is to output the
// vertex's position in clip space via the built-in gl_Position.
// See Glossary: VERTEX_SHADER, SHADER, GLSL

// Chunk 5 draws a single hardcoded triangle with no vertex buffer: the three
// corners live here in the shader and are selected by gl_VertexIndex (0,1,2),
// the index of the vertex currently being processed. From Chunk 7 the positions
// come from a real vertex buffer instead. See Glossary: NDC_SPACE
vec2 positions[3] = vec2[](
    vec2( 0.0, -0.5),   // top    (Vulkan clip/NDC space has +Y pointing down)
    vec2( 0.5,  0.5),   // bottom-right
    vec2(-0.5,  0.5)    // bottom-left
);

void main() {
    // gl_Position is in clip space, a vec4 (x, y, z, w). With w = 1.0 these
    // coordinates are already normalised device coordinates. z = 0.0 sits in
    // front of the depth clear (1.0), so the triangle passes the depth test.
    // (The meaning of the w component is covered in Chunk 8.)
    // See Glossary: CLIP_SPACE
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
