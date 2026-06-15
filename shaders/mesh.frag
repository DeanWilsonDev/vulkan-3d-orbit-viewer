#version 450
// Fragment shader — runs once per fragment (a candidate pixel produced by
// rasterising a triangle). Its job is to output that fragment's colour.
// See Glossary: FRAGMENT_SHADER, SHADER, FRAGMENT, RASTERISATION

// The single output, written to colour attachment 0 of the render pass.
layout(location = 0) out vec4 outColour;

void main() {
    // Chunk 5 outputs one flat colour for every fragment — a warm orange — so
    // the triangle reads clearly against the dark blue-grey clear colour. Real
    // per-fragment shading (lighting) arrives in Chunk 11.
    outColour = vec4(0.90, 0.55, 0.20, 1.0);
}
