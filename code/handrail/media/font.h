#ifndef handrail_font_h_INCLUDED
#define handrail_font_h_INCLUDED

#define FONT_GLYPH_COUNT 256

// Empty pixels left between glyphs in the atlas, so linear sampling doesn't
// bleed into neighbours.
#define FONT_GLYPH_PADDING 1

// Most horizontal (and vertical) bands a vector glyph is split into.
#ifndef FONT_VECTOR_MAX_BANDS
#define FONT_VECTOR_MAX_BANDS 8
#endif

// Most curves in one vector font, all glyphs together. Only limits construction.
#ifndef FONT_VECTOR_MAX_CURVES
#define FONT_VECTOR_MAX_CURVES 65536
#endif

// All metrics are in pixels. position is the glyph's top left in the atlas,
// bearing[1] is the distance from the baseline up to the glyph's top.
typedef struct {
    u32 position[2];
    u32 size[2];
    i32 bearing[2];
    u32 advance;
} FontGlyph;

// Pushed FONT asset. The atlas is a square TEXTURE_FORMAT_R TEXTURE asset.
typedef struct {
    u32       texture_width;
    u32       texture_handle;
    u32       pixel_size;
    i32       ascender;
    i32       descender;
    i32       line_height;
    FontGlyph glyphs[FONT_GLYPH_COUNT];
} FontData;

// Vector fonts are drawn straight from their outlines, with no atlas. Each
// glyph is a list of quadratic Bezier curves in em space. Its bounds are split
// into horizontal and vertical bands, and each band lists the curves that
// overlap it, so a pixel only tests the curves near it (Lengyel's Slug
// algorithm). The curves, bands, and glyph headers live in the GPU regions
// FONT_VECTOR_CURVES, FONT_VECTOR_BANDS, and FONT_VECTOR_GLYPHS, which the
// renderer uploads whole.

// One curve in em space. Lines have their control point at their midpoint.
// Padded to 32 bytes, so every font's curves start on a whole curve.
typedef struct {
    v2 p0;
    v2 p1;
    v2 p2;
    v2 padding;
} FontCurve;

// Glyph header in FONT_VECTOR_GLYPHS. From bands_offset, FONT_VECTOR_BANDS
// holds h_bands_len then v_bands_len band headers of two u32s (curve count,
// offset of the band's curve list from bands_offset), then the curve lists,
// whose entries index curves from curves_offset. Horizontal bands split the
// bounds bottom to top and list curves by descending max x. Vertical bands
// split them left to right and list curves by descending max y.
typedef struct {
    v4  bounds;        // min x, min y, max x, max y
    u32 curves_offset; // In curves from the start of FONT_VECTOR_CURVES
    u32 bands_offset;  // In u32s from the start of FONT_VECTOR_BANDS
    u32 h_bands_len;
    u32 v_bands_len;
} FontVectorGlyph;

// All in ems; multiply by the pixel size.
typedef struct {
    v4  bounds; // Zero for glyphs without an outline, such as space
    f32 advance;
} FontVectorMetrics;

// Pushed FONT_VECTOR asset. The CPU side of a vector font, for layout.
typedef struct {
    f32               ascender;
    f32               descender;
    f32               line_height;
    u32               first_glyph; // Index of glyph 0 in FONT_VECTOR_GLYPHS
    FontVectorMetrics glyphs[FONT_GLYPH_COUNT];
} FontVectorData;

// Normalized x, y, w, h of a glyph within the font's atlas
v4  font_glyph_src(FontData* font, FontGlyph* glyph);
u64 font_size();

#ifdef HANDRAIL_FONT_PROCESSING
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
typedef struct {
    FontData*    font;
    TextureData* texture;
} FontAndTextureData;

// Used during construction
typedef struct {
    FontGlyph glyph;
    u8*       pixels;
} FontGlyphInfo;

FontAndTextureData font_and_texture_from_ttf(char* path, u32 pixel_size, u32 texture_handle, Stack* stack);
// Rasterize a TTF/OTF at pixel_size, push its atlas as a TEXTURE and a FONT header. Returns the FONT handle.
u64                font_push_asset(AssetBuilder* builder, String tag, String path, u32 pixel_size, Stack* stack);

