#ifndef handrail_font_h_INCLUDED
#define handrail_font_h_INCLUDED

#define FONT_GLYPH_COUNT 256

typedef struct {
	u32 position[2];
	u32 size[2];
	i32 bearing[2];
	u32 advance;
} FontGlyph;

typedef struct {
    u64 texture_width;
    u32 texture_handle;
    u32 point_size;
    FontGlyph glyphs[FONT_GLYPH_COUNT];
} FontData;

v4  font_glyph_src(FontData* font, FontGlyph* glyph);
u64 font_size();

#ifdef CSM_FONT_PROCESSING
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

FontAndTextureData font_and_texture_from_ttf(char* path, u32 point_size, u32 texture_handle, Stack* stack);
#endif

#ifdef CSM_IMPLEMENTATION

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

#ifdef CSM_FONT_PROCESSING
FontAndTextureData font_and_texture_from_ttf(char* path, u32 point_size, u32 texture_handle, Stack* stack) {
	FT_Library ft;
	if(FT_Init_FreeType(&ft)) { 
    	panic(); 
	}
	FT_Face ft_face;
	if(FT_New_Face(ft, path, 0, &ft_face)) { 
    	panic(); 
	}
	FT_Set_Pixel_Sizes(ft_face, 0, point_size);

	FontGlyphInfo glyphs[FONT_GLYPH_COUNT];
	u32 pack_order[FONT_GLYPH_COUNT];
	for(i32 i = 0; i < FONT_GLYPH_COUNT; i++) {
		if(FT_Load_Char(ft_face, (unsigned char)i, FT_LOAD_RENDER)) { panic(); }
		pack_order[i] = i;

		FontGlyphInfo* glyph = &glyphs[i];
		glyph->glyph.size[0]    = ft_face->glyph->bitmap.width;
		glyph->glyph.size[1]    = ft_face->glyph->bitmap.rows;
		glyph->glyph.bearing[0] = ft_face->glyph->bitmap_left;
		glyph->glyph.bearing[1] = ft_face->glyph->bitmap_top;
		glyph->glyph.advance    = (u32)ft_face->glyph->advance.x;

		u32 bm_size = sizeof(u8) * glyph->glyph.size[0] * glyph->glyph.size[1];
		glyph->pixels = (u8*)stack_alloc(stack, bm_size);
		memcpy(glyph->pixels, ft_face->glyph->bitmap.buffer, bm_size);

	}
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

	// Pack rects using shelf algorithm.
	u32 atlas_width = 16;

// This is essentially a while loop that goes until a large enough atlas size
// is found. It seems nicer this way to me.
try_pack_again:

	atlas_width *= 2;
	u32 atlas_area = atlas_width * atlas_width;

	i32 curx = 0;
	i32 cury = 0;
	i32 cur_shelf_size = 0;
	for(i32 i = 0; i < FONT_GLYPH_COUNT; i++) {
		FontGlyphInfo* glyph = &glyphs[pack_order[i]];
		if(cur_shelf_size == 0) {
			cur_shelf_size = glyph->glyph.size[1];
		}

		if(curx + glyph->glyph.size[0] > atlas_width) {
			if(cur_shelf_size == 0) {
				goto try_pack_again;
			}

			cury += cur_shelf_size;
			cur_shelf_size = glyph->glyph.size[1];
			curx = 0;
			//printf("New shelf! x,y,h:%i,%i,%i\n", curx, cury, cur_shelf_size);
		}

		glyph->glyph.position[0] = curx;
		glyph->glyph.position[1] = cury;
		//printf("Rect packed (x,y,w,h): %.3u,%.3u,%.3u,%.3u\n", rect->x, rect->y, rect->w, rect->h);
		curx += glyph->glyph.size[0];

		if(cury + cur_shelf_size >= atlas_width) {
			goto try_pack_again;
		}
	}

	// If we made it here, we found a good atlas size, so it's time to allocate
	// space and render glyphs to the buffer
	TextureData* tex_data = (TextureData*)stack_alloc(
	    stack, texture_size_from_dimensions(atlas_width, atlas_width, 1));
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
    font_data->texture_width = tex_data->width;
	font_data->texture_handle = texture_handle;
	font_data->point_size = point_size;
	for(i32 i = 0; i < FONT_GLYPH_COUNT; i++) {
		font_data->glyphs[i] = glyphs[i].glyph;
	}

    FontAndTextureData data = {};
    data.texture = tex_data;
    data.font = font_data;
	return data;
}

#endif
#endif
#endif
