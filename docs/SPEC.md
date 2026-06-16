# Vulkan Orbit Viewer — Example Project Spec

A self-contained C++ project that functions as a lightweight computer graphics course.
The completed project loads an OBJ mesh and renders it in a viewport you can orbit,
pan, and zoom around. Every system is documented to explain not just what is happening
but why, with a companion Glossary that covers computer graphics concepts broadly —
not just what appears directly in the code.

This is the foundation Umbra needs before Swoop Racer and before a 3D viewport can
be embedded in Dawn. You work through the finished project as a course, not in
parallel with its construction.

---

## Confirmed Decisions

| Decision | Choice | Reason |
|---|---|---|
| Language | C++ | Matches Umbra |
| Windowing + Input | SDL3 | Matches Umbra's existing dependency, handles X11/Wayland transparently |
| Graphics API | Vulkan | Long-term engine target, explicit by design |
| Mac support | MoltenVK | Translates Vulkan to Metal transparently |
| Math | GLM | Battle-tested, validates against later Umbra math types |
| Mesh format | OBJ | Plain text, no SDK dependency, human-readable |
| Structure | Layered files, `main.cpp` as narrative | Clean chapters, readable top-to-bottom |
| Comments | Concept name + immediate why | Depth deferred to Glossary |
| Glossary | Open and living — any graphics or Vulkan concept gets an entry | Lightweight computer graphics course companion |
| Build order | Earliest visible output first | Claude Code can verify correctness incrementally |

---

## Project Structure

```
vulkan-orbit-viewer/
├── CMakeLists.txt
├── Glossary.md
├── README.md
├── assets/
│   └── mesh.obj                  — sample mesh shipped with the project
├── shaders/
│   ├── mesh.vert                 — vertex shader (GLSL source)
│   ├── mesh.frag                 — fragment shader (GLSL source)
│   └── compiled/                 — SPIR-V output from glslc (git-ignored or pre-compiled)
└── src/
    ├── main.cpp                  — narrative entry point, reads like a story
    ├── sdl_context.h/.cpp        — SDL3 window creation, event loop, input
    ├── vulkan_context.h/.cpp     — all Vulkan infrastructure
    ├── swapchain.h/.cpp          — swapchain creation and management
    ├── render_pass.h/.cpp        — render pass and framebuffer setup
    ├── shader_pipeline.h/.cpp    — shader loading, SPIR-V, pipeline creation
    ├── mesh.h/.cpp               — OBJ loading, vertex buffers, GPU upload
    ├── camera.h/.cpp             — GLM transforms, orbit/pan/zoom logic
    └── uniform_buffer.h/.cpp     — per-frame uniform data (MVP matrices)
```

`main.cpp` is the table of contents. Each other file is a chapter. Reading `main.cpp`
gives you the shape of the whole system. Diving into any file gives you the internals
of that chapter without the noise of everything else.

---

## Build System

CMake with FetchContent for dependencies:

- **Vulkan SDK** — assumed installed on the host machine (standard for Vulkan dev)
- **SDL3** — fetched via FetchContent
- **GLM** — fetched via FetchContent
- **tinyobjloader** — fetched via FetchContent (single-header OBJ parser)
- **stb_image** — fetched via FetchContent (single-header image loader, added in Chunk 13)

CMake also invokes `glslc` (part of the Vulkan SDK) to compile GLSL shaders to
SPIR-V as part of the build. Shader compilation is not a manual step.

**Platform notes:**
- Linux: Vulkan runs natively. SDL3 handles X11/Wayland transparently.
- Mac: MoltenVK is bundled with the Vulkan SDK macOS installer. No extra steps
  beyond installing the SDK.
- Windows: Vulkan SDK installer handles driver-level setup.

---

## Comment Convention

Every non-trivial line or block follows this pattern:

```cpp
// Create the render pass — defines the structure of a rendering operation:
// what attachments (colour, depth) are used and what happens to them.
// See Glossary: RENDER_PASS
VkRenderPassCreateInfo renderPassInfo{};
```

The comment states:
1. What this thing is (plain English)
2. Why it exists / the problem it solves
3. A Glossary reference if it is a graphics, Vulkan, or GPU concept

Comments do not re-state what the code already makes obvious. They explain intent
and concepts, not mechanics.

