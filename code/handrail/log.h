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
#define LOG_MEMORY     (1ull << 8)

// Verbose engine layers, for events that fire every frame or every allocation.
// Each pairs with the base layer above and is off by default.
#define LOG_MEMORY_VERBOSE   (1ull << 9)
#define LOG_RENDER_VERBOSE   (1ull << 10)
#define LOG_AUDIO_VERBOSE    (1ull << 11)
#define LOG_PLATFORM_VERBOSE (1ull << 12)

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
#define LOG_LAYER_NAME_MAX 24
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
    // Names set by the game. Engine layers are named by a static table.
    char game_layer_names[64 - LOG_ENGINE_LAYERS_LEN][LOG_LAYER_NAME_MAX];
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
// Name a game layer in the bound log. Names longer than LOG_LAYER_NAME_MAX-1 are truncated.
void log_set_layer_name(u64 layer, String name);
// Use log_print rather than calling this directly.
void log_write(u64 layer, char* file, i32 line, char* format, ...) LOG_PRINTF_FORMAT(4, 5);
// Write text as is to the bound log's targets, with no layer prefix or added
// newline. Ignores LOG_MASK. Used for multi-line output like callstacks.
void log_write_raw(u64 layer, char* text, u64 len);

#endif

#if defined(HANDRAIL_IMPLEMENTATION_PASS) && !defined(handrail_log_h_IMPLEMENTED)
#define handrail_log_h_IMPLEMENTED

#include <stdarg.h>

static Log* log_global = NULL;

static char* log_engine_layer_names[LOG_ENGINE_LAYERS_LEN] = {
    "error", "warn", "info", "platform", "audio", "render", "asset", "hot_reload", "memory",
    "memory_verbose", "render_verbose", "audio_verbose", "platform_verbose",
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

void log_init(Log* log, u32 targets, String path) {
    memset(log, 0, sizeof(Log));
    log->targets = targets;
    if(targets & LOG_TARGET_FILE) {
        log->file = file_open(path, FILE_OPEN_WRITE);
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
    i32 index = log_layer_index(layer);
    assert(index >= LOG_ENGINE_LAYERS_LEN);
    char* dst = log_global->game_layer_names[index - LOG_ENGINE_LAYERS_LEN];
    u64 len = name.len < LOG_LAYER_NAME_MAX - 1 ? name.len : LOG_LAYER_NAME_MAX - 1;
    memcpy(dst, name.text, len);
    dst[len] = '\0';
}

void log_write(u64 layer, char* file, i32 line, char* format, ...) {
    // Look up the layer name
    i32 index = log_layer_index(layer);
    char* name = NULL;
    if(index < LOG_ENGINE_LAYERS_LEN) {
        name = log_engine_layer_names[index];
    } else if(log_global != NULL && log_global->game_layer_names[index - LOG_ENGINE_LAYERS_LEN][0] != '\0') {
        name = log_global->game_layer_names[index - LOG_ENGINE_LAYERS_LEN];
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
    log_write_raw(layer, buf, len);
}

void log_write_raw(u64 layer, char* text, u64 len) {
    u32 targets = log_global != NULL ? log_global->targets : LOG_TARGET_STDOUT;
    if(targets & LOG_TARGET_STDOUT) {
        FILE* stream = (layer & (LOG_ERROR | LOG_WARN)) ? stderr : stdout;
        fwrite(text, 1, len, stream);
        fflush(stream);
    }
    if(targets & LOG_TARGET_FILE) {
        fwrite(text, 1, len, log_global->file.handle);
        fflush(log_global->file.handle);
    }
}

#endif
