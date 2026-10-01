#ifndef handrail_shader_h_INCLUDED
#define handrail_shader_h_INCLUDED

#ifndef SHADER_SPIRV_TMP_PATH
#define SHADER_SPIRV_TMP_PATH "build/shader.spv"
#endif

// Searched by #include after the including file's own directory, so game
// shaders can #include "font_vector.glsl" and the other handrail GLSL files.
// Relative to the directory prebuild runs in.
#ifndef SHADER_INCLUDE_PATH
#define SHADER_INCLUDE_PATH "handrail/code/shaders"
#endif

typedef enum {
    SHADER_STAGE_VERTEX,
    SHADER_STAGE_FRAGMENT
} ShaderStage;

// Pushed SHADER asset: SPIR-V for one stage.
typedef struct {
    u32 stage;
    u32 code_size; // In bytes
    u32 code[];
} ShaderData;

// Compile GLSL to SPIR-V with the Vulkan SDK's glslangValidator and push it as
// a SHADER asset. #include directives resolve relative to glsl_path, then to
// SHADER_INCLUDE_PATH. Returns
// the SHADER handle.
u64 shader_push_asset(AssetBuilder* builder, String tag, String glsl_path, ShaderStage stage, Stack* stack);

#endif

#if defined(HANDRAIL_IMPLEMENTATION_PASS) && !defined(handrail_shader_h_IMPLEMENTED)
#define handrail_shader_h_IMPLEMENTED

u64 shader_push_asset(AssetBuilder* builder, String tag, String glsl_path, ShaderStage stage, Stack* stack) {
    // Compile to a temporary SPIR-V file
    String cmd = string_from_stack(stack, 1024);
#if PLATFORM == PLATFORM_WINDOWS
    string_cat(&cmd, string_const("\"%VULKAN_SDK%\\Bin\\glslangValidator\""));
#elif PLATFORM == PLATFORM_LINUX
    string_cat(&cmd, string_const("glslangValidator"));
#elif PLATFORM == PLATFORM_WEB
    panic();
#endif
    string_cat(&cmd, string_const(" -V --quiet --target-env vulkan1.3 -S "));
    string_cat(&cmd, stage == SHADER_STAGE_VERTEX ? string_const("vert") : string_const("frag"));
    string_cat(&cmd, string_const(" -I\"" SHADER_INCLUDE_PATH "\" -o " SHADER_SPIRV_TMP_PATH " "));
    string_cat(&cmd, glsl_path);
    string_write_null_terminator(&cmd);
    log_print(LOG_ASSET, "Compiling shader: %s", cmd.text);
    i32 exit_code = system(cmd.text);
    if(exit_code != 0) {
        log_exit("Shader compilation failed with exit code %i: " STRING_FMT, exit_code, STRING_ARG(glsl_path));
    }

    // Read the SPIR-V back and push it
    File file = file_open(string_const(SHADER_SPIRV_TMP_PATH), FILE_OPEN_READ);
    u64 code_size = file_size(&file);
    assert(code_size % 4 == 0);
    ShaderData* shader = (ShaderData*)stack_alloc(stack, sizeof(ShaderData) + code_size);
    shader->stage = stage;
    shader->code_size = (u32)code_size;
    file_read(&file, shader->code, code_size);
    file_close(&file);

    u64 handle = asset_builder_next_handle_of_type(builder, string_const("SHADER"));
    asset_builder_push_asset(builder, tag, string_const("SHADER"), string_const("ShaderData"), shader, sizeof(ShaderData) + code_size, NULL);
    return handle;
}

#endif
