// MeshVertexData from handrail's media/mesh.h, and the MESH_VERTICES region.
// Requires GL_EXT_buffer_reference and GL_EXT_scalar_block_layout.
// TODO: generate the GLSL mirrors of engine structs and limits from the C
// definitions in prebuild, so they can't drift

#ifndef handrail_mesh_glsl_INCLUDED
#define handrail_mesh_glsl_INCLUDED

struct MeshVertex {
    vec3 position;
    vec3 normal;
    vec2 uv;
};

layout(buffer_reference, scalar) readonly buffer MeshVertices {
    MeshVertex vertices[];
};

#endif