// Used during vector construction, by FT_Outline_Decompose's callbacks
typedef struct {
    FontCurve* curves;
    u32        curves_len;
    v2         pen;
    f32        scale; // Font units to ems
} FontOutline;

// Extract a TTF/OTF's outlines, band them, and push FONT_VECTOR_CURVES,
// FONT_VECTOR_BANDS, FONT_VECTOR_GLYPHS, and a FONT_VECTOR header. Returns the FONT_VECTOR handle.
u64                font_vector_push_asset(AssetBuilder* builder, String tag, String path, Stack* stack);
#endif

#endif

#if defined(HANDRAIL_IMPLEMENTATION_PASS) && !defined(handrail_font_h_IMPLEMENTED)
#define handrail_font_h_IMPLEMENTED

v4 font_glyph_src(FontData* font, FontGlyph* glyph) {
    return v4_new(
        (f32)glyph->position[0] / font->texture_width,
        (f32)glyph->position[1] / font->texture_width,
        (f32)glyph->size[0]     / font->texture_width,
        (f32)glyph->size[1]     / font->texture_width);
}

u64 font_size() {
    return sizeof(FontData);
}

#ifdef HANDRAIL_FONT_PROCESSING
FontAndTextureData font_and_texture_from_ttf(char* path, u32 pixel_size, u32 texture_handle, Stack* stack) {
    FT_Library ft;
    if(FT_Init_FreeType(&ft)) {
        log_exit("Font: failed to initialize FreeType");
    }
    FT_Face ft_face;
    if(FT_New_Face(ft, path, 0, &ft_face)) {
        log_exit("Font: failed to open %s", path);
    }
    FT_Set_Pixel_Sizes(ft_face, 0, pixel_size);

    FontGlyphInfo glyphs[FONT_GLYPH_COUNT];
    u32 pack_order[FONT_GLYPH_COUNT];
    for(i32 i = 0; i < FONT_GLYPH_COUNT; i++) {
        if(FT_Load_Char(ft_face, (unsigned char)i, FT_LOAD_RENDER)) {
            log_exit("Font: failed to render character %i of %s", i, path);
        }
        pack_order[i] = i;

        FontGlyphInfo* glyph = &glyphs[i];
        glyph->glyph.size[0]    = ft_face->glyph->bitmap.width;
        glyph->glyph.size[1]    = ft_face->glyph->bitmap.rows;
        glyph->glyph.bearing[0] = ft_face->glyph->bitmap_left;
        glyph->glyph.bearing[1] = ft_face->glyph->bitmap_top;
        // FreeType advances are in 26.6 fixed point
        glyph->glyph.advance    = (u32)(ft_face->glyph->advance.x >> 6);

        // Rows can be padded, so copy them one at a time
        u32 bm_size = sizeof(u8) * glyph->glyph.size[0] * glyph->glyph.size[1];
        glyph->pixels = (u8*)stack_alloc(stack, bm_size);
        for(i32 y = 0; y < glyph->glyph.size[1]; y++) {
            memcpy(&glyph->pixels[y * glyph->glyph.size[0]],
                   &ft_face->glyph->bitmap.buffer[y * ft_face->glyph->bitmap.pitch],
                   glyph->glyph.size[0]);
        }
    }
    i32 ascender    = (i32)(ft_face->size->metrics.ascender >> 6);
    i32 descender   = (i32)(ft_face->size->metrics.descender >> 6);
    i32 line_height = (i32)(ft_face->size->metrics.height >> 6);
    FT_Done_Face(ft_face);
    FT_Done_FreeType(ft);

    // Sort the packing order by height, tallest glyphs first
    for(u32 i = 0; i < FONT_GLYPH_COUNT; i++) {
        for(i32 j = 0; j < FONT_GLYPH_COUNT - 1; j++) {
            if(glyphs[pack_order[j]].glyph.size[1] < glyphs[pack_order[j + 1]].glyph.size[1]) {
                u32 tmp = pack_order[j];
                pack_order[j] = pack_order[j + 1];
                pack_order[j + 1] = tmp;
            }
        }
    }

    // Pack rects using shelf algorithm. Each glyph takes its size plus
    // FONT_GLYPH_PADDING on the right and bottom.
    u32 atlas_width = 16;

// This is essentially a while loop that goes until a large enough atlas size
// is found. It seems nicer this way to me.
try_pack_again:

    atlas_width *= 2;

    i32 curx = 0;
    i32 cury = 0;
    i32 cur_shelf_size = 0;
    for(i32 i = 0; i < FONT_GLYPH_COUNT; i++) {
        FontGlyphInfo* glyph = &glyphs[pack_order[i]];
        i32 padded_w = glyph->glyph.size[0] + FONT_GLYPH_PADDING;
        i32 padded_h = glyph->glyph.size[1] + FONT_GLYPH_PADDING;
        if(cur_shelf_size == 0) {
            cur_shelf_size = padded_h;
        }

        if(curx + padded_w > atlas_width) {
            if(curx == 0) {
                goto try_pack_again;
            }

            cury += cur_shelf_size;
            cur_shelf_size = padded_h;
            curx = 0;
        }

        glyph->glyph.position[0] = curx;
        glyph->glyph.position[1] = cury;
        curx += padded_w;

        if(cury + cur_shelf_size > atlas_width) {
            goto try_pack_again;
        }
    }
    log_print(LOG_ASSET, "Font %s: %u px, %ux%u atlas", path, pixel_size, atlas_width, atlas_width);

    // If we made it here, we found a good atlas size, so it's time to allocate
    // space and render glyphs to the buffer
    TextureData* tex_data = (TextureData*)stack_alloc(
        stack, texture_size_from_dimensions(atlas_width, atlas_width, TEXTURE_FORMAT_R));
    tex_data->width  = atlas_width;
    tex_data->height = atlas_width;
    tex_data->format = TEXTURE_FORMAT_R;
    memset(tex_data->pixel_buffer, 0, atlas_width * atlas_width);
    for(i32 i = 0; i < FONT_GLYPH_COUNT; i++) {
        FontGlyphInfo* glyph = &glyphs[pack_order[i]];
        for(i32 y = 0; y < glyph->glyph.size[1]; y++) {
            for(i32 x = 0; x < glyph->glyph.size[0]; x++) {
                i32 dst_x = glyph->glyph.position[0] + x;
                i32 dst_y = glyph->glyph.position[1] + y;
                u8 pixel  = glyph->pixels[y * glyph->glyph.size[0] + x];
                tex_data->pixel_buffer[dst_y * atlas_width + dst_x] = pixel;

                /* This, and the comment below, print out each character in the terminal.
                if(glyph->pixels[y * glyph->glyph.size[0] + x] > 128) {
                    printf("#");
                } else {
                    printf(" ");
                }
                */
            }
            //printf("\n");
        }
    }

    FontData* font_data = (FontData*)stack_alloc(
        stack, font_size());
    font_data->texture_width  = atlas_width;
    font_data->texture_handle = texture_handle;
    font_data->pixel_size     = pixel_size;
    font_data->ascender       = ascender;
    font_data->descender      = descender;
    font_data->line_height    = line_height;
    for(i32 i = 0; i < FONT_GLYPH_COUNT; i++) {
        font_data->glyphs[i] = glyphs[i].glyph;
    }

    FontAndTextureData data = {};
    data.texture = tex_data;
    data.font = font_data;
    return data;
}

