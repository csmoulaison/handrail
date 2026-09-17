#ifndef handrail_sprite_h_INCLUDED
#define handrail_sprite_h_INCLUDED

#ifndef SPRITE_ATLAS_MAX_SPRITES
#define SPRITE_ATLAS_MAX_SPRITES 4096
#endif

#ifndef SPRITE_MAX_FRAMES
#define SPRITE_MAX_FRAMES 64
#endif

typedef struct {
    iv2 atlas_position;
    f32 duration;
} SpriteFrame;

typedef struct {
    u64         texture_handle;
    iv2         origin;
    iv2         size;
    u32         frames_len;
    SpriteFrame frames[];
} SpriteData;

typedef struct {
    TextureData* textures[SPRITE_MAX_FRAMES];
} SpriteTextureList;

typedef struct {
    SpriteData*       sprites[SPRITE_ATLAS_MAX_SPRITES];
    String            sprite_tags[SPRITE_ATLAS_MAX_SPRITES];
    SpriteTextureList texture_lists[SPRITE_ATLAS_MAX_SPRITES];
    u32               sprites_len;
    TextureFormat     format;
} SpriteAtlasBuilder;

u64 sprite_size_from_frame_count(u64 frame_count);
u64 sprite_size(SpriteData* sprite);

void sprite_atlas_builder_init(SpriteAtlasBuilder* builder, TextureFormat format);
void sprite_atlas_push_sprite(SpriteAtlasBuilder* builder, String tag, TextureData** textures, f32* frame_durations, u64 frame_count, iv2 origin, Stack* stack);
void sprite_atlas_build_assets(SpriteAtlasBuilder* builder, AssetBuilder* pack_builder, Stack* stack);
void push_palettized_sprite_renderer_assets(
    AssetBuilder* asset_builder, 
    u32* palette_colors, 
    u32* palette_indices, 
    i32 palette_indices_len, 
    String aseprite_path, 
    String bmp_path, 
    Stack* stack);

// Forward declaration
void aseprite_directory_to_sprite_atlas(String aseprite_dir, String bmp_dir, SpriteAtlasBuilder* atlas, Stack* stack);

#ifdef CSM_IMPLEMENTATION

u64 sprite_size_from_frame_count(u64 frame_count) {
    return sizeof(SpriteData) + frame_count * sizeof(SpriteFrame);
}

u64 sprite_size(SpriteData* sprite) {
    return sprite_size_from_frame_count(sprite->frames_len);
}

void sprite_atlas_builder_init(SpriteAtlasBuilder* builder, TextureFormat format) {
    builder->sprites_len = 0;
    builder->format = format;
}

void sprite_atlas_push_sprite(SpriteAtlasBuilder* builder, String tag, TextureData** textures, f32* frame_durations, u64 frame_count, iv2 origin, Stack* stack) {
    assert(frame_count > 0);
    assert(builder->sprites_len < SPRITE_ATLAS_MAX_SPRITES - 1);
    assert(textures[0]->format == builder->format);

    builder->sprite_tags[builder->sprites_len] = tag;
    SpriteData* sprite = (SpriteData*)stack_alloc(stack, sizeof(SpriteData) + frame_count * sizeof(SpriteFrame));
    builder->sprites[builder->sprites_len] = sprite;
    SpriteTextureList* texture_list = &builder->texture_lists[builder->sprites_len];
    builder->sprites_len++;
    sprite->origin = origin;
    sprite->frames_len = frame_count;

    for(i32 i = 0; i < frame_count; i++) {
        SpriteFrame* frame = &sprite->frames[i];
        frame->duration = frame_durations[i];
        texture_list->textures[i] = textures[i];
    }
}

