#ifndef handrail_mesh_h_INCLUDED
#define handrail_mesh_h_INCLUDED

typedef struct {
    v2 position;
} MeshPrimitiveVertex2d;

typedef struct {
    v3 position;
} MeshPrimitiveVertex3d;

typedef struct {
    union {
        struct {
            v3 position;
            v3 normal;
            v2 uv;
        };
        f32 data[8];
    };
} MeshVertexData;

typedef struct {
    u64            vertices_len;
    MeshVertexData vertices[];
} MeshData;

typedef struct {
    u64 vertices_len;
    v2  vertices[];
} Primitive2dData;

// Pushed MESH asset. The vertices live in the MESH_VERTICES region, which the
// renderer uploads whole; vertices_offset is in bytes from that region's start.
typedef struct {
    u32 vertices_len;
    u32 vertices_offset;
} MeshAsset;

// Pushed PRIMITIVE_2D asset, laid out like MeshAsset over PRIMITIVE_2D_VERTICES (v2s).
typedef struct {
    u32 vertices_len;
    u32 vertices_offset;
} Primitive2dAsset;

MeshData*        mesh_from_obj(File* file, Stack* stack);
u64              mesh_size_from_vertices_len(u64 vertices_len);
u64              mesh_size(MeshData* data);
// A box centered on the origin, flat shaded, 36 vertices. Each face's uvs span
// the whole texture.
MeshData*        mesh_box(v3 size, Stack* stack);
// Push a MESH header plus its vertices. Returns the MESH handle.
u64              mesh_push_asset(AssetBuilder* builder, String tag, MeshData* mesh);

Primitive2dData* primitive_2d_from_data(void* data, u64 vertices_len, Stack* stack);
u64              primitive_2d_size_from_info(u64 vertices_len);
u64              primitive_2d_size(Primitive2dData* primitive);

#define PRIMITIVE_2D_QUAD_VERTICES_LEN 6
f32 primitive_2d_quad_vertices[PRIMITIVE_2D_QUAD_VERTICES_LEN * 2] = {
     1.0,  1.0,
     1.0, -1.0,
    -1.0,  1.0,

     1.0, -1.0,
    -1.0, -1.0,
    -1.0,  1.0
};

#endif

#if defined(HANDRAIL_IMPLEMENTATION_PASS) && !defined(handrail_mesh_h_IMPLEMENTED)
#define handrail_mesh_h_IMPLEMENTED

typedef struct {
    union {
        struct {
            i64 position;
            i64 uv;
            i64 normal;
        };
        i64 data[3];
    };
} _MeshObjFaceVertex;

typedef enum {
    MESH_OBJ_KEY_NONE,
    MESH_OBJ_KEY_V,
    MESH_OBJ_KEY_VT,
    MESH_OBJ_KEY_VN,
    MESH_OBJ_KEY_F
} _MeshObjKey;

_MeshObjKey _mesh_read_key(File* file) {
    String token = string_init((char[4096]){}, 4096);
    file_read_string_token(file, &token, ' ');
    if(string_equals(token, string_const("v"))) {
        return MESH_OBJ_KEY_V;
    } else if(string_equals(token, string_const("vt"))) {
        return MESH_OBJ_KEY_VT;
    } else if(string_equals(token, string_const("vn"))) {
        return MESH_OBJ_KEY_VN;
    } else if(string_equals(token, string_const("f"))) {
        return MESH_OBJ_KEY_F;
    }
    return MESH_OBJ_KEY_NONE;
}

void _mesh_read_float_vector(File* file, f32* v, i32 v_len) {
    for(i32 i = 0; i < v_len; i++) {
        v[i] = file_read_float_token(file, ' ');
    }
}

MeshData* mesh_from_obj(File* file, Stack* stack) {
    // Count line types
    i64 v_count  = 0;
    i64 vt_count = 0;
    i64 vn_count = 0;
    i64 f_count = 0;
    i64 obj_i = 0;

    while(file_at_end(file) == false) {
        String token = string_init((char[4096]){}, 4096);
        file_read_string_token(file, &token, ' ');
        if(string_equals(token, string_const("v"))) {
            v_count++;
        } else if(string_equals(token, string_const("vt"))) {
            vt_count++;
        } else if(string_equals(token, string_const("vn"))) {
            vn_count++;
        } else if(string_equals(token, string_const("f"))) {
            f_count++;
        }
        file_read_line(file, NULL);
    }

    // Allocate line data buffers
    u64 vert_bytes = v_count  * sizeof(v3);
    u64 uv_bytes   = vt_count * sizeof(v2);
    u64 norm_bytes = vn_count * sizeof(v3);
    u64 face_bytes = f_count  * 3 * sizeof(_MeshObjFaceVertex);

    v3* verts = (v3*)(stack_alloc(stack, vert_bytes));
    i64 verts_len = 0;
    v2* uvs = (v2*)(stack_alloc(stack, uv_bytes));
    i64 uvs_len = 0;
    v3* norms = (v3*)(stack_alloc(stack, norm_bytes));
    i64 norms_len = 0;
    _MeshObjFaceVertex* face_verts = (_MeshObjFaceVertex*)(stack_alloc(stack, face_bytes));
    i64 face_verts_len = 0;

    // Read/store line data
    file_seek_start(file);
    while(file_at_end(file) == false) {
        String token = string_init((char[4096]){}, 4096);
        file_read_string_token(file, &token, ' ');
        if(string_equals(token, string_const("v"))) {
            _mesh_read_float_vector(file, verts[verts_len].comps, 3);
            verts_len++;
        } else if(string_equals(token, string_const("vt"))) {
            v2* uv = &uvs[uvs_len];
            _mesh_read_float_vector(file, uv->comps, 2);
            //uv->y = 1.0f - uv->y;
            uvs_len++;
        } else if(string_equals(token, string_const("vn"))) {
            _mesh_read_float_vector(file, norms[norms_len].comps, 3);
            norms_len++;
        } else if(string_equals(token, string_const("f"))) {
            String line = string_init((char[4096]){}, 4096);
            file_read_line(file, &line);
            string_replace_char(&line, '/', ' ');
            StringReader reader = string_reader_init(&line);
            for(i64 i = 0; i < 3; i++) {
                _MeshObjFaceVertex* face_vert = &face_verts[face_verts_len];
                for(i64 j = 0; j < 3; j++) {
                    face_vert->data[j] = string_read_int_token(&reader, ' ');
                }
                face_verts_len++;
            }
            string_write_null_terminator(&line);
        } else {
            file_read_line(file, NULL);
        }
    }

    // Populate mesh data
    MeshData* mesh = (MeshData*)stack_alloc(stack, mesh_size_from_vertices_len(face_verts_len));
    mesh->vertices_len = face_verts_len; // = verts_len in indexed case
    // indices_len would go here
     
    for(i64 i = 0; i < face_verts_len; i++) {
        _MeshObjFaceVertex face_vert = face_verts[i];
        // face line indices are 0 indexed for some reason, so we sub 1.
        i64 vert_index = face_vert.position - 1;
        i64 uv_index   = face_vert.uv - 1;
        i64 norm_index = face_vert.normal - 1;

        // For flat shaded, not indexed
        MeshVertexData* mesh_vert = &mesh->vertices[i];
        mesh_vert->position = verts[vert_index];
        mesh_vert->uv = uvs[uv_index];
        mesh_vert->normal = norms[norm_index];
    }
    return mesh;
}

