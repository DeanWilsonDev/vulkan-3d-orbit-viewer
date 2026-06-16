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

// The diffuse texture as a combined image sampler at binding 1 — the image and the
// sampler (filtering/wrapping) packaged in one. `texture()` reads a colour from it at
// a UV coordinate. See Glossary: COMBINED_IMAGE_SAMPLER, SAMPLER, TEXTURE_SAMPLING
layout(set = 0, binding = 1) uniform sampler2D diffuseTex;

// Interpolated from the vertex shader: the world-space normal (renormalised below,
// since interpolation does not preserve unit length) and the UV.
// See Glossary: PER_FRAGMENT_LIGHTING, UV_COORDINATES
layout(location = 0) in vec3 inWorldNormal;
layout(location = 1) in vec2 inUV;

// The single output, written to colour attachment 0 of the render pass.
layout(location = 0) out vec4 outColour;

void main() {
    // The surface's base colour now comes from the diffuse texture (its albedo),
    // sampled at this fragment's UV, replacing the flat orange of earlier chunks.
    // The sRGB image format means the sample is already converted to linear here.
    // See Glossary: DIFFUSE_MAP, TEXTURE_SAMPLING, SRGB_COLOUR_SPACE
    vec3 albedo = texture(diffuseTex, inUV).rgb;

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
