#ifndef handrail_font_h_INCLUDED
#define handrail_font_h_INCLUDED

#define FONT_GLYPH_COUNT 256

// Empty pixels left between glyphs in the atlas, so linear sampling doesn't
// bleed into neighbours.
#define FONT_GLYPH_PADDING 1

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

// Normalized x, y, w, h of a glyph within the font's atlas
v4  font_glyph_src(FontData* font, FontGlyph* glyph);
u64 font_size();

#ifdef HANDRAIL_FONT_PROCESSING
#include <ft2build.h>
#include FT_FREETYPE_H
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

#endif
#endif
