// Shared by draw.vert and draw.frag. The structs here mirror code/draw.c, and
// must match it field for field. The engine's descriptor set and vector font
// data come from handrail's GLSL includes, in handrail/code/shaders.

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

#include "render.glsl"
#include "font_vector.glsl"

// Mirrors DrawInstanceKind in code/draw.c
#define DRAW_INSTANCE_KIND_QUAD  0
#define DRAW_INSTANCE_KIND_GLYPH 1

// Mirrors DrawInstance in code/draw.c
struct DrawInstance {
    vec2 position;
    vec2 size;
    vec4 color;
    uint kind;
    uint index;
};

// Mirrors DrawGlobals in code/draw.c
layout(buffer_reference, scalar) readonly buffer DrawGlobals {
    vec2 screen_size;
};

layout(buffer_reference, scalar) readonly buffer DrawInstances {
    DrawInstance instances[];
};

// Mirrors RenderPushConstants in handrail's render.h: the globals and
// instances, then the buffer regions in the order game_init gives them.
layout(push_constant, scalar) uniform PushConstants {
    DrawGlobals      globals;
    DrawInstances    instances;
    FontVectorGlyphs font_glyphs;
    FontVectorBands  font_bands;
    FontVectorCurves font_curves;
} push;