u64 font_push_asset(AssetBuilder* builder, String tag, String path, u32 pixel_size, Stack* stack) {
    // FreeType wants a null-terminated path
    char* path_buf = (char*)alloca(path.len + 1);
    String path_c = string_init(path_buf, path.len + 1);
    string_cat(&path_c, path);
    string_write_null_terminator(&path_c);

    u64 texture_handle = asset_builder_next_handle_of_type(builder, string_const("TEXTURE"));
    FontAndTextureData data = font_and_texture_from_ttf(path_buf, pixel_size, (u32)texture_handle, stack);
    texture_push_asset(builder, tag, data.texture, stack);

    u64 handle = asset_builder_next_handle_of_type(builder, string_const("FONT"));
    asset_builder_push_asset(builder, tag, string_const("FONT"), string_const("FontData"), data.font, sizeof(FontData), NULL);
    return handle;
}

void font_outline_push_curve(FontOutline* outline, v2 p0, v2 p1, v2 p2) {
    if(outline->curves_len >= FONT_VECTOR_MAX_CURVES) {
        log_exit("Font: more than FONT_VECTOR_MAX_CURVES (%i) curves", FONT_VECTOR_MAX_CURVES);
    }
    outline->curves[outline->curves_len++] = (FontCurve){ .p0 = p0, .p1 = p1, .p2 = p2 };
}

