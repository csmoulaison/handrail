#version 460
#extension GL_GOOGLE_include_directive : require
#include "draw.glsl"

layout(location = 0) in vec2 in_uv;
layout(location = 1) in vec4 in_color;
layout(location = 2) flat in uint in_kind;
layout(location = 3) flat in uint in_index;

layout(location = 0) out vec4 out_color;

void main() {
    // Derivatives before branching, so every pixel of a quad computes them
    vec2 pixels_per_uv = 1.0 / fwidth(in_uv);
    vec4 color = in_color;

    switch(in_kind) {
        case DRAW_INSTANCE_KIND_QUAD: {
            // textures and samplers come from render.glsl. in_index is a
            // TEXTURE handle, and differs per instance, hence nonuniformEXT.
            color *= texture(sampler2D(textures[nonuniformEXT(in_index)], samplers[0]), in_uv);
        } break;
        case DRAW_INSTANCE_KIND_GLYPH: {
            // How much of this pixel the glyph's outline covers. in_uv is in ems.
            color.a *= font_vector_coverage(push.font_glyphs, push.font_bands, push.font_curves, in_index, in_uv, pixels_per_uv);
        } break;
    }
    out_color = color;
}
