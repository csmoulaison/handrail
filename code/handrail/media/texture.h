#ifndef handrail_texture_h_INCLUDED
#define handrail_texture_h_INCLUDED

#define TEXTURE_FORMAT_1_BIT            0
#define TEXTURE_FORMAT_8_BIT_PALLETIZED 1
#define TEXTURE_FORMAT_R                2
#define TEXTURE_FORMAT_RGB              3
#define TEXTURE_FORMAT_RGBA             4
typedef u8 TextureFormat;

typedef struct {
    union {
        u32 pixel;
        struct {
            u8 r, g, b, a;
        } components;
    };
} TexturePixel32;

// In-memory texture, as produced by the loaders below.
typedef struct {
    u64           width;
    u64           height;
    TextureFormat format;
    u8            pixel_buffer[];
} TextureData;

// Pushed TEXTURE asset. The pixels live in the TEXTURE_PIXELS region, which
// the renderer copies into images; only this header stays CPU-side. format is
// TEXTURE_FORMAT_RGBA or TEXTURE_FORMAT_R (one byte per pixel; palettized
// textures keep their indices).
typedef struct {
    u32 width;
    u32 height;
    u32 format;
    u32 pixels_offset;
} TextureAsset;

#pragma pack(push, 1)
typedef struct {
    // File header
    u16 signature;
    u32 file_size;
    u32 reserved;
    u32 data_offset;
    // Info header
    u32 info_size;
    u32 width;
    u32 height;
    u16 planes;
    u16 bits_per_pixel;
    u32 compression;
    u32 image_size;
    u32 x_pixels_per_meter;
    u32 y_pixels_per_meter;
    u32 colors_used;
    u32 important_colors;
} TextureBmpInfo;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    // File header
    u16 signature;
    u32 file_size;
    u32 reserved;
    u32 data_offset;
    // Info header
    u32 info_size;
    u32 width;
    u32 height;
    u16 planes;
    u16 bits_per_pixel;
    u32 compression;
    u32 image_size;
    u32 x_pixels_per_meter;
    u32 y_pixels_per_meter;
    u32 colors_used;
    u32 important_colors;
    // v3 header only
    u32 red_mask;
    u32 green_mask;
    u32 blue_mask;
    u32 alpha_mask;
} TextureBmpV3Info;
#pragma pack(pop)

// TODO: choose channels when loading
TextureData* texture_from_bmp(File* file, Stack* stack);
// Creates an 8 bit texture
TextureData* texture_from_bmp_4_bit_palletized(File* file, Stack* stack);
u64 texture_size_from_dimensions(u64 width, u64 height, u8 format);
u64 texture_size(TextureData* texture);
u8 texture_format_bits_per_pixel(TextureFormat format);
u8 texture_format_bytes_per_pixel(TextureFormat format);
// Push a TEXTURE header plus its pixels, converted to a GPU format. Returns the TEXTURE handle.
u64 texture_push_asset(AssetBuilder* builder, String tag, TextureData* texture, Stack* stack);

#endif

#if defined(HANDRAIL_IMPLEMENTATION_PASS) && !defined(handrail_texture_h_IMPLEMENTED)
#define handrail_texture_h_IMPLEMENTED

TextureData* texture_from_bmp(File* file, Stack* stack) {
    assert(sizeof(TextureBmpInfo) == 54);
    TextureBmpInfo info = {};
    file_read(file, &info, sizeof(TextureBmpInfo));

    assert(info.reserved == 0);
    assert(info.signature == 0x4D42); // 'BM'
    log_print(LOG_ASSET, "BMP: %u bits per pixel", (u32)info.bits_per_pixel);
    assert(info.bits_per_pixel == 32);
    assert(info.compression == 0);

    TextureData* data = (TextureData*)stack_alloc(
        stack, texture_size_from_dimensions(info.width, info.height, TEXTURE_FORMAT_RGBA));
    data->width = info.width;
    data->height = info.height;
    data->format = TEXTURE_FORMAT_RGBA;
    for(i32 i = 0; i < info.width * info.height; i++) {
        u8 src[4];
        file_read(file, src, sizeof(src));
        TexturePixel32* dst = (TexturePixel32*)&data->pixel_buffer[i * 4];
        // BMP files store pixels in ABGR order.
        dst->components.r = src[3];
        dst->components.g = src[2];
        dst->components.b = src[1];
        dst->components.a = src[0];
    }
    return data;
}