v2 font_outline_point(FontOutline* outline, const FT_Vector* point) {
    return v2_new((f32)point->x * outline->scale, (f32)point->y * outline->scale);
}

// FreeType callbacks. They return int, as FT_Outline_Funcs requires.
int font_outline_move_to(const FT_Vector* to, void* user) {
    FontOutline* outline = (FontOutline*)user;
    outline->pen = font_outline_point(outline, to);
    return 0;
}

int font_outline_line_to(const FT_Vector* to, void* user) {
    FontOutline* outline = (FontOutline*)user;
    v2 p2 = font_outline_point(outline, to);
    if(p2.x != outline->pen.x || p2.y != outline->pen.y) {
        font_outline_push_curve(outline, outline->pen, v2_scale(v2_add(outline->pen, p2), 0.5f), p2);
    }
    outline->pen = p2;
    return 0;
}

int font_outline_conic_to(const FT_Vector* control, const FT_Vector* to, void* user) {
    FontOutline* outline = (FontOutline*)user;
    v2 p2 = font_outline_point(outline, to);
    font_outline_push_curve(outline, outline->pen, font_outline_point(outline, control), p2);
    outline->pen = p2;
    return 0;
}

// Cubics (CFF outlines) are split in half, and each half approximated by one quadratic
int font_outline_cubic_to(const FT_Vector* control1, const FT_Vector* control2, const FT_Vector* to, void* user) {
    FontOutline* outline = (FontOutline*)user;
    v2 p0 = outline->pen;
    v2 c1 = font_outline_point(outline, control1);
    v2 c2 = font_outline_point(outline, control2);
    v2 p3 = font_outline_point(outline, to);

    // de Casteljau split at t = 0.5
    v2 p01  = v2_scale(v2_add(p0, c1), 0.5f);
    v2 p12  = v2_scale(v2_add(c1, c2), 0.5f);
    v2 p23  = v2_scale(v2_add(c2, p3), 0.5f);
    v2 p012 = v2_scale(v2_add(p01, p12), 0.5f);
    v2 p123 = v2_scale(v2_add(p12, p23), 0.5f);
    v2 mid  = v2_scale(v2_add(p012, p123), 0.5f);

    // A cubic a, b, c, d is closest to the quadratic with control (3(b + c) - a - d) / 4
    v2 q0 = v2_scale(v2_sub(v2_scale(v2_add(p01, p012), 3.0f), v2_add(p0, mid)), 0.25f);
    v2 q1 = v2_scale(v2_sub(v2_scale(v2_add(p123, p23), 3.0f), v2_add(mid, p3)), 0.25f);
    font_outline_push_curve(outline, p0, q0, mid);
    font_outline_push_curve(outline, mid, q1, p3);
    outline->pen = p3;
    return 0;
}

