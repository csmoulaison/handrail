#ifndef handrail_asset_pack_h_INCLUDED
#define handrail_asset_pack_h_INCLUDED

#ifndef ASSET_BUILDER_MAX_TYPES
#define ASSET_BUILDER_MAX_TYPES 64
#endif

#ifndef ASSET_BUILDER_MAX_ASSETS_PER_TYPE
#define ASSET_BUILDER_MAX_ASSETS_PER_TYPE 4096
#endif

typedef struct {
    String tag;
    u64    byte_index;
} Asset;

typedef struct {
    String type_name;
    String struct_name;
    Asset  assets[ASSET_BUILDER_MAX_ASSETS_PER_TYPE];
    u32    assets_len;
} AssetType;

typedef struct {
    Stack*    stack;
    AssetType types[ASSET_BUILDER_MAX_TYPES];
    u32       types_len;
} AssetBuilder;

void asset_builder_init(AssetBuilder* builder, Stack* stack);
void* asset_builder_push_asset(AssetBuilder* builder, String tag, String type_name, String struct_name, void* data, u64 size);
u64  asset_builder_next_handle_of_type(AssetBuilder* builder, String type_name);
void asset_builder_output_pack(AssetBuilder* builder, String path);
void asset_builder_output_source(AssetBuilder* builder, String handles_path, String data_path);

#ifdef CSM_IMPLEMENTATION

void asset_builder_init(AssetBuilder* builder, Stack* stack) {
    builder->stack = stack;
    builder->types_len = 0;
}

void* asset_builder_push_asset(AssetBuilder* builder, String tag, String type_name, String struct_name, void* data, u64 size) {
    AssetType* type = NULL;
    for(i32 i = 0; i < builder->types_len; i++) {
        if(string_equals(type_name, builder->types[i].type_name)) {
            type = &builder->types[i];
            break;
        }
    }

    if(type == NULL) {
        type = &builder->types[builder->types_len];
        type->type_name = type_name;
        type->struct_name = struct_name;
        assert(builder->types_len < ASSET_BUILDER_MAX_TYPES);
        builder->types_len++;
    }

    Asset* asset = &type->assets[type->assets_len];
    assert(type->assets_len < ASSET_BUILDER_MAX_ASSETS_PER_TYPE);
    type->assets_len++;
    asset->tag = tag;
    asset->byte_index = builder->stack->head;
    char* dst = stack_alloc(builder->stack, size);
    memcpy(dst, data, size);

    u32 align_mod = builder->stack->head % 16;
    if(align_mod != 0) {
        stack_alloc(builder->stack, 16 - align_mod);
    }

    return dst;
}

u64 asset_builder_next_handle_of_type(AssetBuilder* builder, String type_name) {
    u64 type_count = 0;
    for(i32 i = 0; i < builder->types_len; i++) {
        if(string_equals(type_name, builder->types[i].type_name)) {
            type_count++;
        }
    }
    // NOW: why is this needing iterate by 1? 
    return type_count + 1;
}

void asset_builder_output_pack(AssetBuilder* builder, String path) {
    File file = file_open(path, FILE_OPEN_WRITE);
    file_write(&file, builder->stack->memory, builder->stack->head);
    file_close(&file);
#if PLATFORM == PLATFORM_WINDOWS
	// NOW: Create assets.rc file
#endif
}

void asset_builder_output_source(AssetBuilder* builder, String handles_path, String data_path) {
    // Asset handles data file
    File file = file_open(handles_path, FILE_OPEN_WRITE);
    file_write_string(&file, string_const("// Pregenerated file. Any changes made will be erased on recompilation.\n\n"));

    // Asset handle defines
    for(i32 i = 0; i < builder->types_len; i++) {
        AssetType* type = &builder->types[i];
        file_write_string(&file, string_const("#define "));
        file_write_string(&file, type->type_name);
        file_write_string(&file, string_const("_COUNT "));
        file_print_int(&file, type->assets_len);
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
    }

    // Asset index arrays
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
            file_print_uint(&file, asset->byte_index);
            if(j != type->assets_len - 1) {
                file_write_string(&file, string_const(","));
            }
            file_write_string(&file, string_const("\n"));
        }
        file_write_string(&file, string_const("};\n\n"));

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
    file_write_string(&file, string_const("#ifdef __EMSCRIPTEN__\n\n"));
    file_write_string(&file, string_const("#define ASSET_PACK_SIZE "));
    file_print_uint(&file, builder->stack->head);
    file_write_string(&file, string_const("\nchar _binary_build_asset_pack_data_start["));
    file_print_uint(&file, builder->stack->head);
    file_write_string(&file, string_const("];\n"));
    file_write_string(&file, string_const("\n#else\n\n"));
    file_write_string(&file, string_const("extern char _binary_build_asset_pack_data_start[];\n"));
    file_write_string(&file, string_const("extern char _binary_build_asset_pack_data_end[];\n"));
    file_write_string(&file, string_const("extern char _binary_build_asset_pack_data_size[];\n"));
    file_write_string(&file, string_const("\n#endif\n"));
    file_write_string(&file, string_const("char* asset_pack_data = _binary_build_asset_pack_data_start;\n"));
    file_close(&file);
}

#endif
#endif
