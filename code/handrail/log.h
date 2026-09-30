#ifndef handrail_log_h_INCLUDED
#define handrail_log_h_INCLUDED

// Layers are bits in a u64 mask. The engine owns the low 32 bits and the game
// owns the high 32 bits, so the game can add its own layers without the engine
// knowing about them:
//
//     #define LOG_NET_SEND LOG_GAME_LAYER(0)
//
// LOG_MASK selects the enabled layers at build time. Calls to log_print on a
// disabled layer compile out entirely, arguments included.

// Engine layers
#define LOG_ERROR      (1ull << 0)
#define LOG_WARN       (1ull << 1)
#define LOG_INFO       (1ull << 2)
#define LOG_PLATFORM   (1ull << 3)
#define LOG_AUDIO      (1ull << 4)
#define LOG_RENDER     (1ull << 5)
#define LOG_ASSET      (1ull << 6)
#define LOG_HOT_RELOAD (1ull << 7)

#define LOG_ENGINE_LAYERS_LEN 32
#define LOG_ENGINE_ALL        0x00000000FFFFFFFFull
#define LOG_GAME_ALL          0xFFFFFFFF00000000ull
#define LOG_ENGINE_DEFAULT    (LOG_ERROR | LOG_WARN | LOG_INFO)

// Game layers, n is in [0, 32)
#define LOG_GAME_LAYER(n) (1ull << (LOG_ENGINE_LAYERS_LEN + (n)))

// Targets
#define LOG_TARGET_STDOUT (1u << 0)
#define LOG_TARGET_FILE   (1u << 1)

#ifndef LOG_MASK
#define LOG_MASK LOG_ENGINE_DEFAULT
#endif

#ifndef LOG_TARGETS
#define LOG_TARGETS LOG_TARGET_STDOUT
#endif

#ifndef LOG_FILE_PATH
#define LOG_FILE_PATH "log.txt"
#endif

#ifndef LOG_LINE_MAX
#define LOG_LINE_MAX 1024
#endif

#ifndef LOG_LAYER_NAME_MAX
#define LOG_LAYER_NAME_MAX 16
#endif

// Helpers for printing a String with a %-format: log_print(LOG_INFO, "name: " STRING_FMT, STRING_ARG(name));
#define STRING_FMT    "%.*s"
#define STRING_ARG(s) (int)(s).len, (s).text

#ifdef __GNUC__
#define LOG_PRINTF_FORMAT(fmt_index, args_index) __attribute__((format(printf, fmt_index, args_index)))
#else
#define LOG_PRINTF_FORMAT(fmt_index, args_index)
#endif

typedef struct {
    u32  targets;
    File file;
    char layer_names[64][LOG_LAYER_NAME_MAX];
} Log;

// Log a printf-style message on a layer. Compiled out if the layer is not in LOG_MASK.
#define log_print(layer, ...) do { if(((layer) & (LOG_MASK)) != 0) log_write((layer), __FILE__, __LINE__, __VA_ARGS__); } while(0)
// Log an error, then panic. Panics even if LOG_ERROR is not in LOG_MASK.
#define log_exit(...)         do { log_print(LOG_ERROR, __VA_ARGS__); panic(); } while(0)

// Open the log's targets. path is only used if targets includes LOG_TARGET_FILE.
void log_init(Log* log, u32 targets, String path);
void log_close(Log* log);
// Set the log this module writes to. Each module (executable, game library)
// must bind separately. An unbound module logs to stdout.
void log_bind(Log* log);
// Name a layer in the bound log. Names longer than LOG_LAYER_NAME_MAX-1 are truncated.
void log_set_layer_name(u64 layer, String name);
// Use log_print rather than calling this directly.
void log_write(u64 layer, char* file, i32 line, char* format, ...) LOG_PRINTF_FORMAT(4, 5);

#ifdef CSM_IMPLEMENTATION

#include <stdarg.h>

static Log* log_global = NULL;

static char* log_engine_layer_names[LOG_ENGINE_LAYERS_LEN] = {
    "error", "warn", "info", "platform", "audio", "render", "asset", "hot_reload",
};

static i32 log_layer_index(u64 layer) {
    assert(layer != 0);
    i32 index = 0;
    while((layer & 1) == 0) {
        layer >>= 1;
        index++;
    }
    return index;
}

static void log_layer_name_write(Log* log, i32 index, String name) {
    u64 len = name.len < LOG_LAYER_NAME_MAX - 1 ? name.len : LOG_LAYER_NAME_MAX - 1;
    memcpy(log->layer_names[index], name.text, len);
    log->layer_names[index][len] = '\0';
}

void log_init(Log* log, u32 targets, String path) {
    memset(log, 0, sizeof(Log));
    log->targets = targets;
    if(targets & LOG_TARGET_FILE) {
        log->file = file_open(path, FILE_OPEN_WRITE);
    }
    for(i32 i = 0; i < LOG_ENGINE_LAYERS_LEN; i++) {
        if(log_engine_layer_names[i] != NULL) {
            log_layer_name_write(log, i, string_const(log_engine_layer_names[i]));
        }
    }
}

void log_close(Log* log) {
    if(log->targets & LOG_TARGET_FILE) {
        file_close(&log->file);
    }
    if(log_global == log) {
        log_global = NULL;
    }
}

void log_bind(Log* log) {
    log_global = log;
}

void log_set_layer_name(u64 layer, String name) {
    assert(log_global != NULL);
    log_layer_name_write(log_global, log_layer_index(layer), name);
}

void log_write(u64 layer, char* file, i32 line, char* format, ...) {
    // Look up the layer name
    i32 index = log_layer_index(layer);
    char* name = NULL;
    if(log_global != NULL && log_global->layer_names[index][0] != '\0') {
        name = log_global->layer_names[index];
    } else if(index < LOG_ENGINE_LAYERS_LEN) {
        name = log_engine_layer_names[index];
    }

    // Format the line. Each line is written with a single call per target so
    // lines logged from other threads don't interleave.
    char buf[LOG_LINE_MAX];
    i32 len;
    if(name != NULL) {
        len = snprintf(buf, LOG_LINE_MAX, "[%s] %s:%i ", name, file, line);
    } else {
        len = snprintf(buf, LOG_LINE_MAX, "[game:%i] %s:%i ", index - LOG_ENGINE_LAYERS_LEN, file, line);
    }
    if(len < LOG_LINE_MAX) {
        va_list args;
        va_start(args, format);
        len += vsnprintf(buf + len, LOG_LINE_MAX - len, format, args);
        va_end(args);
    }
    // Truncate, leaving room for the newline
    if(len > LOG_LINE_MAX - 2) {
        len = LOG_LINE_MAX - 2;
    }
    buf[len++] = '\n';
    buf[len] = '\0';

    // Write to targets
    u32 targets = log_global != NULL ? log_global->targets : LOG_TARGET_STDOUT;
    if(targets & LOG_TARGET_STDOUT) {
        FILE* stream = (layer & (LOG_ERROR | LOG_WARN)) ? stderr : stdout;
        fwrite(buf, 1, len, stream);
        fflush(stream);
    }
    if(targets & LOG_TARGET_FILE) {
        fwrite(buf, 1, len, log_global->file.handle);
        fflush(log_global->file.handle);
    }
}

#endif
#endif
