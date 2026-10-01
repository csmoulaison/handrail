// Vector glyphs from handrail's media/font.h: FontVectorGlyph and FontCurve,
// the FONT_VECTOR_GLYPHS, FONT_VECTOR_BANDS, and FONT_VECTOR_CURVES regions,
// and the coverage of a pixel by a glyph's outline. The regions are passed in
// as buffer references, so the game's push constants can put them anywhere.
// Requires GL_EXT_buffer_reference and GL_EXT_scalar_block_layout.
// TODO: generate the GLSL mirrors of engine structs and limits from the C
// definitions in prebuild, so they can't drift

#ifndef handrail_font_vector_glsl_INCLUDED
#define handrail_font_vector_glsl_INCLUDED

struct FontVectorGlyph {
    vec4 bounds; // min x, min y, max x, max y, in ems
    uint curves_offset;
    uint bands_offset;
    uint h_bands_len;
    uint v_bands_len;
};

struct FontCurve {
    vec2 p0;
    vec2 p1;
    vec2 p2;
    vec2 padding;
};

layout(buffer_reference, scalar) readonly buffer FontVectorGlyphs {
    FontVectorGlyph glyphs[];
};

layout(buffer_reference, scalar) readonly buffer FontVectorBands {
    uint bands[];
};

layout(buffer_reference, scalar) readonly buffer FontVectorCurves {
    FontCurve curves[];
};

// The em-space position of a glyph quad's corner (0 or 1 on each axis), grown
// by a pixel on every side so pixels the outline only partly covers are still
// shaded. pixels_per_em is the quad's scale on each axis.
vec2 font_vector_quad_em(vec4 bounds, vec2 corner, vec2 pixels_per_em) {
    vec2 dilation = 1.0 / pixels_per_em;
    return mix(bounds.xy - dilation, bounds.zw + dilation, corner);
}

// Which roots of a curve's quadratic cross the ray, from the signs of its
// three coordinates across the ray: bit 0 for t1, where it crosses going
// down, and bit 1 for t2, where it crosses going up. Two bits per case,
// indexed by (y0 > 0) + 2 (y1 > 0) + 4 (y2 > 0).
uint font_vector_root_code(float y0, float y1, float y2) {
    uint shift = (y0 > 0.0 ? 2u : 0u) + (y1 > 0.0 ? 4u : 0u) + (y2 > 0.0 ? 8u : 0u);
    return (0x2E74u >> shift) & 3u;
}

// Roots t1 and t2 of y(t) = 0 for the curve y0, y1, y2. With no real roots
// they coincide, so their crossings cancel.
vec2 font_vector_roots(float y0, float y1, float y2) {
    float a = y0 - 2.0 * y1 + y2;
    float b = y0 - y1;
    float c = y0;
    if(abs(a) < 1.0 / 65536.0) {
        float t = c * 0.5 / b;
        return vec2(t, t);
    }
    float d = sqrt(max(b * b - a * c, 0.0));
    return vec2((b - d) / a, (b + d) / a);
}

float font_vector_curve_at(float x0, float x1, float x2, float t) {
    return (x0 - 2.0 * x1 + x2) * t * t + 2.0 * (x1 - x0) * t + x0;
}

