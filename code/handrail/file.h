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

// A directory open for reading its entries one at a time
typedef struct {
#if PLATFORM == PLATFORM_LINUX
    DIR*             handle;
#elif PLATFORM == PLATFORM_WINDOWS
    HANDLE           handle;
    WIN32_FIND_DATAA data;
    // Opening reads the first entry, so it's held until the first read
    bool             pending;
#elif PLATFORM == PLATFORM_WEB
    // TODO: Web implementation
    u8               unused;
#endif
} FileDirectory;

typedef struct {
    // View of the entry's name, valid until the next read
    String name;
    bool   is_directory;
} FileDirectoryEntry;

File file_open(String fname, FileOpenMode mode);
bool file_try_open(String fname, FileOpenMode mode, File* out);
void file_close(File* file);
u64 file_size(File* file);
u64 file_last_modified(File* file);
// Returns 0 if the file doesn't exist
u64 file_path_last_modified(String path);
// Returns 0 if the file doesn't exist
u64 file_path_size(String path);
String* file_names_in_directory(String path, i32* out_path_count, Stack* stack);
String* file_paths_in_directory(String path, i32* out_path_count, Stack* stack);
// Open a directory to read its entries. Fails if it can't be opened.
bool file_directory_try_open(String path, FileDirectory* out_directory);
// Read the next entry, skipping . and .. Fails once there are none left.
bool file_directory_try_read(FileDirectory* directory, FileDirectoryEntry* out_entry);
// Close a directory opened with file_directory_try_open.
void file_directory_close(FileDirectory* directory);

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

#endif

#if defined(HANDRAIL_IMPLEMENTATION_PASS) && !defined(handrail_file_h_IMPLEMENTED)
#define handrail_file_h_IMPLEMENTED

File file_open(String fname, FileOpenMode mode) {
    File file;
    if(!file_try_open(fname, mode, &file)) {
        log_exit("Couldn't open file " STRING_FMT " (mode %i)", STRING_ARG(fname), mode);
    }
    return file;
}

