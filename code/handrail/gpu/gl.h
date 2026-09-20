#ifndef handrail_gl_h_INCLUDED
#define handrail_gl_h_INCLUDED

#if PLATFORM == PLATFORM_WINDOWS || PLATFORM == PLATFORM_LINUX
#include "GL/gl3w.h"
#endif

#include <GL/gl.h>

typedef struct {
	u32 id;
} GlProgram;

typedef struct {
	u32 vao;
	u32 vertices_len;
} GlVertexObject;

typedef struct {
	u32 id;
} GlTexture;

typedef struct {
	u32 id;
	u32 size;
	u32 binding;
} GlUbo;

typedef struct {
	u32 id;
	u32 size;
	u32 binding;
} GlSsbo;

typedef struct {
	u32 id;
	u32 texture_id;
	iv2 size;
} GlFramebuffer;

void           gl_init_start();
void           gl_init_end();
GlProgram      gl_create_program(char* vert_src, u32 vert_len, char* frag_src, u32 frag_len);
GlProgram      gl_create_program_from_files(char* vert_fname, char* frag_fname);
GlVertexObject gl_create_vertex_object(u32* vert_attrib_sizes, u32 vert_attrib_sizes_len, u32 verts_len, f32* vert_data);
GlTexture      gl_create_texture(u32 width, u32 height, u8 channel_count, u8* pixels, i32 wrap_param, i32 min_filter_param, i32 max_filter_param, bool generate_mipmap);
GlUbo          gl_create_ubo(u64 size, u64 binding);
GlSsbo         gl_create_ssbo(u64 size, u64 binding);
GlFramebuffer  gl_create_framebuffer(iv2 size);
void           gl_clear_color(v4 clear_color);
void           gl_clear_color_and_depth(v4 clear_color);
void           gl_viewport(i32 x, i32 y, i32 w, i32 h);

#ifdef CSM_IMPLEMENTATION

// PRIVATE FUNCTIONS
void gl_init_buffer(u32* id, u64 size, GLenum target, GLenum usage) {
	glGenBuffers(1, id);
	glBindBuffer(target, *id);
	glBufferData(target, size, NULL, usage);
	glBindBuffer(target, 0);
}

void gl_get_file_info(char* fname, char** out_src, u32* out_src_len) {
	FILE* file = fopen(fname, "r");
	assert(file != NULL);
	fseek(file, 0, SEEK_END);
	*out_src_len = ftell(file);
	fseek(file, 0, SEEK_SET);
	*out_src = malloc(*out_src_len + 1);
	fread(*out_src, *out_src_len, 1, file);
	fclose(file);
}

u32 gl_compile_shader(const char* src, i32 src_len, GLenum type) {
#ifdef __EMSCRIPTEN__
    String src_string = string_init((void*)src, src_len+1);
    src_string.len = src_len;
    string_replace_substring(&src_string, string_const("430 core"), string_const("300 es"));
    string_write_null_terminator(&src_string);
    printf("%s\n", src_string.text);
#endif

	u32 shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, &src_len);
	glCompileShader(shader);

	i32 success;
	char info[256];
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if(success == false) {
		glGetShaderInfoLog(shader, 256, NULL, info);
		printf(info);
		panic();
	}
	return shader;
}


// PUBLIC INIT_FUNCTIONS

void gl_init_start() {
#ifndef __EMSCRIPTEN__
	assert(gl3wInit() == 0);
#endif
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// NOTO: depth as parameter
	//glEnable(GL_DEPTH_TEST);
}

void gl_init_end() {
}

GlProgram gl_create_program(char* vert_src, u32 vert_len, char* frag_src, u32 frag_len) {
	i32 program = glCreateProgram();
	u32 vert = gl_compile_shader(vert_src, vert_len, GL_VERTEX_SHADER);
	u32 frag = gl_compile_shader(frag_src, frag_len, GL_FRAGMENT_SHADER);
	glAttachShader(program, vert);
	glAttachShader(program, frag);
	glLinkProgram(program);

    i32 success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if(!success) {
        i32 len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        char* log = (char*)alloca(len);
        glGetProgramInfoLog(program, len, &len, log);
        printf("Shader program linking failed: %s\n", log);
    }

	glDeleteShader(vert);
	glDeleteShader(frag);
	return (GlProgram){ .id = program };
}