// Coverage of the pixel at em-space position em by a vector glyph, after
// Lengyel's Slug algorithm (JCGT 2017). glyph indexes glyphs. pixels_per_em
// comes from derivatives, so take it before any branch.
//
// A ray from the pixel runs along +x through the curves of its horizontal
// band, and another along +y through its vertical band. Each crossing adds the
// fraction of the pixel on its far side, signed by its direction, which sums
// to a winding number filtered over the pixel. The two rays are blended by how
// near their crossings are.
float font_vector_coverage(FontVectorGlyphs glyphs, FontVectorBands bands, FontVectorCurves curves,
                           uint glyph_index, vec2 em, vec2 pixels_per_em) {
    FontVectorGlyph glyph = glyphs.glyphs[glyph_index];
    vec2 bands_len = vec2(glyph.v_bands_len, glyph.h_bands_len);
    vec2 band_scale = bands_len / max(glyph.bounds.zw - glyph.bounds.xy, vec2(1.0 / 65536.0));
    ivec2 band = clamp(ivec2(floor((em - glyph.bounds.xy) * band_scale)), ivec2(0), ivec2(bands_len) - 1);

    // Horizontal band, ray along +x
    float x_coverage = 0.0;
    float x_weight   = 0.0;
    uint header = glyph.bands_offset + uint(band.y) * 2u;
    uint curves_len = bands.bands[header];
    uint list = glyph.bands_offset + bands.bands[header + 1u];
    for(uint i = 0u; i < curves_len; i++) {
        FontCurve curve = curves.curves[glyph.curves_offset + bands.bands[list + i]];
        vec2 p0 = curve.p0 - em;
        vec2 p1 = curve.p1 - em;
        vec2 p2 = curve.p2 - em;
        // Curves are listed by descending max x, so the rest are all behind the pixel too
        if(max(max(p0.x, p1.x), p2.x) * pixels_per_em.x < -0.5) {
            break;
        }
        uint code = font_vector_root_code(p0.y, p1.y, p2.y);
        if(code != 0u) {
            vec2 t = font_vector_roots(p0.y, p1.y, p2.y);
            vec2 x = vec2(font_vector_curve_at(p0.x, p1.x, p2.x, t.x), font_vector_curve_at(p0.x, p1.x, p2.x, t.y)) * pixels_per_em.x;
            if((code & 1u) != 0u) {
                x_coverage += clamp(x.x + 0.5, 0.0, 1.0);
                x_weight = max(x_weight, clamp(1.0 - abs(x.x) * 2.0, 0.0, 1.0));
            }
            if((code & 2u) != 0u) {
                x_coverage -= clamp(x.y + 0.5, 0.0, 1.0);
                x_weight = max(x_weight, clamp(1.0 - abs(x.y) * 2.0, 0.0, 1.0));
            }
        }
    }

    // Vertical band, ray along +y. Swapping the axes mirrors the outline, so
    // crossings count with the opposite sign.
    float y_coverage = 0.0;
    float y_weight   = 0.0;
    header = glyph.bands_offset + (glyph.h_bands_len + uint(band.x)) * 2u;
    curves_len = bands.bands[header];
    list = glyph.bands_offset + bands.bands[header + 1u];
    for(uint i = 0u; i < curves_len; i++) {
        FontCurve curve = curves.curves[glyph.curves_offset + bands.bands[list + i]];
        vec2 p0 = curve.p0 - em;
        vec2 p1 = curve.p1 - em;
        vec2 p2 = curve.p2 - em;
        if(max(max(p0.y, p1.y), p2.y) * pixels_per_em.y < -0.5) {
            break;
        }
        uint code = font_vector_root_code(p0.x, p1.x, p2.x);
        if(code != 0u) {
            vec2 t = font_vector_roots(p0.x, p1.x, p2.x);
            vec2 y = vec2(font_vector_curve_at(p0.y, p1.y, p2.y, t.x), font_vector_curve_at(p0.y, p1.y, p2.y, t.y)) * pixels_per_em.y;
            if((code & 1u) != 0u) {
                y_coverage -= clamp(y.x + 0.5, 0.0, 1.0);
                y_weight = max(y_weight, clamp(1.0 - abs(y.x) * 2.0, 0.0, 1.0));
            }
            if((code & 2u) != 0u) {
                y_coverage += clamp(y.y + 0.5, 0.0, 1.0);
                y_weight = max(y_weight, clamp(1.0 - abs(y.y) * 2.0, 0.0, 1.0));
            }
        }
    }

    // Trust whichever ray had a crossing near the pixel; with neither, take
    // the smaller estimate, since a ray grazing a curve tends to overcount
    float blended = abs(x_coverage * x_weight + y_coverage * y_weight) / max(x_weight + y_weight, 1.0 / 65536.0);
    return clamp(max(blended, min(abs(x_coverage), abs(y_coverage))), 0.0, 1.0);
}

#endif
