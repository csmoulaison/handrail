#version 460
#extension GL_GOOGLE_include_directive : require
#include "draw.glsl"

layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec4 out_color;
layout(location = 2) flat out uint out_kind;
layout(location = 3) flat out uint out_index;

// Two counter-clockwise triangles over the unit square, one corner per vertex
const vec2 quad_corners[6] = vec2[](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0),
    vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0)
);

void main() {
    // gl_InstanceIndex is the draw command's first_instance
    DrawInstance instance = push.instances.instances[gl_InstanceIndex];
    vec2 corner = quad_corners[gl_VertexIndex];
    out_color = instance.color;
    out_kind  = instance.kind;
    out_index = instance.index;

    vec2 pixel;
    switch(instance.kind) {
        case DRAW_INSTANCE_KIND_QUAD: {
            pixel  = instance.position + corner * instance.size;
            // Texture rows run top to bottom, screen y runs bottom to top
            out_uv = vec2(corner.x, 1.0 - corner.y);
        } break;
        case DRAW_INSTANCE_KIND_GLYPH: {
            // Cover the glyph's bounds, in ems, then scale ems to pixels. The
            // fragment shader gets the position in ems to test against the outline.
            vec4 bounds = push.font_glyphs.glyphs[instance.index].bounds;
            vec2 em     = font_vector_quad_em(bounds, corner, instance.size);
            pixel  = instance.position + em * instance.size;
            out_uv = em;
        } break;
    }

    // Pixels to clip space. The backend flips the viewport, so +y is up.
    gl_Position = vec4(pixel / push.globals.screen_size * 2.0 - 1.0, 0.0, 1.0);
}
