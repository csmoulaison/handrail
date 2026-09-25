#ifndef handrail_file_h_INCLUDED
#define handrail_file_h_INCLUDED

typedef struct {
    FILE* handle;
} File;

typedef enum {
    FILE_OPEN_READ,
    FILE_OPEN_WRITE,
    FILE_OPEN_READ_WRITE
} FileOpenMode;

File file_open(String fname, FileOpenMode mode);
bool file_try_open(String fname, FileOpenMode mode, File* out);
void file_close(File* file);
u64 file_size(File* file);
u64 file_last_modified(File* file);
u64 file_path_last_modified(String path);
String* file_names_in_directory(String path, i32* out_path_count, Stack* stack);
String* file_paths_in_directory(String path, i32* out_path_count, Stack* stack);

void file_write(File* file, void* data, u64 size);
void file_write_char(File* file, char c);
void file_write_string(File* file, String string);
void file_print_uint(File* file, u64 n);
void file_print_int(File* file, i64 n);
void file_print_float(File* file, f64 n);

u64  file_read(File* file, void* dst, u64 size);
u64  file_read_all(File* file, void* dst, u64 dst_size);
char file_read_char(File* file);
// dst can be NULL in these string related functions.
void file_read_line(File* file, String* dst);
u64  file_read_string_token(File* file, String* dst, char delimiter);
i64  file_read_int_token(File* file, char delimiter);
f64  file_read_float_token(File* file, char delimiter);

void file_seek(File* file, f64 offset);
void file_seek_from_start(File* file, f64 offset);
void file_seek_from_end(File* file, f64 offset);
void file_seek_start(File* file);
void file_seek_end(File* file);
bool file_at_end(File* file);
char file_peek_char(File* file);
void file_peek_string_token(File* file, String* dst, char delimiter);

#ifdef CSM_IMPLEMENTATION

File file_open(String fname, FileOpenMode mode) {
    File file;
    assert(file_try_open(fname, mode, &file));
    return file;
}

bool file_try_open(String fname, FileOpenMode mode, File* out) {
    FILE* handle;
    char* buf = alloca(fname.len + 1);
    String fname_cstring = string_init(buf, fname.len+1);
    string_cat(&fname_cstring, fname);
    string_write_null_terminator(&fname_cstring);
    switch(mode) {
        // NOW: yo, theres a binary and a text mode
        case FILE_OPEN_READ: {
            handle = fopen(fname_cstring.text, "rb");
        } break;
        case FILE_OPEN_WRITE: {
            handle = fopen(fname_cstring.text, "wb");
        } break;
        case FILE_OPEN_READ_WRITE: {
            handle = fopen(fname_cstring.text, "rwb");
        } break;
        default: {
            fprintf(stderr, "File open mode %i not valid.", mode);
            panic();
        } break;
    }
    if(handle == NULL) {
        return false;
    }
    out->handle = handle;
    return true;
}

void file_close(File* file) {
    fclose(file->handle);
}

u64 file_size(File* file) {
    file_seek_end(file);
    u64 size = ftell(file->handle);
    file_seek_start(file);
    return size;
}

u64 file_last_modified(File* file) {
    i32 descriptor = fileno(file->handle);
    struct stat stat;
    assert(fstat(descriptor, &stat) == 0);
    return (u64)stat.st_mtime;
}

u64 file_path_last_modified(String path) {
#if PLATFORM == PLATFORM_LINUX
	char* buf = alloca(path.len + 1);
    String cpath = string_init(buf, path.len + 1);
    string_cat(&cpath, path);
    string_write_null_terminator(&cpath);
    struct stat file_stat;
    stat(cpath.text, &file_stat);
    return file_stat.st_mtim.tv_sec;
#elif PLATFORM == PLATFORM_WINDOWS
	// NOW: Windows implementation
	return 0;
#elif PLATFORM == PLATFORM_WEB
	// TODO: Web implementation
#endif
}

// NOTO: reduntant two functions below.
String* file_names_in_directory(String path, i32* out_path_count, Stack* stack) {
#if PLATFORM == PLATFORM_LINUX
	char* buf = alloca(path.len + 1);
    String cpath = string_init(buf, path.len+1);
    string_cat(&cpath, path);
    string_write_null_terminator(&cpath);

    DIR *dir = opendir(cpath.text); 
    assert(dir != NULL);
    struct dirent *entity;
    *out_path_count = 0;
    while((entity = readdir(dir)) != NULL) {
        if(strcmp(entity->d_name, ".") != 0 && strcmp(entity->d_name, "..") != 0) {
            *out_path_count += 1;
        }
    }

    String* paths = (String*)stack_alloc(stack, *out_path_count * sizeof(String));
    *out_path_count = 0;
    rewinddir(dir);
    while((entity = readdir(dir)) != NULL) {
        if(strcmp(entity->d_name, ".") != 0 && strcmp(entity->d_name, "..") != 0) {
            paths[*out_path_count] = string_from_stack(stack, strlen(entity->d_name));
            string_cat(&paths[*out_path_count], string_const(entity->d_name));
            *out_path_count += 1;
        }
    }

    closedir(dir);
    return paths;
#elif PLATFORM == PLATFORM_WINDOWS
	return NULL;
	// NOW: Windows implementation
#elif PLATFORM == PLATFORM_WEB
	// TODO: Web implementation
#endif
}

