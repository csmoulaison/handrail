#ifndef handrail_aseprite_h_INCLUDED
#define handrail_aseprite_h_INCLUDED

#ifndef ASEPRITE_EXE_PATH
#define ASEPRITE_EXE_PATH "~/.steam/steam/steamapps/common/Aseprite/aseprite"
#endif

#pragma pack(push, 1)
typedef struct {
    u32 file_size;
    u16 magic_number;
    u16 frame_count;
    u16 width;
    u16 height;
    u16 bits_per_pixel;
    u32 flags;
    u16 speed;
    u32 set1;
    u32 set2;
    u8  palette_entry;
    u8  ignore[3];
    u16 color_count;
    u8  pixel_width;
    u8  pixel_height;
    i16 grid_position_x;
    i16 grid_position_y;
    u16 grid_width;
    u16 grid_height;
    u8  for_future[84];
} AsepriteHeader;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    u32 byte_size;
    u16 magic_number;
    u16 old_chunks_field;
    u16 duration;
    u8  for_future[2];
    u32 new_chunks_field;
} AsepriteFrame;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    u32 byte_size;
    u16 type;
    u8  data[];
} AsepriteChunkHeader;
#pragma pack(pop)

typedef struct {
    TextureData** textures;
    f32*          frame_durations;
    i32           frame_count;
    iv2           origin;
} AsepriteData;

AsepriteData textures_from_aseprite(String path, String tag, String bmp_dir, Stack* stack);
void         aseprite_to_sprite_atlas(String path, String tag, String bmp_dir, SpriteAtlasBuilder* atlas, Stack* stack);
void         aseprite_directory_to_sprite_atlas(String aseprite_dir, String bmp_dir, SpriteAtlasBuilder* atlas, Stack* stack);

#ifdef CSM_IMPLEMENTATION

AsepriteData textures_from_aseprite(String path, String tag, String bmp_dir, Stack* stack) {
    assert(sizeof(AsepriteHeader) == 128);
    assert(sizeof(AsepriteFrame) == 16);
    AsepriteData result = {};

    String cmd = string_from_stack(stack, 1024);
    string_cat(&cmd, string_const(ASEPRITE_EXE_PATH));
    string_cat(&cmd, string_const(" -b "));
    string_cat(&cmd, path);
    string_cat(&cmd, string_const(" --save-as "));
    string_cat(&cmd, bmp_dir);
    string_cat(&cmd, tag);
    string_cat(&cmd, string_const("_.bmp"));
    string_write_null_terminator(&cmd);
    printf("%s\n", cmd.text);
    system(cmd.text);

    File file = file_open(path, FILE_OPEN_READ);
    AsepriteHeader header = {};
    file_read(&file, &header, sizeof(AsepriteHeader));
    assert(header.magic_number == 0xA5E0);

    // NOTO: Check user data for origin?
    result.origin = iv2_new(0, 0); 
    result.frame_count = header.frame_count;
    result.frame_durations = (f32*)stack_alloc(stack, result.frame_count * sizeof(f32));
    result.textures = (TextureData**)stack_alloc(stack, result.frame_count * sizeof(TextureData*));

    for(i32 i = 0; i < result.frame_count; i++) {
        AsepriteFrame frame = {};
        file_read(&file, &frame, sizeof(AsepriteFrame));
        assert(frame.magic_number == 0xF1FA);
        result.frame_durations[i] = frame.duration;
        file_seek(&file, frame.byte_size - sizeof(frame));
    }
    file_close(&file);

    String tex_path = string_from_stack(stack, 1024);
    for(i32 i = 0; i < result.frame_count; i++) {
        string_clear(&tex_path);
        string_cat(&tex_path, bmp_dir);
        string_cat(&tex_path, tag);
        string_cat(&tex_path, string_const("_"));
        if(result.frame_count > 1) {
            string_print_int(&tex_path, i + 1);
        }
        string_cat(&tex_path, string_const(".bmp"));

        File tex_file = file_open(tex_path, FILE_OPEN_READ);
        result.textures[i] = texture_from_bmp_4_bit_palletized(&tex_file, stack);
        file_close(&tex_file);
    }
    return result;
}

void aseprite_to_sprite_atlas(String path, String tag, String bmp_dir, SpriteAtlasBuilder* atlas, Stack* stack) {
    AsepriteData ase = textures_from_aseprite(path, tag, bmp_dir, stack);
    sprite_atlas_push_sprite(atlas, tag, ase.textures, ase.frame_durations, ase.frame_count, ase.origin, stack);
}

void aseprite_directory_to_sprite_atlas(String aseprite_dir, String bmp_dir, SpriteAtlasBuilder* atlas, Stack* stack) {
    i32 aseprite_paths_len = 0;
    String* aseprite_paths = file_paths_in_directory(aseprite_dir, &aseprite_paths_len, stack);
    String* aseprite_names = file_names_in_directory(aseprite_dir, &aseprite_paths_len, stack);
    for(i32 i = 0; i < aseprite_paths_len; i++) {
        String fname = aseprite_names[i];
        String tag = string_from_stack(stack, fname.len);
        string_cat(&tag, fname);
        string_replace_substring(&tag, string_const(".aseprite"), string_const(""));
        aseprite_to_sprite_atlas(aseprite_paths[i], tag, bmp_dir, atlas, stack);
    }
}

#endif
#endif
