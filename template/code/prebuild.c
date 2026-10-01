// The asset build step. The build script compiles this into its own program
// (bootstrap) and runs it before every static build. It packs every asset into
// one file, laid out by type, and writes code/generated/, which tells the game
// where each asset is: a handle per asset, like TEXTURE_CHECKER, and each
// type's region, like FONT_VECTOR_GLYPHS_REGION_OFFSET.
// See handrail/code/handrail/media/asset_builder.h.

#include "config.h"

// Fonts are read with FreeType, which only prebuild links. Without fonts,
// delete this define and the build skips FreeType entirely.
#define HANDRAIL_FONT_PROCESSING
#define HANDRAIL_IMPLEMENTATION
#include <handrail/core.h>

i32 main(i32 argc, char** argv) {
    // Allocate memory
    void* mem = malloc(MEGABYTE * 64);
    Stack root_stack = stack_from_memory(mem, MEGABYTE * 64, string_const("Root"));
    Stack scratch_stack = stack_from_stack(&root_stack, MEGABYTE * 32, string_const("Scratch"));
    Stack asset_builder_stack = stack_from_stack(&root_stack, MEGABYTE * 32, string_const("AssetBuilder"));

    AssetBuilder* asset_builder = (AssetBuilder*)stack_alloc(&scratch_stack, sizeof(AssetBuilder));
    asset_builder_init(asset_builder, &asset_builder_stack);

    // Shaders, compiled to SPIR-V. Each tag becomes a handle: SHADER_DRAW_VERT.
    shader_push_asset(asset_builder, string_const("DRAW_VERT"), string_const("code/shaders/draw.vert"), SHADER_STAGE_VERTEX, &scratch_stack);
    shader_push_asset(asset_builder, string_const("DRAW_FRAG"), string_const("code/shaders/draw.frag"), SHADER_STAGE_FRAGMENT, &scratch_stack);

    // Checkerboard texture, generated here so the project needs no image files
    u64 checker_size = 64;
    TextureData* checker = (TextureData*)stack_alloc(&scratch_stack, texture_size_from_dimensions(checker_size, checker_size, TEXTURE_FORMAT_RGBA));
    checker->width  = checker_size;
    checker->height = checker_size;
    checker->format = TEXTURE_FORMAT_RGBA;
    for(u64 y = 0; y < checker_size; y++) {
        for(u64 x = 0; x < checker_size; x++) {
            TexturePixel32* pixel = (TexturePixel32*)&checker->pixel_buffer[(y * checker_size + x) * 4];
            u8 value = ((x / 8 + y / 8) % 2) ? 0xFF : 0x40;
            pixel->components.r = value;
            pixel->components.g = value;
            pixel->components.b = value;
            pixel->components.a = 0xFF;
        }
    }
    texture_push_asset(asset_builder, string_const("CHECKER"), checker, &scratch_stack);

    // Font, stored as glyph outlines that the fragment shader draws at any size
    font_vector_push_asset(asset_builder, string_const("BODY"), string_const("assets/ttf/et-book-roman-line-figures.ttf"), &scratch_stack);

    // Write the pack and the code that indexes it
    asset_builder_output_source(asset_builder, string_const("code/generated/asset_handles.c"), string_const("code/generated/asset_data.c"));
#if PLATFORM == PLATFORM_WINDOWS
    asset_builder_output_pack(asset_builder, string_const("build/asset/pack.obj"));
#else
    asset_builder_output_pack(asset_builder, string_const("build/asset/pack.data"));
#endif
}
