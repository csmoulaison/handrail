#ifndef handrail_asset_pack_h_INCLUDED
#define handrail_asset_pack_h_INCLUDED

#ifndef ASSET_BUILDER_MAX_TYPES
#define ASSET_BUILDER_MAX_TYPES 64
#endif

#ifndef ASSET_BUILDER_MAX_ASSETS_PER_TYPE
#define ASSET_BUILDER_MAX_ASSETS_PER_TYPE 4096
#endif

// Every asset starts on this alignment, both within its region and in the pack.
#define ASSET_BUILDER_ALIGNMENT 16

// Symbol the pack is exported under when it is written as a COFF object.
#define ASSET_BUILDER_COFF_SYMBOL "assets"

// Assets are staged in push order, but the pack is laid out by type: every
// asset of a type sits in one contiguous region. That lets a renderer upload
// a whole type (all mesh vertices, say) with one copy.
typedef struct {
    String tag;
    u64    stack_offset;  // Where the staged data lives in the builder's stack
    u64    size;
    u64    region_offset; // Offset from the start of the asset's type region
} Asset;

typedef struct {
    String type_name;
    String struct_name;
    Asset  assets[ASSET_BUILDER_MAX_ASSETS_PER_TYPE];
    u32    assets_len;
    u64    region_size;
} AssetType;

typedef struct {
    Stack*    stack;
    AssetType types[ASSET_BUILDER_MAX_TYPES];
    u32       types_len;
} AssetBuilder;

void  asset_builder_init(AssetBuilder* builder, Stack* stack);
// Stage an asset. out_region_offset (nullable) receives its offset within its type region.
void* asset_builder_push_asset(AssetBuilder* builder, String tag, String type_name, String struct_name, void* data, u64 size, u64* out_region_offset);
// The handle the next pushed asset of this type will get.
u64   asset_builder_next_handle_of_type(AssetBuilder* builder, String type_name);
// Write the pack: raw bytes on Linux (linked with ld -b binary), a COFF object on Windows.
void  asset_builder_output_pack(AssetBuilder* builder, String path);
void  asset_builder_output_source(AssetBuilder* builder, String handles_path, String data_path);

#endif

#if defined(HANDRAIL_IMPLEMENTATION_PASS) && !defined(handrail_asset_pack_h_IMPLEMENTED)
#define handrail_asset_pack_h_IMPLEMENTED

void asset_builder_init(AssetBuilder* builder, Stack* stack) {
    builder->stack = stack;
    builder->types_len = 0;
}

void* asset_builder_push_asset(AssetBuilder* builder, String tag, String type_name, String struct_name, void* data, u64 size, u64* out_region_offset) {
    AssetType* type = NULL;
    for(i32 i = 0; i < builder->types_len; i++) {
        if(string_equals(type_name, builder->types[i].type_name)) {
            type = &builder->types[i];
            break;
        }
    }

    if(type == NULL) {
        assert(builder->types_len < ASSET_BUILDER_MAX_TYPES);
        type = &builder->types[builder->types_len];
        type->type_name = type_name;
        type->struct_name = struct_name;
        type->assets_len = 0;
        type->region_size = 0;
        builder->types_len++;
    }

    assert(type->assets_len < ASSET_BUILDER_MAX_ASSETS_PER_TYPE);
    Asset* asset = &type->assets[type->assets_len];
    type->assets_len++;

    // Stage the data, padded so the next asset stays aligned
    u64 padded_size = (size + ASSET_BUILDER_ALIGNMENT - 1) / ASSET_BUILDER_ALIGNMENT * ASSET_BUILDER_ALIGNMENT;
    asset->tag = tag;
    asset->stack_offset = builder->stack->head;
    asset->size = padded_size;
    asset->region_offset = type->region_size;
    type->region_size += padded_size;

    char* dst = stack_alloc(builder->stack, padded_size);
    memcpy(dst, data, size);
    memset(dst + size, 0, padded_size - size);

    if(out_region_offset != NULL) {
        *out_region_offset = asset->region_offset;
    }
    return dst;
}

u64 asset_builder_next_handle_of_type(AssetBuilder* builder, String type_name) {
    for(i32 i = 0; i < builder->types_len; i++) {
        if(string_equals(type_name, builder->types[i].type_name)) {
            return builder->types[i].assets_len;
        }
    }
    return 0;
}

void asset_builder_output_pack(AssetBuilder* builder, String path) {
    // Lay the staged assets out grouped by type
    u64 pack_size = 0;
    for(i32 i = 0; i < builder->types_len; i++) {
        pack_size += builder->types[i].region_size;
    }
    u8* pack = (u8*)stack_alloc(builder->stack, pack_size);
    u64 region_start = 0;
    for(i32 i = 0; i < builder->types_len; i++) {
        AssetType* type = &builder->types[i];
        for(i32 j = 0; j < type->assets_len; j++) {
            Asset* asset = &type->assets[j];
            memcpy(&pack[region_start + asset->region_offset], &builder->stack->memory[asset->stack_offset], asset->size);
        }
        region_start += type->region_size;
    }

    // Write it in the format the platform's linker embeds
#if PLATFORM == PLATFORM_WINDOWS
    coff_output_binary_object(pack, pack_size, path, string_const(ASSET_BUILDER_COFF_SYMBOL));
#else
    File file = file_open(path, FILE_OPEN_WRITE);
    file_write(&file, pack, pack_size);
    file_close(&file);
#endif
}