String* file_paths_in_directory(String path, i32* out_path_count, Stack* stack) {
#if PLATFORM == PLATFORM_LINUX
	char* buf = alloca(path.len + 1);
    String cpath = string_init(buf, path.len+1);
    string_cat(&cpath, path);
    string_write_null_terminator(&cpath);

    DIR* dir = opendir(cpath.text); 
    assert(dir != NULL);
    struct dirent* entity;
    *out_path_count = 0;
    while((entity = readdir(dir)) != NULL) {
        if(strcmp(entity->d_name, ".") != 0 && strcmp(entity->d_name, "..") != 0) {
            *out_path_count += 1;
        }
    }

    String* paths = (String*)stack_alloc(stack, *out_path_count * sizeof(String));
    *out_path_count = 0;
    rewinddir(dir);
    while((entity = readdir(dir)) != NULL) {
        if(strcmp(entity->d_name, ".") != 0 && strcmp(entity->d_name, "..") != 0) {
            paths[*out_path_count] = string_from_stack(stack, path.len + strlen(entity->d_name));
            string_cat(&paths[*out_path_count], path);
            string_cat(&paths[*out_path_count], string_const(entity->d_name));
            *out_path_count += 1;
        }
    }

    closedir(dir);
    return paths;
#elif PLATFORM == PLATFORM_WINDOWS
	// NOW: Windows implementation
	return NULL;
#elif PLATFORM == PLATFORM_WEB
	// TODO: Web implementation
#endif
}

void file_write(File* file, void* data, u64 size) {
    fwrite(data, size, 1, file->handle);
}

void file_write_char(File* file, char c) {
    file_write(file, &c, 1);
}

void file_write_string(File* file, String string) {
    file_write(file, string.text, string.len);
}

void file_print_uint(File* file, u64 n) {
    fprintf(file->handle, "%" PRIu64, n);
}

void file_print_int(File* file, i64 n) {
    fprintf(file->handle, "%" PRId64, n);
}

void file_print_float(File* file, f64 n) {
    fprintf(file->handle, "%lf", n);
}

u64 file_read(File* file, void* dst, u64 size) {
    return fread(dst, size, 1, file->handle);
}

u64 file_read_all(File* file, void* dst, u64 dst_size) {
    u64 size = file_size(file);
    assert(size < dst_size);
    return file_read(file, dst, size);
}

char file_read_char(File* file) {
    return fgetc(file->handle);
}

void file_read_line(File* file, String* dst) {
    char c;
    while((c = file_read_char(file)) != '\n') {
        if(c == EOF) {
            return;
        }
        if(dst != NULL) {
            string_write_char(dst, c);
        }
    }
}

u64 file_read_string_token(File* file, String* dst, char delimiter) {
    u64 len = 0;
    char c = 0;
    while((c = fgetc(file->handle)) != EOF && c != '\n' && c != delimiter) {
        if(dst != NULL) {
            string_write_char(dst, c);
        }
        len++;
    }
    assert(len > 0);
    return len;
}

// NOTO: factor int/float conversions into string_to_* functions.
i64 file_read_int_token(File* file, char delimiter) {
    String tmp = string_init((char[256]){}, 256);
    file_read_string_token(file, &tmp, delimiter);
    string_write_null_terminator(&tmp);
    char* end;
    i64 n = strtol(tmp.text, &end, 10);
    errno = 0;
    if (end == tmp.text) {
        fprintf(stderr, "Could not read int token. No digits found.\n");
        panic();
    } else if (errno == ERANGE || n > INT_MAX || n < INT_MIN) {
        fprintf(stderr, "Could not read int token. Value out of range for an int.\n");
        panic();
    }
    return n;
}

f64 file_read_float_token(File* file, char delimiter) {
    String tmp = string_init((char[256]){}, 256);
    file_read_string_token(file, &tmp, delimiter);
    string_write_null_terminator(&tmp);
    char* end;
    f64 n = strtof(tmp.text, &end);
    if (end == tmp.text) {
        fprintf(stderr, "Could not read float token.\n");
        panic();
    }
    return n;
}

void file_seek(File* file, f64 offset) {
    fseek(file->handle, offset, SEEK_CUR);
}

void file_seek_from_start(File* file, f64 offset) {
    fseek(file->handle, offset, SEEK_SET);
}

void file_seek_from_end(File* file, f64 offset) {
    fseek(file->handle, offset, SEEK_END);
}

void file_seek_start(File* file) {
    fseek(file->handle, 0, SEEK_SET);
}

void file_seek_end(File* file) {
    fseek(file->handle, 0, SEEK_END);
}

bool file_at_end(File* file) {
    if(file_peek_char(file) == EOF) {
        return true;
    }
    return false;
}

char file_peek_char(File* file) {
    char c = file_read_char(file);
    if(c != EOF) {
        ungetc(c, file->handle);
    }
    return c;
}

void file_peek_string_token(File* file, String* dst, char delimiter) {
    u64 len = file_read_string_token(file, dst, delimiter);
    file_seek(file, -len);
}

#endif
#endif