// WARNING: calls malloc for src buffer
GlProgram gl_create_program_from_files(char* vert_fname, char* frag_fname) {
	char* vert_src = NULL;
	char* frag_src = NULL;
	u32 vert_len;
	u32 frag_len;
	gl_get_file_info(vert_fname, &vert_src, &vert_len);
	gl_get_file_info(frag_fname, &frag_src, &frag_len);
	GlProgram program = gl_create_program(vert_src, vert_len, frag_src, frag_len);
	free(vert_src);
	free(frag_src);
	return program;
}

GlVertexObject gl_create_vertex_object(u32* vert_attrib_sizes, u32 vert_attribs_len, u32 verts_len, f32* vert_data) {
	GlVertexObject obj = {};
	obj.vertices_len = verts_len;

	glGenVertexArrays(1, &obj.vao);
	glBindVertexArray(obj.vao);
	u32 vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	u32 vertex_size = 0;
	for(i32 attr_index = 0; attr_index < vert_attribs_len; attr_index++) {
		vertex_size += vert_attrib_sizes[attr_index];
	}

	u32 cur_offset = 0;
	for(i32 attr_index = 0; attr_index < vert_attribs_len; attr_index++) {
		u32 attribute_size = vert_attrib_sizes[attr_index];
		glVertexAttribPointer(
    		attr_index, 
    		attribute_size, 
    		GL_FLOAT, 
    		GL_FALSE, 
    		sizeof(f32) * vertex_size, 
    		(void*)(cur_offset * sizeof(f32))); 
		glEnableVertexAttribArray(attr_index);
		cur_offset += attribute_size;
	}

    u64 buf_size = sizeof(f32) * verts_len * vertex_size;
	glBufferData(GL_ARRAY_BUFFER, buf_size, vert_data, GL_STATIC_DRAW);
	return obj;
}

GlTexture gl_create_texture(
	u32 width, 
	u32 height, 
	u8  bytes_per_pixel, 
	u8* pixel_data, 
	i32 wrap_param, 
	i32 min_filter_param, 
	i32 max_filter_param, 
	bool generate_mipmap
) {
	GlTexture texture = {};
	glGenTextures(1, &texture.id);
	glBindTexture(GL_TEXTURE_2D, texture.id);

	i32 format;
	i32 internal_format;
	if(bytes_per_pixel == 1) {
		format = GL_RED;
		internal_format = GL_R8;
	} else if(bytes_per_pixel == 4) {
		format = GL_RGBA;
		internal_format = GL_RGBA8;
	} else {
		printf("csm_gl: bytes per pixel %u in texture data not supported.\n", bytes_per_pixel);
		panic();
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_param);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_param);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter_param);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, max_filter_param);
	glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, GL_UNSIGNED_BYTE, pixel_data);
	if(generate_mipmap) {
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	return texture;
}

GlUbo gl_create_ubo(u64 size, u64 binding) {
	GlUbo ubo = {};
	ubo.binding = binding;
	ubo.size = size;
	gl_init_buffer(&ubo.id, size, GL_UNIFORM_BUFFER, GL_DYNAMIC_DRAW);
	return ubo;
}

GlSsbo gl_create_ssbo(u64 size, u64 binding) {
	GlSsbo ssbo = {};
	ssbo.binding = binding;
	ssbo.size = size;
	gl_init_buffer(&ssbo.id, size, GL_SHADER_STORAGE_BUFFER, GL_DYNAMIC_DRAW);
	return ssbo;
}

GlFramebuffer gl_create_framebuffer(iv2 size) {
    GlFramebuffer framebuffer = {};
    glGenFramebuffers(1, &framebuffer.id);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.id);

    glGenTextures(1, &framebuffer.texture_id);
    glBindTexture(GL_TEXTURE_2D, framebuffer.texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size.x, size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebuffer.texture_id, 0);

    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    framebuffer.size = size;
    return framebuffer;
}


// PUBLIC UPDATE FUNCTIONS
 
void gl_clear_color(v4 clear_color) {
	glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
	glClear(GL_COLOR_BUFFER_BIT);
}

void gl_clear_color_and_depth(v4 clear_color) {
	glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

#endif
#endif