void asset_builder_output_source(AssetBuilder* builder, String handles_path, String data_path) {
    // Asset handles data file
    File file = file_open(handles_path, FILE_OPEN_WRITE);
    file_write_string(&file, string_const("// Pregenerated file. Any changes made will be erased on recompilation.\n\n"));

    // Asset handle and region defines
    u64 region_start = 0;
    u64 pack_size = 0;
    for(i32 i = 0; i < builder->types_len; i++) {
        AssetType* type = &builder->types[i];
        file_write_string(&file, string_const("#define "));
        file_write_string(&file, type->type_name);
        file_write_string(&file, string_const("_COUNT "));
        file_print_int(&file, type->assets_len);
        file_write_string(&file, string_const("\n#define "));
        file_write_string(&file, type->type_name);
        file_write_string(&file, string_const("_REGION_OFFSET "));
        file_print_uint(&file, region_start);
        file_write_string(&file, string_const("\n#define "));
        file_write_string(&file, type->type_name);
        file_write_string(&file, string_const("_REGION_SIZE "));
        file_print_uint(&file, type->region_size);
        file_write_string(&file, string_const("\n"));
        for(i32 j = 0; j < type->assets_len; j++) {
            Asset* asset = &type->assets[j];
            file_write_string(&file, string_const("#define "));
            file_write_string(&file, type->type_name);
            file_write_string(&file, string_const("_"));
            file_write_string(&file, asset->tag);
            file_write_string(&file, string_const(" "));
            file_print_uint(&file, j);
            file_write_string(&file, string_const("\n"));
        }
        file_write_string(&file, string_const("\n"));
        region_start += type->region_size;
    }
    pack_size = region_start;

    // Asset index arrays
    region_start = 0;
    for(i32 i = 0; i < builder->types_len; i++) {
        AssetType* type = &builder->types[i];
        char* buf = (char*)alloca(type->type_name.len);
        String lowercase_type = string_init(buf, type->type_name.len);
        string_cat(&lowercase_type, type->type_name);
        string_to_lower(&lowercase_type);

        file_write_string(&file, string_const("u64 "));
        file_write_string(&file, lowercase_type);
        file_write_string(&file, string_const("_data_offsets["));
        file_write_string(&file, type->type_name);
        file_write_string(&file, string_const("_COUNT] = {\n"));
        for(i32 j = 0; j < type->assets_len; j++) {
            Asset* asset = &type->assets[j];
            file_write_string(&file, string_const("    "));
            file_print_uint(&file, region_start + asset->region_offset);
            if(j != type->assets_len - 1) {
                file_write_string(&file, string_const(","));
            }
            file_write_string(&file, string_const("\n"));
        }
        file_write_string(&file, string_const("};\n\n"));
        region_start += type->region_size;

        // Getter function
        file_write_string(&file, type->struct_name);
        file_write_string(&file, string_const("* "));
        file_write_string(&file, lowercase_type);
        file_write_string(&file, string_const("_asset(char* pack, u64 handle) {\n"));
        // return (_Data*)&pack[__data_offsets[handle]];
        file_write_string(&file, string_const("    return ("));
        file_write_string(&file, type->struct_name);
        file_write_string(&file, string_const("*)&pack["));
        file_write_string(&file, lowercase_type);
        file_write_string(&file, string_const("_data_offsets[handle]];\n}\n\n"));
    }
    file_close(&file);

    // Asset pack data file
    file = file_open(data_path, FILE_OPEN_WRITE);
    file_write_string(&file, string_const("// Pregenerated file. Any changes made will be erased on recompilation.\n\n"));
    file_write_string(&file, string_const("#define ASSET_PACK_SIZE "));
    file_print_uint(&file, pack_size);
    file_write_string(&file, string_const("\n\n#if defined(__EMSCRIPTEN__)\n\n"));
    file_write_string(&file, string_const("char _binary_build_asset_pack_data_start[ASSET_PACK_SIZE];\n"));
    file_write_string(&file, string_const("char* asset_pack_data = _binary_build_asset_pack_data_start;\n"));
    file_write_string(&file, string_const("\n#elif defined(_MSC_VER)\n\n"));
    file_write_string(&file, string_const("extern char " ASSET_BUILDER_COFF_SYMBOL "[];\n"));
    file_write_string(&file, string_const("char* asset_pack_data = " ASSET_BUILDER_COFF_SYMBOL ";\n"));
    file_write_string(&file, string_const("\n#else\n\n"));
    file_write_string(&file, string_const("extern char _binary_build_asset_pack_data_start[];\n"));
    file_write_string(&file, string_const("char* asset_pack_data = _binary_build_asset_pack_data_start;\n"));
    file_write_string(&file, string_const("\n#endif\n"));
    file_close(&file);
}

#endif
