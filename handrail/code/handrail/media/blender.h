#ifndef handrail_blender_h_INCLUDED
#define handrail_blender_h_INCLUDED

char blender_export_obj_cmd[] = 
    "blender --background --python build/obj_tmp.py -- %blend_path %obj_path";

char blender_export_bmp_cmd[] = 
    "blender --background --python build/bmp_tmp.py -- %blend_path %bmp_path %img_name";

char blender_export_obj_python[] = 
    "import bpy, sys, pathlib\n"
    "blend_file, obj_file = sys.argv[-2:]\n"
    "bpy.ops.wm.open_mainfile(filepath=blend_file)\n"
    "mesh = bpy.data.objects[\"mesh\"]\n"
    "mesh.select_set(True)\n"
    "bpy.context.view_layer.objects.active = mesh\n"
    "bpy.ops.wm.obj_export(\n"
        "filepath = obj_file,\n"
        "apply_modifiers = True,\n"
        "apply_transform = False,\n"
        "export_selected_objects = True,\n"
        "export_uv = True,\n"
        "export_normals = True,\n"
        "export_materials = False,\n"
        "export_triangulated_mesh = True)\n";

char blender_export_bmp_python[] = 
    "import bpy, sys, pathlib\n"
    "blend_file, bmp_file, img_name = sys.argv[-3:]\n"
    "bpy.ops.wm.open_mainfile(filepath=blend_file)\n"
    "workdir = pathlib.Path().resolve()\n"
    "filepath = str(workdir / bmp_file)\n"
    "img = bpy.data.images[img_name]\n"
    "scene = bpy.context.scene\n"
    "original_format = scene.render.image_settings.file_format\n"
    "scene.render.image_settings.file_format = 'BMP'\n"
    "img.save_render(filepath, scene=scene)\n"
    "scene.render.image_settings.file_format = original_format\n";

void blender_export_obj(String blend_path, String obj_path);
void blender_export_bmp(String blend_path, String bmp_path, String img_name);

#ifdef CSM_IMPLEMENTATION

void blender_export_obj(String blend_path, String obj_path) {
    File py_file = file_open(string_const("build/obj_tmp.py"), FILE_OPEN_WRITE);
    file_write(&py_file, blender_export_obj_python, sizeof(blender_export_obj_python) - 1);
    file_close(&py_file);

    String blender_cmd = string_init((char[4096]){}, 4096);
    string_cat(&blender_cmd, string_const(blender_export_obj_cmd));
    string_replace_substring(&blender_cmd, string_const("%blend_path"), blend_path);
    string_replace_substring(&blender_cmd, string_const("%obj_path"), obj_path);
    string_write_null_terminator(&blender_cmd);
    system(blender_cmd.text);
}

void blender_export_bmp(String blend_path, String bmp_path, String img_name) {
    File py_file = file_open(string_const("build/bmp_tmp.py"), FILE_OPEN_WRITE);
    file_write(&py_file, blender_export_bmp_python, sizeof(blender_export_bmp_python) - 1);
    file_close(&py_file);
     
    String blender_cmd = string_init((char[4096]){}, 4096);
    string_cat(&blender_cmd, string_const(blender_export_bmp_cmd));
    string_replace_substring(&blender_cmd, string_const("%blend_path"), blend_path);
    string_replace_substring(&blender_cmd, string_const("%bmp_path"), bmp_path);
    string_replace_substring(&blender_cmd, string_const("%img_name"), img_name);
    string_write_null_terminator(&blender_cmd);
    system(blender_cmd.text);
}

#endif
#endif
