#ifndef handrail_dynamic_library_h_INCLUDED
#define handrail_dynamic_library_h_INCLUDED

typedef struct {
    void*  handle;
    String path;
    u64    last_modified;
} DynamicLibrary;

void  dynamic_library_init(DynamicLibrary* lib, String path);
bool  dynamic_lib_update(DynamicLibrary* lib);
void* dynamic_lib_load_function(DynamicLibrary lib, String name);

#ifdef CSM_IMPLEMENTATION

void dynamic_lib_init(DynamicLibrary* lib, String path) {
    lib->handle = NULL;
    lib->path = path;
    lib->last_modified = 0;
}

bool dynamic_lib_update(DynamicLibrary* lib) {
	char* path_buf = (char*)alloca(lib->path.len + 1);
    String path_cstr = string_init(path_buf, lib->path.len + 1);
    string_cat(&path_cstr, lib->path);
    string_write_null_terminator(&path_cstr);

	u64 actual_last_modified = file_path_last_modified(path_cstr);
	if(actual_last_modified > lib->last_modified) {
		if(lib->last_modified != 0) {
#if PLATFORM == PLATFORM_LINUX
			dlclose(lib->handle);
#elif PLATFORM == PLATFORM_WINDOWS
			// NOW: Implement windows
#elif PLATFORM == PLATFORM_WEB
			// TODO: Implement web
#endif
		}
		lib->last_modified = actual_last_modified;

		// Copy filename to earlier version
		// tmp filename format is: library_name0000.so, where 0000 is the current
		// time displayed as an integer.
		time_t cur_time;
		time(&cur_time);
		char time_buf[64] = {};
		String time_str = string_init(time_buf, 64);
		string_print_int(&time_str, cur_time);

		char* copy_buf = (char*)alloca(lib->path.len + 64 + 1);
		String copy_cstr = string_init(&copy_buf, lib->path.len + 64 + 1);
		string_cat(&copy_cstr, string_const("./"));
		string_cat(&copy_cstr, lib->path);
		string_replace_substring(&copy_cstr, string_const(".so"), time_str);
        string_cat(&copy_cstr, string_const(".so"));
        string_write_null_terminator(&copy_cstr);

		char cmd[512];
		sprintf(cmd, "cp %s %s", path_cstr.text, copy_cstr.text);
		system(cmd);

#if PLATFORM == PLATFORM_LINUX
		lib->handle = dlopen(copy_cstr.text, RTLD_NOW);
		char* err;
		if((err = dlerror()) != NULL) {
			fprintf(stderr, "dlerror: %s\n", err);
		}
		assert(lib->handle != NULL);
		return true;
#elif PLATFORM == PLATFORM_WINDOWS
		// NOW: Implement windows
#elif PLATFORM == PLATFORM_WEB
		// TODO: Implement web
#endif
	}
	return false;
}

void* dynamic_lib_load_function(DynamicLibrary lib, String name) {
	char* buffer = (char*)alloca(name.len + 1);
    String cstr = string_init(buffer, name.len + 1);
    string_cat(&cstr, name);
    string_write_null_terminator(&cstr);

#if PLATFORM == PLATFORM_LINUX
	void* ptr = dlsym(lib.handle, cstr.text);
	char* err;
	if((err = dlerror()) != NULL) {
		printf("dlerror: %s\n", err);
		panic();
	}
	return ptr;
#elif PLATFORM == PLATFORM_WINDOWS
	// NOW: Implement windows
	return NULL;
#elif PLATFORM == PLATFORM_WEB
	// TODO: Implement web
#endif
}

#endif // CSM_IMPLEMENTATION
#endif // dynamic_library_h_INCLUDED
