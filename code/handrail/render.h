#ifndef handrail_render_h_INCLUDED
#define handrail_render_h_INCLUDED

// The interface between the game and the GPU backend. It is API agnostic, so
// the game library never sees Vulkan types.
//
// The backend knows nothing about what is drawn. At init the game describes
// which asset regions to upload, its textures, samplers, and passes. Each
// frame the game writes, straight into GPU-visible memory:
// - globals:   one block of whatever the game's shaders want (camera, time).
// - instances: a flat array of the game's per-instance records.
// - commands:  one RenderDrawCommand per instance, per pass.
// Each pass is then drawn with a single indirect draw call.
//
// Shaders reach everything through RenderPushConstants (buffer device
// addresses, mirrored in GLSL with buffer_reference and scalar layout) and one
// descriptor set:
//   layout(set = 0, binding = 0) uniform texture2D textures[RENDER_MAX_TEXTURES];
//   layout(set = 0, binding = 1) uniform sampler   samplers[RENDER_MAX_SAMPLERS];

#ifndef RENDER_MAX_BUFFER_REGIONS
#define RENDER_MAX_BUFFER_REGIONS 8
#endif

#ifndef RENDER_MAX_TEXTURES
#define RENDER_MAX_TEXTURES 1024
#endif

#ifndef RENDER_MAX_SAMPLERS
#define RENDER_MAX_SAMPLERS 16
#endif

#ifndef RENDER_MAX_PASSES
#define RENDER_MAX_PASSES 4
#endif

#ifndef RENDER_FRAME_GLOBALS_SIZE
#define RENDER_FRAME_GLOBALS_SIZE (KILOBYTE * 64)
#endif

#ifndef RENDER_FRAME_INSTANCES_SIZE
#define RENDER_FRAME_INSTANCES_SIZE (MEGABYTE * 8)
#endif

#ifndef RENDER_FRAME_MAX_COMMANDS
#define RENDER_FRAME_MAX_COMMANDS 65536
#endif

// Same layout as VkDrawIndirectCommand. first_instance is the index of the
// instance record, which shaders read as gl_InstanceIndex.
typedef struct {
    u32 vertex_count;
    u32 instance_count;
    u32 first_vertex;
    u32 first_instance;
} RenderDrawCommand;

typedef enum {
    RENDER_BLEND_NONE,
    RENDER_BLEND_ALPHA
} RenderBlend;

typedef enum {
    RENDER_CULL_NONE,
    RENDER_CULL_BACK
} RenderCull;

typedef enum {
    RENDER_FILTER_NEAREST,
    RENDER_FILTER_LINEAR
} RenderFilter;

typedef enum {
    RENDER_ADDRESS_CLAMP,
    RENDER_ADDRESS_REPEAT
} RenderAddress;

typedef struct {
    ShaderData* vertex_shader;
    ShaderData* fragment_shader;
    RenderBlend blend;
    RenderCull  cull;
    bool        depth_test;
    bool        depth_write;
} RenderPassDesc;

typedef struct {
    RenderFilter  filter;
    RenderAddress address;
} RenderSamplerDesc;

// A block of the asset pack to upload as one GPU buffer, typically a whole
// asset type region such as MESH_VERTICES.
typedef struct {
    void* data;
    u64   size;
} RenderBufferRegion;

// Filled in by the game at init.
typedef struct {
    RenderBufferRegion buffer_regions[RENDER_MAX_BUFFER_REGIONS];
    u32                buffer_regions_len;
    // Index in this array is the index into the shaders' textures array.
    TextureAsset*      textures[RENDER_MAX_TEXTURES];
    u32                textures_len;
    // Start of the TEXTURE_PIXELS region, which TextureAsset.pixels_offset is relative to.
    u8*                texture_pixels;
    RenderSamplerDesc  samplers[RENDER_MAX_SAMPLERS];
    u32                samplers_len;
    RenderPassDesc     passes[RENDER_MAX_PASSES];
    u32                passes_len;
} RenderSetup;

// Handed to the game each frame. Every pointer is into GPU-visible memory.
typedef struct {
    v4                 clear_color;
    void*              globals;          // RENDER_FRAME_GLOBALS_SIZE bytes
    void*              instances;        // RENDER_FRAME_INSTANCES_SIZE bytes
    RenderDrawCommand* commands[RENDER_MAX_PASSES];
    u32                commands_len[RENDER_MAX_PASSES];
} RenderFrame;

// Pushed to every pass. buffer_regions matches RenderSetup.buffer_regions in order.
typedef struct {
    u64 globals;
    u64 instances;
    u64 buffer_regions[RENDER_MAX_BUFFER_REGIONS];
} RenderPushConstants;

#endif
