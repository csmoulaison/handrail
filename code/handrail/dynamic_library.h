#ifndef handrail_dynamic_library_h_INCLUDED
#define handrail_dynamic_library_h_INCLUDED

typedef struct {
    void*  handle;
    String path;
    u64    last_modified;
    // Times the library has been loaded, including the first load
    u32    load_count;
} DynamicLibrary;

void  dynamic_library_init(DynamicLibrary* lib, String path);
bool  dynamic_library_update(DynamicLibrary* lib);
void* dynamic_library_load_function(DynamicLibrary lib, String name);

#endif

#if defined(HANDRAIL_IMPLEMENTATION_PASS) && !defined(handrail_dynamic_library_h_IMPLEMENTED)
#define handrail_dynamic_library_h_IMPLEMENTED

void dynamic_library_init(DynamicLibrary* lib, String path) {
    lib->handle = NULL;
    lib->path = path;
    lib->last_modified = 0;
    lib->load_count = 0;
}

bool dynamic_library_update(DynamicLibrary* lib) {
    char* path_buf = (char*)alloca(lib->path.len + 1);
    String path_cstr = string_init(path_buf, lib->path.len + 1);
    string_cat(&path_cstr, lib->path);
    string_write_null_terminator(&path_cstr);

    u64 actual_last_modified = file_path_last_modified(path_cstr);
    if(actual_last_modified > lib->last_modified) {
        // The build may still be writing the library. Leave last_modified alone
        // so the next update tries again.
        if(file_path_size(lib->path) == 0) {
            log_print(LOG_HOT_RELOAD, STRING_FMT " is empty, likely still being written. Retrying next update",
                      STRING_ARG(lib->path));
            return false;
        }
        log_print(LOG_HOT_RELOAD, "Change detected in " STRING_FMT " (modified %" PRIu64 " -> %" PRIu64 ")",
                  STRING_ARG(lib->path), lib->last_modified, actual_last_modified);
        if(lib->last_modified != 0) {
#if PLATFORM == PLATFORM_LINUX
            dlclose(lib->handle);
#elif PLATFORM == PLATFORM_WINDOWS
            FreeLibrary((HMODULE)lib->handle);
#elif PLATFORM == PLATFORM_WEB
            // TODO: Implement web
#endif
        }
        lib->last_modified = actual_last_modified;

        // Copy the library to a timestamped name and load that, so the build
        // can overwrite the original while it is loaded. The copy is named
        // library_name0000.ext, where 0000 is the current time as an integer.
        time_t cur_time;
        time(&cur_time);
        char time_buf[64] = {};
        String time_str = string_init(time_buf, 64);
        string_print_int(&time_str, cur_time);

        u64 extension_start = lib->path.len;
        for(u64 i = 0; i < lib->path.len; i++) {
            if(lib->path.text[i] == '.') extension_start = i;
        }
        char* copy_buf = (char*)alloca(lib->path.len + 64 + 3);
        String copy_cstr = string_init(copy_buf, lib->path.len + 64 + 3);
        string_cat(&copy_cstr, string_const("./"));
        string_cat(&copy_cstr, (String){ .text = lib->path.text, .len = extension_start, .capacity = extension_start });
        string_cat(&copy_cstr, time_str);
        string_cat(&copy_cstr, (String){ .text = lib->path.text + extension_start, .len = lib->path.len - extension_start, .capacity = lib->path.len - extension_start });
        string_write_null_terminator(&copy_cstr);

#if PLATFORM == PLATFORM_LINUX
        char cmd[512];
        sprintf(cmd, "cp %s %s", path_cstr.text, copy_cstr.text);
        if(system(cmd) != 0) {
            log_exit("Couldn't copy %s to %s", path_cstr.text, copy_cstr.text);
        }

        lib->handle = dlopen(copy_cstr.text, RTLD_NOW);
        if(lib->handle == NULL) {
            log_exit("Couldn't load %s: %s", copy_cstr.text, dlerror());
        }
        lib->load_count++;
        log_print(LOG_HOT_RELOAD, "Loaded copy %s (load %u)", copy_cstr.text, lib->load_count);
        return true;
#elif PLATFORM == PLATFORM_WINDOWS
        if(!CopyFileA(path_cstr.text, copy_cstr.text, FALSE)) {
            log_exit("Couldn't copy %s to %s (error %lu)", path_cstr.text, copy_cstr.text, GetLastError());
        }
        lib->handle = LoadLibraryA(copy_cstr.text);
        if(lib->handle == NULL) {
            log_exit("Couldn't load %s (error %lu)", copy_cstr.text, GetLastError());
        }
        lib->load_count++;
        log_print(LOG_HOT_RELOAD, "Loaded copy %s (load %u)", copy_cstr.text, lib->load_count);
        return true;
#elif PLATFORM == PLATFORM_WEB
        // TODO: Implement web
#endif
    }
    return false;
}

void* dynamic_library_load_function(DynamicLibrary lib, String name) {
    char* buffer = (char*)alloca(name.len + 1);
    String cstr = string_init(buffer, name.len + 1);
    string_cat(&cstr, name);
    string_write_null_terminator(&cstr);

#if PLATFORM == PLATFORM_LINUX
    void* ptr = dlsym(lib.handle, cstr.text);
    char* err;
    if((err = dlerror()) != NULL) {
        log_exit("Couldn't load function %s: %s", cstr.text, err);
    }
    return ptr;
#elif PLATFORM == PLATFORM_WINDOWS
    void* ptr = (void*)GetProcAddress((HMODULE)lib.handle, cstr.text);
    if(ptr == NULL) {
        log_exit("Couldn't load function %s (error %lu)", cstr.text, GetLastError());
    }
    return ptr;
#elif PLATFORM == PLATFORM_WEB
    // TODO: Implement web
#endif
}

#endif
