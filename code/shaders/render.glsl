// The bindless descriptor set from handrail's render.h. Requires
// GL_EXT_nonuniform_qualifier to index it per instance.
// TODO: generate the GLSL mirrors of engine structs and limits from the C
// definitions in prebuild, so they can't drift

#ifndef handrail_render_glsl_INCLUDED
#define handrail_render_glsl_INCLUDED

// Must match render.h, including any override there
#ifndef RENDER_MAX_TEXTURES
#define RENDER_MAX_TEXTURES 1024
#endif
#ifndef RENDER_MAX_SAMPLERS
#define RENDER_MAX_SAMPLERS 16
#endif

layout(set = 0, binding = 0) uniform texture2D textures[RENDER_MAX_TEXTURES];
layout(set = 0, binding = 1) uniform sampler   samplers[RENDER_MAX_SAMPLERS];

#endif