bool file_try_open(String fname, FileOpenMode mode, File* out) {
    FILE* handle;
    char* buf = alloca(fname.len + 1);
    String fname_cstring = string_init(buf, fname.len+1);
    string_cat(&fname_cstring, fname);
    string_write_null_terminator(&fname_cstring);
    // Always binary: text mode on Windows rewrites newlines and stops reading at 0x1A
    switch(mode) {
        case FILE_OPEN_READ: {
            handle = fopen(fname_cstring.text, "rb");
        } break;
        case FILE_OPEN_WRITE: {
            handle = fopen(fname_cstring.text, "wb");
        } break;
        case FILE_OPEN_READ_WRITE: {
            handle = fopen(fname_cstring.text, "r+b");
        } break;
        default: {
            log_exit("File open mode %i not valid.", mode);
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
    if(stat(cpath.text, &file_stat) != 0) {
        return 0;
    }
    return file_stat.st_mtim.tv_sec;
#elif PLATFORM == PLATFORM_WINDOWS
    char* buf = alloca(path.len + 1);
    String cpath = string_init(buf, path.len + 1);
    string_cat(&cpath, path);
    string_write_null_terminator(&cpath);
    WIN32_FILE_ATTRIBUTE_DATA attributes;
    if(!GetFileAttributesExA(cpath.text, GetFileExInfoStandard, &attributes)) {
        return 0;
    }
    return ((u64)attributes.ftLastWriteTime.dwHighDateTime << 32) | (u64)attributes.ftLastWriteTime.dwLowDateTime;
#elif PLATFORM == PLATFORM_WEB
    // TODO: Web implementation
#endif
}

u64 file_path_size(String path) {
    char* buf = alloca(path.len + 1);
    String cpath = string_init(buf, path.len + 1);
    string_cat(&cpath, path);
    string_write_null_terminator(&cpath);
#if PLATFORM == PLATFORM_LINUX
    struct stat file_stat;
    if(stat(cpath.text, &file_stat) != 0) {
        return 0;
    }
    return (u64)file_stat.st_size;
#elif PLATFORM == PLATFORM_WINDOWS
    WIN32_FILE_ATTRIBUTE_DATA attributes;
    if(!GetFileAttributesExA(cpath.text, GetFileExInfoStandard, &attributes)) {
        return 0;
    }
    return ((u64)attributes.nFileSizeHigh << 32) | (u64)attributes.nFileSizeLow;
#elif PLATFORM == PLATFORM_WEB
    // TODO: Web implementation
    return 0;
#endif
}

// TODO: reduntant two functions below.
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
    char* buf = alloca(path.len + 3);
    String pattern = string_init(buf, path.len + 3);
    string_cat(&pattern, path);
    string_cat(&pattern, string_const("/*"));
    string_write_null_terminator(&pattern);

    WIN32_FIND_DATAA find_data;
    HANDLE find = FindFirstFileA(pattern.text, &find_data);
    assert(find != INVALID_HANDLE_VALUE);
    *out_path_count = 0;
    do {
        if(strcmp(find_data.cFileName, ".") != 0 && strcmp(find_data.cFileName, "..") != 0) {
            *out_path_count += 1;
        }
    } while(FindNextFileA(find, &find_data));
    FindClose(find);

    String* paths = (String*)stack_alloc(stack, *out_path_count * sizeof(String));
    *out_path_count = 0;
    find = FindFirstFileA(pattern.text, &find_data);
    assert(find != INVALID_HANDLE_VALUE);
    do {
        if(strcmp(find_data.cFileName, ".") != 0 && strcmp(find_data.cFileName, "..") != 0) {
            paths[*out_path_count] = string_from_stack(stack, strlen(find_data.cFileName));
            string_cat(&paths[*out_path_count], string_const(find_data.cFileName));
            *out_path_count += 1;
        }
    } while(FindNextFileA(find, &find_data));
    FindClose(find);
    return paths;
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
    char* buf = alloca(path.len + 3);
    String pattern = string_init(buf, path.len + 3);
    string_cat(&pattern, path);
    string_cat(&pattern, string_const("/*"));
    string_write_null_terminator(&pattern);

    WIN32_FIND_DATAA find_data;
    HANDLE find = FindFirstFileA(pattern.text, &find_data);
    assert(find != INVALID_HANDLE_VALUE);
    *out_path_count = 0;
    do {
        if(strcmp(find_data.cFileName, ".") != 0 && strcmp(find_data.cFileName, "..") != 0) {
            *out_path_count += 1;
        }
    } while(FindNextFileA(find, &find_data));
    FindClose(find);

    String* paths = (String*)stack_alloc(stack, *out_path_count * sizeof(String));
    *out_path_count = 0;
    find = FindFirstFileA(pattern.text, &find_data);
    assert(find != INVALID_HANDLE_VALUE);
    do {
        if(strcmp(find_data.cFileName, ".") != 0 && strcmp(find_data.cFileName, "..") != 0) {
            paths[*out_path_count] = string_from_stack(stack, path.len + strlen(find_data.cFileName));
            string_cat(&paths[*out_path_count], path);
            string_cat(&paths[*out_path_count], string_const(find_data.cFileName));
            *out_path_count += 1;
        }
    } while(FindNextFileA(find, &find_data));
    FindClose(find);
    return paths;
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

// TODO: factor int/float conversions into string_to_* functions.
i64 file_read_int_token(File* file, char delimiter) {
    String tmp = string_init((char[256]){}, 256);
    file_read_string_token(file, &tmp, delimiter);
    string_write_null_terminator(&tmp);
    char* end;
    i64 n = strtol(tmp.text, &end, 10);
    errno = 0;
    if (end == tmp.text) {
        log_exit("Could not read int token. No digits found.");
    } else if (errno == ERANGE || n > INT_MAX || n < INT_MIN) {
        log_exit("Could not read int token. Value out of range for an int.");
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
        log_exit("Could not read float token.");
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

bool file_directory_try_open(String path, FileDirectory* out_directory) {
#if PLATFORM == PLATFORM_LINUX
    String cpath = string_init(alloca(path.len + 1), path.len + 1);
    string_cat(&cpath, path);
    string_write_null_terminator(&cpath);
    out_directory->handle = opendir(cpath.text);
    return out_directory->handle != NULL;
#elif PLATFORM == PLATFORM_WINDOWS
    // Find everything in the directory with a wildcard
    String pattern = string_init(alloca(path.len + 3), path.len + 3);
    string_cat(&pattern, path);
    string_cat(&pattern, string_const("\\*"));
    string_write_null_terminator(&pattern);
    out_directory->handle  = FindFirstFileA(pattern.text, &out_directory->data);
    out_directory->pending = out_directory->handle != INVALID_HANDLE_VALUE;
    return out_directory->pending;
#elif PLATFORM == PLATFORM_WEB
    // TODO: Web implementation
    return false;
#endif
}

bool file_directory_try_read(FileDirectory* directory, FileDirectoryEntry* out_entry) {
    while(true) {
        char* name = NULL;
#if PLATFORM == PLATFORM_LINUX
        struct dirent* entity = readdir(directory->handle);
        if(entity == NULL) return false;
        name = entity->d_name;
        // d_type isn't POSIX and some file systems leave it unknown, so stat the entry
        struct stat entry_stat;
        out_entry->is_directory = fstatat(dirfd(directory->handle), name, &entry_stat, 0) == 0 && S_ISDIR(entry_stat.st_mode);
#elif PLATFORM == PLATFORM_WINDOWS
        if(!directory->pending && !FindNextFileA(directory->handle, &directory->data)) return false;
        directory->pending = false;
        name = directory->data.cFileName;
        out_entry->is_directory = (directory->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#elif PLATFORM == PLATFORM_WEB
        // TODO: Web implementation
        return false;
#endif
        if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        out_entry->name = string_const(name);
        return true;
    }
}

void file_directory_close(FileDirectory* directory) {
#if PLATFORM == PLATFORM_LINUX
    closedir(directory->handle);
#elif PLATFORM == PLATFORM_WINDOWS
    FindClose(directory->handle);
#elif PLATFORM == PLATFORM_WEB
    // TODO: Web implementation
#endif
}

#endif