---

## Glossary Convention

`Glossary.md` is a living document. It is not a closed list derived from what
appears in the code — it is a companion reference for the computer graphics domain.

**Any term that a graphics programmer would consider domain knowledge gets an entry.**
This includes:

- Vulkan-specific concepts (swapchain, render pass, descriptor set)
- General computer graphics concepts (rasterisation, NDC space, the rendering pipeline)
- GPU architecture concepts (memory types, parallelism, the graphics pipeline stages)
- Linear algebra as applied to rendering (coordinate spaces, matrix transforms, quaternions)
- Concepts that are required to understand another concept, even if not directly in the code

If explaining a concept requires understanding a related concept, add that related
concept too. The Glossary grows organically. When in doubt, add the entry.

Each entry follows this structure:

```markdown
## TERM

**What it is**
Plain English definition. No assumed knowledge.

**Why it matters**
The problem this concept solves, or why it exists in graphics programming.
For Vulkan-specific terms: why Vulkan needs this and why it couldn't be simpler.

**How it appears in this project**
Which file(s) and roughly where to find it in the code.
If the term is a general concept not tied to a specific file, say so.

**Further reading**
- [Link title](url)
- [Link title](url)
```

Glossary terms are written in SCREAMING_SNAKE_CASE so they are easy to search for
when following a `See Glossary: TERM` reference in the code.

The second field is labelled **Why it matters** rather than **Why Vulkan needs it**
because many entries will be general graphics concepts that predate Vulkan.

---

## Build Order for Claude Code

The chunks below are the build order for Claude Code, not a learning progression.
You work through the finished project as a course. The chunks exist so Claude Code
can verify correctness at each stage rather than building the entire project in one
shot and discovering a structural problem at the end.

Each chunk has a **verification checkpoint** — a plain statement of what Claude Code
should confirm before moving to the next chunk. These are internal quality gates,
not steps you need to follow.

---

### Chunk 1 — Project Scaffold and Window

**Files touched:** `CMakeLists.txt`, `main.cpp`, `sdl_context.h/.cpp`

**What gets built:**
- CMake project configured for Linux, Mac, and Windows
- FetchContent pulling SDL3 and GLM
- SDL3 window opens with a title
- Event loop runs: window closes cleanly on quit
- No Vulkan yet

**Concepts to document:**
- What SDL3 is and what it provides (window, input, Vulkan surface bridge)
- Why window management is separated from rendering
- What an event loop is and how it drives a real-time application
- Glossary entries: `SDL3`, `EVENT_LOOP`, `WINDOWING_SYSTEM`

**Verification checkpoint:**
A window opens. The title bar shows the project name. Closing the window exits
the program cleanly without a crash or leak reported by the OS.

---

### Chunk 2 — Vulkan Instance and Device

**Files touched:** `vulkan_context.h/.cpp`, `main.cpp`

**What gets built:**
- Vulkan instance created with validation layers enabled in debug builds
- Physical device selected (GPU enumerated and chosen)
- Logical device created with graphics and presentation queues
- Vulkan surface created via SDL3 (`SDL_Vulkan_CreateSurface`)
- Validation layer output visible in terminal on errors

**Concepts to document:**
- What a Vulkan instance is and why it exists
- Physical device vs logical device — what the distinction means
- What a queue family is and why graphics and presentation queues can be separate
- What validation layers are and why you always enable them in development
- What a surface is — the bridge between Vulkan and the OS window
- What extensions are in Vulkan and why they exist
- Glossary entries: `VULKAN_INSTANCE`, `PHYSICAL_DEVICE`, `LOGICAL_DEVICE`,
  `QUEUE_FAMILY`, `VALIDATION_LAYERS`, `SURFACE`, `VULKAN_EXTENSIONS`

**Verification checkpoint:**
The window opens. The terminal confirms Vulkan initialised successfully.
Intentionally triggering a validation error (e.g. a bad parameter) produces
a descriptive message. The program exits cleanly. Nothing is rendered yet.

---

### Chunk 3 — Swapchain

**Files touched:** `swapchain.h/.cpp`, `vulkan_context.h/.cpp`, `main.cpp`