u64 font_vector_push_asset(AssetBuilder* builder, String tag, String path, Stack* stack) {
    // FreeType wants a null-terminated path
    char* path_buf = (char*)alloca(path.len + 1);
    String path_c = string_init(path_buf, path.len + 1);
    string_cat(&path_c, path);
    string_write_null_terminator(&path_c);

    FT_Library ft;
    if(FT_Init_FreeType(&ft)) {
        log_exit("Font: failed to initialize FreeType");
    }
    FT_Face ft_face;
    if(FT_New_Face(ft, path_buf, 0, &ft_face)) {
        log_exit("Font: failed to open %s", path_buf);
    }
    f32 scale = 1.0f / (f32)ft_face->units_per_EM;

    // Every glyph's curves go in one array, and every glyph's bands in another.
    // A band header is two u32s, and each curve is listed at most once per band.
    FontOutline outline = {};
    outline.curves = (FontCurve*)stack_alloc(stack, FONT_VECTOR_MAX_CURVES * sizeof(FontCurve));
    outline.scale  = scale;
    u32 bands_capacity = FONT_VECTOR_MAX_CURVES * 2 * FONT_VECTOR_MAX_BANDS + FONT_GLYPH_COUNT * 4 * FONT_VECTOR_MAX_BANDS;
    u32* bands = (u32*)stack_alloc(stack, bands_capacity * sizeof(u32));
    u32 bands_len = 0;

    FT_Outline_Funcs funcs = {
        .move_to  = font_outline_move_to,
        .line_to  = font_outline_line_to,
        .conic_to = font_outline_conic_to,
        .cubic_to = font_outline_cubic_to,
    };

    FontVectorGlyph* glyphs = (FontVectorGlyph*)stack_alloc_zero(stack, FONT_GLYPH_COUNT * sizeof(FontVectorGlyph));
    FontVectorData* font = (FontVectorData*)stack_alloc_zero(stack, sizeof(FontVectorData));
    font->ascender    = (f32)ft_face->ascender * scale;
    font->descender   = (f32)ft_face->descender * scale;
    font->line_height = (f32)ft_face->height * scale;
    for(i32 i = 0; i < FONT_GLYPH_COUNT; i++) {
        // Unscaled and unhinted, so points and advances are in font units
        if(FT_Load_Char(ft_face, (FT_ULong)i, FT_LOAD_NO_SCALE)) {
            log_exit("Font: failed to load character %i of %s", i, path_buf);
        }
        font->glyphs[i].advance = (f32)ft_face->glyph->advance.x * scale;

        u32 first_curve = outline.curves_len;
        if(ft_face->glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
            FT_Outline_Decompose(&ft_face->glyph->outline, &funcs, &outline);
        }
        FontCurve* curves = &outline.curves[first_curve];
        u32 curves_len = outline.curves_len - first_curve;

        FontVectorGlyph* glyph = &glyphs[i];
        glyph->curves_offset = first_curve;
        glyph->bands_offset  = bands_len;
        if(curves_len == 0) {
            continue;
        }

        // Bounds
        v4 bounds = v4_new(1e30f, 1e30f, -1e30f, -1e30f);
        for(i32 c = 0; c < curves_len; c++) {
            v2 points[3] = { curves[c].p0, curves[c].p1, curves[c].p2 };
            for(i32 p = 0; p < 3; p++) {
                bounds.x = fminf(bounds.x, points[p].x);
                bounds.y = fminf(bounds.y, points[p].y);
                bounds.z = fmaxf(bounds.z, points[p].x);
                bounds.w = fmaxf(bounds.w, points[p].y);
            }
        }
        glyph->bounds = bounds;
        font->glyphs[i].bounds = bounds;

        // About one band per four curves, so most bands hold a handful
        u32 bands_per_axis = (curves_len + 3) / 4;
        if(bands_per_axis > FONT_VECTOR_MAX_BANDS) {
            bands_per_axis = FONT_VECTOR_MAX_BANDS;
        }
        glyph->h_bands_len = bands_per_axis;
        glyph->v_bands_len = bands_per_axis;
        assert(bands_len + 4 * bands_per_axis + 2 * bands_per_axis * curves_len <= bands_capacity);

        // Axis 0 is the horizontal bands: they split y, and their rays run along x.
        // Axis 1 is the vertical bands, the other way round.
        u32* glyph_bands = &bands[bands_len];
        u32 glyph_bands_len = 4 * bands_per_axis;
        for(i32 axis = 0; axis < 2; axis++) {
            i32 split = 1 - axis;
            i32 ray = axis;
            f32 split_min = bounds.comps[split];
            f32 band_size = (bounds.comps[split + 2] - split_min) / (f32)bands_per_axis;
            // Curves that touch a band edge go in both bands, so no pixel misses one
            f32 epsilon = band_size * (1.0f / 1024.0f);
            for(i32 band = 0; band < bands_per_axis; band++) {
                f32 band_min = split_min + band_size * (f32)band - epsilon;
                f32 band_max = split_min + band_size * (f32)(band + 1) + epsilon;
                u32* list = &glyph_bands[glyph_bands_len];
                u32 list_len = 0;
                for(i32 c = 0; c < curves_len; c++) {
                    FontCurve* curve = &curves[c];
                    f32 curve_min = fminf(fminf(curve->p0.comps[split], curve->p1.comps[split]), curve->p2.comps[split]);
                    f32 curve_max = fmaxf(fmaxf(curve->p0.comps[split], curve->p1.comps[split]), curve->p2.comps[split]);
                    if(curve_max < band_min || curve_min > band_max) {
                        continue;
                    }

                    // Insertion sort by descending max along the ray, so the shader can stop at
                    // the first curve wholly behind the pixel
                    f32 key = fmaxf(fmaxf(curve->p0.comps[ray], curve->p1.comps[ray]), curve->p2.comps[ray]);
                    i32 j = list_len;
                    while(j > 0) {
                        FontCurve* other = &curves[list[j - 1]];
                        f32 other_key = fmaxf(fmaxf(other->p0.comps[ray], other->p1.comps[ray]), other->p2.comps[ray]);
                        if(other_key >= key) {
                            break;
                        }
                        list[j] = list[j - 1];
                        j--;
                    }
                    list[j] = c;
                    list_len++;
                }

                u32 header = (axis * bands_per_axis + band) * 2;
                glyph_bands[header]     = list_len;
                glyph_bands[header + 1] = glyph_bands_len;
                glyph_bands_len += list_len;
            }
        }
        bands_len += glyph_bands_len;
    }
    FT_Done_Face(ft_face);
    FT_Done_FreeType(ft);

    // Push the GPU data first, then make the glyphs' offsets relative to the
    // region starts, which is where the shader indexes from
    u64 curves_offset = 0;
    u64 bands_offset  = 0;
    u64 glyphs_offset = 0;
    asset_builder_push_asset(builder, tag, string_const("FONT_VECTOR_CURVES"), string_const("FontCurve"),
        outline.curves, outline.curves_len * sizeof(FontCurve), &curves_offset);
    asset_builder_push_asset(builder, tag, string_const("FONT_VECTOR_BANDS"), string_const("u32"),
        bands, bands_len * sizeof(u32), &bands_offset);
    assert(curves_offset % sizeof(FontCurve) == 0);
    for(i32 i = 0; i < FONT_GLYPH_COUNT; i++) {
        glyphs[i].curves_offset += (u32)(curves_offset / sizeof(FontCurve));
        glyphs[i].bands_offset  += (u32)(bands_offset / sizeof(u32));
    }
    asset_builder_push_asset(builder, tag, string_const("FONT_VECTOR_GLYPHS"), string_const("FontVectorGlyph"),
        glyphs, FONT_GLYPH_COUNT * sizeof(FontVectorGlyph), &glyphs_offset);
    assert(glyphs_offset % sizeof(FontVectorGlyph) == 0);
    font->first_glyph = (u32)(glyphs_offset / sizeof(FontVectorGlyph));
    log_print(LOG_ASSET, "Font %s: %u curves, %u band words", path_buf, outline.curves_len, bands_len);

    u64 handle = asset_builder_next_handle_of_type(builder, string_const("FONT_VECTOR"));
    asset_builder_push_asset(builder, tag, string_const("FONT_VECTOR"), string_const("FontVectorData"), font, sizeof(FontVectorData), NULL);
    return handle;
}

#endif
#endif
