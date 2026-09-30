#ifndef handrail_buffer_h_INCLUDED
#define handrail_buffer_h_INCLUDED

// TODO: Debug build code for tracking suballocations.

#ifndef BUFFER_DEBUG
#define BUFFER_DEBUG false
#endif

#ifdef BUFFER_DEBUG
#define BUFFER_LABEL_MAX_LENGTH 64
#endif

#ifndef BUFFER_MAX_TRACKED_SUBALLOCATIONS
#define BUFFER_MAX_TRACKED_SUBALLOCATIONS 64
#endif

typedef u8 BufferType;
#define BUFFER_TYPE_RAW    1 << 0
#define BUFFER_TYPE_BUFFER 1 << 1
#define BUFFER_TYPE_STACK  1 << 2
#define BUFFER_TYPE_SUB    1 << 3

struct Buffer {
    u8* memory;
    u64 size;
    
#if BUFFER_DEBUG
    String label;
    BufferType type;
    Buffer* children[BUFFER_MAX_TRACKED_SUBALLOCATIONS];
    u32 children_len;
#endif
};

Buffer buffer_alloc(Buffer* buffer, u64 at_byte, u64 size, String label);
Buffer buffer_alloc_typed(Buffer* buffer, u64 at_byte, u64 size, String label, BufferType type);
Buffer buffer_from_memory(void* memory, u64 size, String label);
Buffer buffer_from_memory_typed(void* memory, u64 size, String label, BufferType type);
Buffer buffer_malloc(u64 size, String label);
// This is only useful for when BUFFER_DEBUG is active.
void buffer_clear_suballocations(Buffer* buffer);

#endif

#if defined(HANDRAIL_IMPLEMENTATION_PASS) && !defined(handrail_buffer_h_IMPLEMENTED)
#define handrail_buffer_h_IMPLEMENTED

// Labels only exist when BUFFER_DEBUG is on. Logging goes through this so that
// log calls compile either way.
static String buffer_label(Buffer* buffer) {
#if BUFFER_DEBUG
    return buffer->label;
#else
    return string_const("buffer");
#endif
}

Buffer buffer_alloc(Buffer* buffer, u64 at_byte, u64 size, String label) {
    return buffer_alloc_typed(buffer, at_byte, size, label, BUFFER_TYPE_BUFFER | BUFFER_TYPE_SUB);
}

Buffer buffer_alloc_typed(Buffer* parent, u64 at_byte, u64 size, String label, BufferType type) {
    if(at_byte + size > parent->size) {
        log_exit(STRING_FMT ": Buffer overflow. Size: %" PRIu64 ", requested %" PRIu64 "-%" PRIu64,
                 STRING_ARG(buffer_label(parent)), parent->size, at_byte, at_byte + size);
    }

    Buffer child = {};
    child.memory = &parent->memory[at_byte];
    child.size = size;

#if BUFFER_DEBUG
    child.label = label;
    child.type = type;
#endif
    return child;
}

Buffer buffer_from_memory(void* memory, u64 size, String label) {
    return buffer_from_memory_typed(memory, size, label, BUFFER_TYPE_RAW);
}

Buffer buffer_from_memory_typed(void* memory, u64 size, String label, BufferType type) {
    Buffer buffer = {};
    buffer.memory = (u8*)memory;
    buffer.size = size;

#if BUFFER_DEBUG
    buffer.label = label;
    buffer.type = type;
#endif
    return buffer;
}

Buffer buffer_malloc(u64 size, String label) {
    void* memory = malloc(size);
    return buffer_from_memory(memory, size, label);
}

void buffer_clear_suballocations(Buffer* buffer) {
#if BUFFER_DEBUG
    buffer->children_len = 0;
#endif
}

#endif