**What gets built:**
- Swapchain created: the queue of images Vulkan renders into and presents to screen
- Swapchain images and image views created
- Swapchain recreation on window resize handled

**Concepts to document:**
- What a swapchain is and why it exists
- What tearing is and how the swapchain prevents it
- Double buffering vs triple buffering — the tradeoffs
- What an image view is — why you don't use VkImage directly
- What present modes are (FIFO, MAILBOX) and what each means for latency and throughput
- Why swapchain recreation on resize is required (not optional)
- What colour formats and colour spaces are (sRGB, UNORM etc.)
- Glossary entries: `SWAPCHAIN`, `IMAGE_VIEW`, `PRESENT_MODE`, `DOUBLE_BUFFERING`,
  `TRIPLE_BUFFERING`, `SCREEN_TEARING`, `COLOUR_FORMAT`, `COLOUR_SPACE`

**Verification checkpoint:**
Window opens, Vulkan initialises, swapchain created without validation errors.
Terminal confirms swapchain image count and chosen format. Resizing the window
recreates the swapchain without crashing. Nothing is rendered yet.

---

### Chunk 4 — Render Pass and Framebuffers

**Files touched:** `render_pass.h/.cpp`, `main.cpp`

**What gets built:**
- Render pass created: declares colour attachment and depth attachment
- Depth buffer image and image view created
- Framebuffers created: one per swapchain image
- Clear colour set to a visible non-black colour (dark grey or deep blue)