TextureData* texture_from_bmp_4_bit_palletized(File* file, Stack* stack) {
    TextureBmpInfo info = {};
    file_read(file, &info, sizeof(TextureBmpInfo));

    assert(info.reserved       == 0);
    assert(info.signature      == 0x4D42); // 'BM'
    assert(info.bits_per_pixel == 4);
    assert(info.compression    == 0);
    assert(info.width % 8      == 0); // Otherwise we would have to account for padding
    assert(info.height % 8     == 0);

    TextureData* data = (TextureData*)stack_alloc(
        stack, texture_size_from_dimensions(info.width, info.height, TEXTURE_FORMAT_8_BIT_PALLETIZED));
    data->width = info.width;
    data->height = info.height;
    data->format = TEXTURE_FORMAT_8_BIT_PALLETIZED;
    file_seek_from_start(file, info.data_offset);
    for(i32 i = 0; i < (info.width * info.height) / 2; i++) {
        u8 packed;
        file_read(file, &packed, 1);
        u8 upper = (packed >> 4) & 0x0F;
        u8 lower = packed & 0x0F;
        u8* dst = &data->pixel_buffer[i * 2];
        dst[0] = upper;
        dst[1] = lower;
    }
    return data;
}

u64 texture_size_from_dimensions(u64 width, u64 height, TextureFormat format) {
    u8 pixel_bytes = texture_format_bytes_per_pixel(format);
    return sizeof(TextureData) + width * height * pixel_bytes;
}

u64 texture_size(TextureData* data) {
    return texture_size_from_dimensions(data->width, data->height, data->format);
}

u8 texture_format_bits_per_pixel(TextureFormat format) {
    switch(format) {
        case TEXTURE_FORMAT_1_BIT:            return 1; 
        case TEXTURE_FORMAT_8_BIT_PALLETIZED: return 8; 
        case TEXTURE_FORMAT_R:                return 8; 
        case TEXTURE_FORMAT_RGB:              return 24;
        case TEXTURE_FORMAT_RGBA:             return 32;
        default: panic();
    }
    panic();
}

u8 texture_format_bytes_per_pixel(TextureFormat format) {
    u8 pixel_bits = texture_format_bits_per_pixel(format);
    return (pixel_bits + 8 - 1) / 8;
}

u64 texture_push_asset(AssetBuilder* builder, String tag, TextureData* texture, Stack* stack) {
    TextureAsset asset = {};
    asset.width  = (u32)texture->width;
    asset.height = (u32)texture->height;
    u64 pixels_len = texture->width * texture->height;
    u8* pixels = texture->pixel_buffer;
    u64 pixels_size = 0;

    // Convert to a format every GPU can sample
    switch(texture->format) {
        case TEXTURE_FORMAT_RGBA: {
            asset.format = TEXTURE_FORMAT_RGBA;
            pixels_size = pixels_len * 4;
        } break;
        case TEXTURE_FORMAT_RGB: {
            asset.format = TEXTURE_FORMAT_RGBA;
            pixels_size = pixels_len * 4;
            pixels = (u8*)stack_alloc(stack, pixels_size);
            for(u64 i = 0; i < pixels_len; i++) {
                pixels[i * 4 + 0] = texture->pixel_buffer[i * 3 + 0];
                pixels[i * 4 + 1] = texture->pixel_buffer[i * 3 + 1];
                pixels[i * 4 + 2] = texture->pixel_buffer[i * 3 + 2];
                pixels[i * 4 + 3] = 0xFF;
            }
        } break;
        case TEXTURE_FORMAT_R:
        case TEXTURE_FORMAT_8_BIT_PALLETIZED: {
            asset.format = TEXTURE_FORMAT_R;
            pixels_size = pixels_len;
        } break;
        default: {
            log_exit("texture_push_asset: unsupported texture format %u", texture->format);
        }
    }

    u64 pixels_offset = 0;
    asset_builder_push_asset(builder, tag, string_const("TEXTURE_PIXELS"), string_const("u8"), pixels, pixels_size, &pixels_offset);
    asset.pixels_offset = (u32)pixels_offset;

    u64 handle = asset_builder_next_handle_of_type(builder, string_const("TEXTURE"));
    asset_builder_push_asset(builder, tag, string_const("TEXTURE"), string_const("TextureAsset"), &asset, sizeof(TextureAsset), NULL);
    return handle;
}

#endif