void sprite_atlas_build_assets(SpriteAtlasBuilder* builder, AssetBuilder* pack_builder, Stack* stack) {
    // Sort sprites by height.
	u32* pack_order = (u32*)alloca(builder->sprites_len);
	for(u32 i = 0; i < builder->sprites_len; i++) {
    	pack_order[i] = i;
	}

	for(u32 i = 0; i < builder->sprites_len; i++) {
		for(i32 j = 0; j < builder->sprites_len - 1; j++) {
    		TextureData* tex_a0 = builder->texture_lists[pack_order[j]].textures[0];
    		TextureData* tex_b0 = builder->texture_lists[pack_order[j + 1]].textures[0];
			if(tex_a0->height < tex_b0->height) {
				u32 tmp = pack_order[j];
				pack_order[j] = pack_order[j + 1];
				pack_order[j + 1] = tmp;
			}
		}
	}

	// Pack sprites with shelf algorithm.
	u32 atlas_width = 64;
try_pack_again:
	atlas_width *= 2;
	u32 atlas_area = atlas_width * atlas_width;

	i32 curx = 0;
	i32 cury = 0;
	i32 cur_shelf_size = 0;
	for(i32 i = 0; i < builder->sprites_len; i++) {
    	SpriteTextureList* list = &builder->texture_lists[pack_order[i]];
    	SpriteData* sprite      = builder->sprites[pack_order[i]];
    	sprite->size = iv2_new(list->textures[0]->width, list->textures[0]->height);

    	for(i32 j = 0; j < sprite->frames_len; j++) {
    		TextureData* tex   = builder->texture_lists[pack_order[i]].textures[j];
            SpriteFrame* frame = &sprite->frames[j];

    		if(cur_shelf_size == 0) {
    			cur_shelf_size = tex->height;
    		}

    		if(curx + tex->width > atlas_width) {
    			if(cur_shelf_size == 0) {
    				goto try_pack_again;
    			}

    			cury += cur_shelf_size;
    			cur_shelf_size = tex->height;
    			curx = 0;
    		}

    		frame->atlas_position.x = curx;
    		frame->atlas_position.y = cury;
    		curx += tex->height;

    		if(cury + cur_shelf_size >= atlas_width) {
    			goto try_pack_again;
    		}
    	}
	}

    // Packing size was successful, now render rects to atlas texture.
	TextureData* atlas = (TextureData*)stack_alloc(
	    stack, texture_size_from_dimensions(atlas_width, atlas_width, builder->format));
	atlas->width  = atlas_width;
	atlas->height = atlas_width;
	atlas->format = builder->format;
    u8 pixel_bytes = texture_format_bytes_per_pixel(builder->format);
	memset(atlas->pixel_buffer, 0, atlas_width * atlas_width * pixel_bytes);

	for(i32 i = 0; i < builder->sprites_len; i++) {
    	printf("sprites %d!\n", i);
    	SpriteTextureList* list = &builder->texture_lists[i];
    	SpriteData* sprite      = builder->sprites[i];
    	for(i32 j = 0; j < sprite->frames_len; j++) {
        	printf("frame %d\n", j);
    		TextureData* tex   = list->textures[j];
            SpriteFrame* frame = &sprite->frames[j];
    		for(i32 y = 0; y < tex->height; y++) {
    			for(i32 x = 0; x < tex->width; x++) {
    				i32 dst_x = frame->atlas_position.x + x;
    				i32 dst_y = frame->atlas_position.y + y;
    				u8* src_pixel = &tex->pixel_buffer[(y * tex->width + x) * pixel_bytes];
    				u8* dst_pixel = &atlas->pixel_buffer[(dst_y * atlas_width + dst_x) * pixel_bytes];
    				memcpy(dst_pixel, src_pixel, pixel_bytes);
    				if(*src_pixel == 0) {
        				printf("  ");
    				} else {
            			printf("%2d", *src_pixel);
    				}
    				//dst_pixel[0] = src_pixel[0];
    				//dst_pixel[1] = src_pixel[1];
    				//dst_pixel[2] = src_pixel[2];
    				//dst_pixel[3] = src_pixel[3];
    			}
    			printf("\n");
    		}
    	}
	}

    // NOW: is this only for atlas case with the one texture from duckies?
    //u64 texture_handle = asset_builder_next_handle_of_type(pack_builder, string_const("TEXTURE"));
    u64 texture_handle = 0;
    //assert(texture_handle == 0);
    asset_builder_push_asset(pack_builder, 
        string_const("SPRITE_ATLAS"), string_const("TEXTURE"), string_const("TextureData"),
        atlas, texture_size(atlas));

    for(i32 i = 0; i < builder->sprites_len; i++) {
        SpriteData* sprite = builder->sprites[i];
        sprite->texture_handle = texture_handle;

        asset_builder_push_asset(pack_builder, 
            builder->sprite_tags[i], string_const("SPRITE"), string_const("SpriteData"),
            sprite, sprite_size(sprite));
    }
}

void push_palettized_sprite_renderer_assets(
    AssetBuilder* asset_builder, 
    u32* palette_colors, 
    u32* palette_indices, 
    i32 palette_indices_len, 
    String aseprite_path, 
    String bmp_path, 
    Stack* stack) 
{
    SpriteAtlasBuilder* atlas_builder = (SpriteAtlasBuilder*)stack_alloc(stack, sizeof(SpriteAtlasBuilder));
    sprite_atlas_builder_init(atlas_builder, TEXTURE_FORMAT_8_BIT_PALLETIZED);
    aseprite_directory_to_sprite_atlas(aseprite_path, bmp_path, atlas_builder, stack);
    sprite_atlas_build_assets(atlas_builder, asset_builder, stack);

    u32 palette_pixels[256] = {};
    //for(i32 i = 0; i < palette_indices_len; i++) {
    //    palette_pixels[i] = palette_colors[palette_indices[i]];
    //}
    for(i32 i = 0; i < 256; i++) {
        if(i < palette_indices_len) {
            palette_pixels[i] = palette_colors[palette_indices[i]];
        } else {
            palette_pixels[i] = 0xFF888888;
        }
    }
    TextureData* palette = (TextureData*)stack_alloc(stack, texture_size_from_dimensions(256, 1, TEXTURE_FORMAT_RGBA));
    palette->width = 256;
    palette->height = 1;
    palette->format = TEXTURE_FORMAT_RGBA;
    memcpy(palette->pixel_buffer, palette_pixels, 256 * sizeof(u32));
    asset_builder_push_asset(asset_builder, 
        string_const("PALETTE"), string_const("TEXTURE"), string_const("TextureData"),
        palette, texture_size(palette));

    Primitive2dData* primitive = primitive_2d_from_data(primitive_2d_quad_vertices, PRIMITIVE_2D_QUAD_VERTICES_LEN, stack);
    asset_builder_push_asset(asset_builder, 
        string_const("QUAD"), string_const("PRIMITIVE_2D"), string_const("Primitive2dData"),
        primitive, primitive_2d_size(primitive));
}

#endif
#endif