**Concepts to document:**
- What a render pass is — a description of rendering structure, not the act of rendering
- What an attachment is (colour, depth) and why they are declared up front
- What load and store operations are on attachments
- What a subpass is and why render passes can contain multiple of them
- What a framebuffer is — the concrete images a render pass operates on
- What a depth buffer is and the problem it solves (painter's algorithm, z-fighting)
- What depth testing is and how the GPU uses the depth buffer per fragment
- Glossary entries: `RENDER_PASS`, `ATTACHMENT`, `FRAMEBUFFER`, `DEPTH_BUFFER`,
  `DEPTH_TESTING`, `SUBPASS`, `Z_FIGHTING`, `PAINTERS_ALGORITHM`

**Verification checkpoint:**
The window opens and displays a solid clear colour — not black, not garbage data.
This is the first frame actually presented to screen. Validation layers report
no errors. Resizing still works.

---

### Chunk 5 — Shaders and Graphics Pipeline

**Files touched:** `shader_pipeline.h/.cpp`, `shaders/mesh.vert`,
`shaders/mesh.frag`, `CMakeLists.txt`, `main.cpp`

**What gets built:**
- GLSL vertex shader: accepts position, outputs `gl_Position`
- GLSL fragment shader: outputs a hardcoded solid colour
- CMake build step compiles both shaders to SPIR-V via `glslc`
- Shader modules loaded from SPIR-V at runtime
- Graphics pipeline created: vertex input, rasteriser, colour blending,
  viewport, scissor, depth test all configured
- Pipeline layout created (empty for now — no uniforms yet)
- A hardcoded triangle rendered using the pipeline

**Concepts to document:**
- What a shader is and where it runs (on the GPU, not the CPU)
- The programmable vs fixed-function stages of the graphics pipeline
- What the vertex shader does and when it runs
- What the fragment shader does and when it runs
- What rasterisation is — the process of converting triangles to fragments
- What GLSL is
- What SPIR-V is and why Vulkan uses bytecode rather than source
- What the graphics pipeline is in Vulkan and why it is described as a whole object
- Why the Vulkan pipeline is immutable once created (performance contract with the driver)
- What the pipeline layout is and why it exists separately from the pipeline
- What viewport and scissor are
- What colour blending is
- What NDC space is (normalised device coordinates) — where vertices live after
  the vertex shader
- What clip space is and how it relates to NDC
- Glossary entries: `SHADER`, `VERTEX_SHADER`, `FRAGMENT_SHADER`, `GLSL`, `SPIR_V`,
  `GRAPHICS_PIPELINE`, `PIPELINE_LAYOUT`, `RASTERISER`, `RASTERISATION`,
  `NDC_SPACE`, `CLIP_SPACE`, `VIEWPORT`, `SCISSOR`, `COLOUR_BLENDING`,
  `FIXED_FUNCTION_PIPELINE`, `PROGRAMMABLE_PIPELINE_STAGES`, `FRAGMENT`

**Verification checkpoint:**
A hardcoded triangle appears on screen in a solid colour against the clear colour
background. Resizing, minimising, and closing all work cleanly. Validation layers
report no errors.

---

### Chunk 6 — Command Buffers and Synchronisation

**Files touched:** `vulkan_context.h/.cpp`, `main.cpp`

**What gets built:**
- Command pool created
- Command buffers allocated: one per frame in flight
- Draw commands recorded into command buffers each frame
- Synchronisation primitives created: semaphores and fences
- Frame loop fully functional: acquire image, record commands, submit, present
- Frames in flight capped at 2

**Concepts to document:**
- What a command buffer is — why Vulkan requires pre-recorded work rather than
  immediate submission
- What a command pool is and why command buffers are allocated from one
- The acquire → record → submit → present cycle
- What a semaphore is and what it synchronises (GPU-to-GPU)
- What a fence is and what it synchronises (GPU-to-CPU)
- Why synchronisation is explicit in Vulkan where OpenGL hid it
- What frames in flight means — why you want more than one frame being processed
  simultaneously and what the tradeoff is
- What a pipeline barrier is and why GPU work ordering matters
- What image layout transitions are — why images must be in the right layout
  for each operation
- Glossary entries: `COMMAND_BUFFER`, `COMMAND_POOL`, `SEMAPHORE`, `FENCE`,
  `SYNCHRONISATION`, `FRAMES_IN_FLIGHT`, `PIPELINE_BARRIER`,
  `IMAGE_LAYOUT_TRANSITION`, `GPU_CPU_SYNC`, `SUBMISSION_QUEUE`

**Verification checkpoint:**
The triangle renders at full frame rate. Resizing recreates the swapchain without
crashing or producing validation errors. The terminal is clean. CPU and GPU are
not stalling each other unnecessarily.

---

### Chunk 7 — Vertex Buffers and Index Buffers

**Files touched:** `mesh.h/.cpp`, `vulkan_context.h/.cpp`, `main.cpp`

**What gets built:**
- Vertex struct defined: position (vec3), normal (vec3), UV (vec2)
- Hardcoded cube vertex and index data defined in code
- Staging buffer pattern implemented: CPU-side buffer copies to GPU-local buffer
- Vertex buffer bound in draw call
- Index buffer bound in draw call
- Cube renders instead of triangle

**Concepts to document:**
- What a vertex is and what attributes a vertex can carry
- What a vertex buffer is and how vertex data is described to the pipeline
  (vertex binding descriptions, attribute descriptions)
- What an index buffer is and why it exists (reusing vertices across triangles)
- What GPU memory is and why it is separate from CPU memory
- What memory types are in Vulkan (host-visible vs device-local)
- Why device-local memory is faster but not directly writable by the CPU
- What the staging buffer pattern is and why the double-copy is necessary
- What buffer usage flags are
- What a mesh is in the context of 3D graphics
- What a triangle winding order is and why it matters for back-face culling
- What back-face culling is and why GPUs do it
- Glossary entries: `VERTEX_BUFFER`, `INDEX_BUFFER`, `STAGING_BUFFER`,
  `GPU_MEMORY`, `MEMORY_TYPES`, `HOST_VISIBLE_MEMORY`, `DEVICE_LOCAL_MEMORY`,
  `MESH`, `WINDING_ORDER`, `BACK_FACE_CULLING`, `VERTEX_ATTRIBUTES`

**Verification checkpoint:**
A hardcoded cube is visible on screen as a flat-shaded solid. It may appear as
a hexagon at this stage because there are no transforms yet — the cube occupies
clip space directly. This is expected and correct.

---

### Chunk 8 — Uniform Buffers and MVP Transforms

**Files touched:** `uniform_buffer.h/.cpp`, `camera.h/.cpp`,
`shader_pipeline.h/.cpp`, `shaders/mesh.vert`, `main.cpp`

**What gets built:**
- Uniform buffer created per frame in flight
- UBO struct defined: model matrix, view matrix, projection matrix
- Descriptor set layout created
- Descriptor pool created
- Descriptor sets allocated and bound to uniform buffers
- Vertex shader updated to apply MVP transform
- Camera struct created with GLM: position, target, up vector
- Perspective projection matrix constructed with GLM
- Static model matrix applied (no rotation yet)
- Cube appears as a cube in perspective

**Concepts to document:**
- What a uniform buffer is — data uploaded once per draw that all vertices share
- What a descriptor is — how a shader declares that it needs external data
- What a descriptor set layout is — the blueprint declaring what a shader expects
- What a descriptor pool is — the allocator for descriptor sets
- What a descriptor set is — a bound collection of resources a shader can read
- What the MVP matrix chain is: Model (object in world space), View (world from
  the camera's perspective), Projection (3D world to 2D screen)
- What a model matrix is and what transforms it encodes (translation, rotation, scale)
- What a view matrix is and how it represents the camera
- What a projection matrix is
- What perspective projection is — why parallel lines converge and distant objects
  appear smaller
- What field of view is
- What orthographic projection is, as a contrast to perspective
- What homogeneous coordinates are — why positions become vec4 in shaders
- What the w component means in a homogeneous coordinate
- What coordinate spaces are and the full chain: object space → world space →
  view space → clip space → NDC space → screen space
- Glossary entries: `UNIFORM_BUFFER`, `DESCRIPTOR`, `DESCRIPTOR_SET`,
  `DESCRIPTOR_SET_LAYOUT`, `DESCRIPTOR_POOL`, `MVP_MATRIX`, `MODEL_MATRIX`,
  `VIEW_MATRIX`, `PROJECTION_MATRIX`, `PERSPECTIVE_PROJECTION`,
  `ORTHOGRAPHIC_PROJECTION`, `HOMOGENEOUS_COORDINATES`, `COORDINATE_SPACES`,
  `OBJECT_SPACE`, `WORLD_SPACE`, `VIEW_SPACE`, `SCREEN_SPACE`, `FIELD_OF_VIEW`

**Verification checkpoint:**
The cube is visible in perspective — it looks like a 3D cube, not a flat hexagon.
Perspective foreshortening is visible (near edges larger than far edges).
The shape is static. No interaction yet.

---

### Chunk 9 — OBJ Loading

**Files touched:** `mesh.h/.cpp`, `main.cpp`, `assets/mesh.obj`

**What gets built:**
- tinyobjloader integrated via FetchContent
- OBJ file parsed: positions, normals, UVs extracted
- Vertex deduplication performed (OBJ indexes positions/normals/UVs separately,
  GPU wants a single unified index)
- Vertex and index buffers populated from loaded mesh data
- Hardcoded cube replaced with loaded OBJ mesh
- A sample OBJ file (Stanford bunny or equivalent) shipped in `assets/`

**Concepts to document:**
- What the OBJ format is and how it stores mesh data
- What face data in OBJ means and why it differs from a GPU index buffer
- What vertex deduplication is and why it matters for GPU memory
- What UVs are and what UV mapping means (deferred — textures not in scope
  for this project, but the concept should be documented for context)
- What a normal is in OBJ and why smooth vs flat normals differ
- What an asset pipeline is in the context of a game engine (broad concept,
  this project is the seed of understanding it)
- Glossary entries: `OBJ_FORMAT`, `VERTEX_DEDUPLICATION`, `UV_COORDINATES`,
  `UV_MAPPING`, `SMOOTH_NORMALS`, `FLAT_NORMALS`, `ASSET_PIPELINE`

**Verification checkpoint:**
The loaded OBJ mesh renders in perspective in place of the cube. It is static
and flat-shaded. The shape is recognisable as the loaded asset.

---

### Chunk 10 — Orbit Camera and Input

**Files touched:** `camera.h/.cpp`, `sdl_context.h/.cpp`, `main.cpp`

**What gets built:**
- Orbit camera implemented with GLM
  - Left mouse drag: rotate around target (azimuth and elevation angles)
  - Right mouse drag: pan (translate target in camera-relative XY plane)
  - Scroll wheel: zoom (translate camera along view direction)
- SDL3 mouse events wired to camera
- Camera state updates each frame, view matrix rebuilt and uploaded to UBO
- Minimum zoom distance clamped (cannot clip into mesh)
- Elevation angle clamped (cannot flip over the poles)

**Concepts to document:**
- What an orbit camera is — spherical coordinates around a focal point
- What azimuth and elevation are in spherical coordinates
- What spherical coordinates are and how they relate to Cartesian coordinates
- How the view matrix is rebuilt from camera position and target each frame
- What gimbal lock is and why it is a problem for cameras (and why orbit cameras
  avoid it with clamping rather than quaternions at this stage)
- What delta time is and why camera movement should be frame-rate independent
- Glossary entries: `ORBIT_CAMERA`, `SPHERICAL_COORDINATES`, `AZIMUTH`,
  `ELEVATION`, `CAMERA_TARGET`, `GIMBAL_LOCK`, `DELTA_TIME`,
  `FRAME_RATE_INDEPENDENCE`

**Verification checkpoint:**
The mesh is fully interactive. Left drag rotates around it, right drag pans,
scroll zooms. The camera never clips through the mesh. Movement feels
continuous and clean with no jitter at any frame rate.

---

### Chunk 11 — Diffuse Lighting

**Files touched:** `shaders/mesh.vert`, `shaders/mesh.frag`,
`uniform_buffer.h/.cpp`, `main.cpp`

**What gets built:**
- Directional light direction and colour added to UBO
- Normal matrix computed and uploaded
- Vertex shader transforms normals to world space
- Fragment shader computes Lambert diffuse: `dot(normal, lightDir)`
- Ambient term added so shadow sides are not pure black
- Flat solid colour replaced with lit shading

**Concepts to document:**
- What a surface normal is and why it drives lighting calculations
- Why normals cannot use the same transform matrix as positions (and what the
  normal matrix is)
- What the lighting model is as a concept — a simplified approximation of physics
- What Lambert diffuse lighting is and the physics it approximates
- What the dot product means geometrically — why it measures alignment between
  two directions
- What ambient light is and what it approximates (global illumination, indirect light)
- What a directional light is vs a point light vs a spot light (document all three
  even though only directional is implemented)
- What the Phong lighting model is — ambient + diffuse + specular — even though
  specular is not implemented here, document it for completeness
- What world space lighting vs view space lighting means and why it matters where
  calculations happen
- What per-vertex lighting vs per-fragment lighting is and the visual difference
- Glossary entries: `SURFACE_NORMAL`, `NORMAL_MATRIX`, `LIGHTING_MODEL`,
  `LAMBERT_DIFFUSE`, `AMBIENT_LIGHT`, `DIRECTIONAL_LIGHT`, `POINT_LIGHT`,
  `SPOT_LIGHT`, `PHONG_LIGHTING`, `SPECULAR_LIGHT`, `DOT_PRODUCT_GEOMETRY`,
  `PER_VERTEX_LIGHTING`, `PER_FRAGMENT_LIGHTING`, `GLOBAL_ILLUMINATION`

**Verification checkpoint:**
The mesh is lit. Faces pointing toward the light are bright, faces pointing
away are dark but not black. Orbiting the mesh keeps the light fixed in world
space — it does not follow the camera.

---

### Chunk 12 — Polish and Completeness

**Files touched:** all

**What gets built:**
- Cleanup pass: all validation layer warnings resolved
- Proper Vulkan teardown order verified (destruction is reverse of creation)
- Window resize handled robustly across all three platforms
- README written: what the project is, how to build on each platform,
  prerequisites (Vulkan SDK), controls
- Glossary audit: every `See Glossary:` reference in the code resolves to a
  real entry. Any concept mentioned in comments that warrants an entry but
  doesn't have one yet gets added
- A second sample OBJ loadable via command-line argument

**Concepts to document:**
- Why Vulkan teardown order matters — parent objects must outlive their children
- What resource lifetime management means in graphics programming
- Any remaining concepts surfaced during the audit
- Glossary entries: `RESOURCE_LIFETIME`, `VULKAN_TEARDOWN_ORDER`,
  plus any gaps identified during audit

**Verification checkpoint:**
The project builds cleanly on all three platforms with no warnings and no
validation errors. The README is sufficient for someone starting from scratch.
Every `See Glossary:` reference resolves. Resizing the window at any point
during runtime does not crash or produce validation warnings.

### Chunk 13 — Diffuse Texture Support

**Files touched:** `mesh.h/.cpp`, `shader_pipeline.h/.cpp`,
`uniform_buffer.h/.cpp`, `shaders/mesh.frag`, `main.cpp`, `CMakeLists.txt`

**What gets built:**
- `stb_image` integrated via FetchContent (single-header image loader)
- Image loaded from disk into CPU memory via `stb_image`
- Vulkan image created and pixel data uploaded via staging buffer (same
  staging pattern as vertex buffers)
- Image view created for the texture
- Sampler created: controls how the GPU reads between texel samples
- Descriptor set layout updated to include a combined image sampler binding
- Descriptor sets updated to bind the texture
- Fragment shader updated to sample the diffuse texture and multiply by the
  Lambert diffuse light value
- MTL file parsing via tinyobjloader: diffuse texture path extracted and loaded
- Scope limited to diffuse/albedo texture only — normal maps, roughness,
  metallic and other PBR maps are explicitly out of scope for this chunk

**Concepts to document:**
- What a texture is in graphics programming
- What a texel is (a texture pixel) vs a screen pixel (fragment)
- What UV coordinates are in practice — how a 2D image maps onto 3D geometry
- What texture sampling is — how the GPU reads a colour value from a texture
  given a UV coordinate
- What a sampler is in Vulkan — the object that describes filtering and
  wrapping behaviour, separate from the image itself
- What texture filtering is: nearest neighbour vs bilinear vs trilinear —
  what each looks like and the performance tradeoff
- What mipmaps are, why they exist (aliasing at distance), and how the sampler
  uses them
- What texture wrapping modes are (repeat, clamp to edge, mirrored repeat)
  and when each is appropriate
- What a combined image sampler descriptor is in Vulkan and why image and
  sampler are often paired
- What the MTL file format is and how it relates to OBJ
- What a diffuse map is vs other texture map types (normal map, roughness,
  metallic, AO) — document all types briefly for context even though only
  diffuse is implemented
- What PBR (physically based rendering) is at a high level — document as a
  concept for orientation even though this project uses a simpler lighting model
- What sRGB colour space means for textures and why it matters for correct
  colour reproduction
- Glossary entries: `TEXTURE`, `TEXEL`, `TEXTURE_SAMPLING`, `SAMPLER`,
  `TEXTURE_FILTERING`, `NEAREST_NEIGHBOUR_FILTERING`, `BILINEAR_FILTERING`,
  `TRILINEAR_FILTERING`, `MIPMAPS`, `TEXTURE_WRAPPING`, `UV_COORDINATES`
  (extended from Chunk 9), `COMBINED_IMAGE_SAMPLER`, `MTL_FORMAT`,
  `DIFFUSE_MAP`, `NORMAL_MAP`, `ROUGHNESS_MAP`, `METALLIC_MAP`,
  `AMBIENT_OCCLUSION_MAP`, `PBR`, `SRGB_COLOUR_SPACE`

**Verification checkpoint:**
A textured OBJ asset from Sketchfab (exported as OBJ + MTL + diffuse texture)
loads and renders with its diffuse texture applied. The texture is lit by the
Lambert diffuse model — surfaces facing the light show the texture at full
brightness, surfaces facing away are darkened. UV seams are not visible at
normal viewing distances. Validation layers report no errors.

---

## Handing to Claude Code

This project is built in chunks for Claude Code's benefit — to allow incremental
verification. The finished project is what matters, not the chunking process.

When starting a chunk, provide this document and the following instruction:

> Build Chunk N of the Vulkan Orbit Viewer spec. The finished project is a
> lightweight computer graphics course — treat it that way. Every non-trivial
> line must have a comment explaining what it is and why it exists, with a
> `See Glossary: TERM` reference for any concept from the graphics, Vulkan,
> GPU, or linear algebra domain.
>
> The Glossary is a living document, not a closed list. Add an entry for every
> concept introduced in this chunk. If explaining a concept requires
> understanding a related concept, add that too. Each entry follows the
> structure: What it is / Why it matters / How it appears in this project /
> Further reading with links.
>
> Verify the checkpoint before considering this chunk done:
> [paste checkpoint text here]
>
> Do not begin Chunk N+1.
