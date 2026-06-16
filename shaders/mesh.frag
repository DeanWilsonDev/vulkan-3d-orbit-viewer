#version 450
// Fragment shader — runs once per fragment (a candidate pixel from rasterising a
// triangle). As of Chunk 11 it shades that fragment with a single directional light
// instead of a flat colour. See Glossary: FRAGMENT_SHADER, FRAGMENT, RASTERISATION

// Same uniform block as the vertex shader (the descriptor is visible to both stages).
// The fragment shader only reads the lighting fields. See Glossary: UNIFORM_BUFFER
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 normalMatrix;
    vec4 lightDirection; // xyz = direction toward the light (normalised)
    vec4 lightColour;    // rgb = light colour / intensity
    vec4 ambientColour;  // rgb = ambient fill
} ubo;

// The interpolated world-space normal from the vertex shader. Interpolation does not
// preserve unit length, so it is renormalised below. See Glossary: PER_FRAGMENT_LIGHTING
layout(location = 0) in vec3 inWorldNormal;

// The single output, written to colour attachment 0 of the render pass.
layout(location = 0) out vec4 outColour;

void main() {
    // The surface's base (unlit) colour — the warm orange used since Chunk 5, now
    // acting as the material albedo that the light modulates.
    const vec3 albedo = vec3(0.90, 0.55, 0.20);

    vec3 N = normalize(inWorldNormal);
    vec3 L = normalize(ubo.lightDirection.xyz);

    // Lambert diffuse: brightness is the cosine of the angle between the surface
    // normal and the light direction, which the dot product gives directly for unit
    // vectors. max(..., 0) discards light arriving from behind the surface.
    // See Glossary: LAMBERT_DIFFUSE, DOT_PRODUCT_GEOMETRY
    float diffuse = max(dot(N, L), 0.0);

    // Final shade = ambient fill + diffuse contribution, modulated by the albedo.
    // The ambient term keeps faces turned away from the light dark but not pure
    // black. See Glossary: AMBIENT_LIGHT, LIGHTING_MODEL
    vec3 colour = albedo * (ubo.ambientColour.rgb + diffuse * ubo.lightColour.rgb);
    outColour = vec4(colour, 1.0);
}