u64 mesh_size_from_vertices_len(u64 vertices_len) {
    return sizeof(u64) + vertices_len * sizeof(MeshVertexData);
}

u64 mesh_size(MeshData* mesh) {
    return mesh_size_from_vertices_len(mesh->vertices_len);
}

MeshData* mesh_box(v3 size, Stack* stack) {
    // Each face spans axes u and v with u x v = normal, so its triangles wind
    // counter-clockwise seen from outside
    v3 face_normals[6] = { {{{ 1, 0, 0}}}, {{{-1, 0, 0}}}, {{{ 0, 1, 0}}}, {{{ 0,-1, 0}}}, {{{ 0, 0, 1}}}, {{{ 0, 0,-1}}} };
    v3 face_us[6]      = { {{{ 0, 1, 0}}}, {{{ 0, 0, 1}}}, {{{ 0, 0, 1}}}, {{{ 1, 0, 0}}}, {{{ 1, 0, 0}}}, {{{ 0, 1, 0}}} };
    v3 face_vs[6]      = { {{{ 0, 0, 1}}}, {{{ 0, 1, 0}}}, {{{ 1, 0, 0}}}, {{{ 0, 0, 1}}}, {{{ 0, 1, 0}}}, {{{ 1, 0, 0}}} };
    v2 face_corners[6] = { {{{0, 0}}}, {{{1, 0}}}, {{{1, 1}}}, {{{0, 0}}}, {{{1, 1}}}, {{{0, 1}}} };
    MeshData* box = (MeshData*)stack_alloc(stack, mesh_size_from_vertices_len(36));
    box->vertices_len = 36;
    for(i32 face = 0; face < 6; face++) {
        for(i32 corner = 0; corner < 6; corner++) {
            MeshVertexData* vertex = &box->vertices[face * 6 + corner];
            v2 c = face_corners[corner];
            v3 unit = v3_add(v3_scale(face_normals[face], 0.5f),
                      v3_add(v3_scale(face_us[face], c.x - 0.5f), v3_scale(face_vs[face], c.y - 0.5f)));
            vertex->position = v3_new(unit.x * size.x, unit.y * size.y, unit.z * size.z);
            vertex->normal = face_normals[face];
            vertex->uv = c;
        }
    }
    return box;
}

u64 mesh_push_asset(AssetBuilder* builder, String tag, MeshData* mesh) {
    u64 vertices_offset = 0;
    asset_builder_push_asset(builder, tag, string_const("MESH_VERTICES"), string_const("MeshVertexData"),
        mesh->vertices, mesh->vertices_len * sizeof(MeshVertexData), &vertices_offset);

    MeshAsset asset = {};
    asset.vertices_len    = (u32)mesh->vertices_len;
    asset.vertices_offset = (u32)vertices_offset;
    u64 handle = asset_builder_next_handle_of_type(builder, string_const("MESH"));
    asset_builder_push_asset(builder, tag, string_const("MESH"), string_const("MeshAsset"), &asset, sizeof(MeshAsset), NULL);
    return handle;
}

Primitive2dData* primitive_2d_from_data(void* data, u64 vertices_len, Stack* stack) {
    Primitive2dData* prim = (Primitive2dData*)stack_alloc(stack, primitive_2d_size_from_info(vertices_len));
    prim->vertices_len = vertices_len;
    memcpy(prim->vertices, data, vertices_len * sizeof(v2));
    return prim;
}

u64 primitive_2d_size_from_info(u64 vertices_len) {
    return sizeof(u64) + vertices_len * sizeof(v2);
}

u64 primitive_2d_size(Primitive2dData* primitive) {
    return primitive_2d_size_from_info(primitive->vertices_len);
}

#endif
