#ifndef handrail_buffer_h_INCLUDED
#define handrail_buffer_h_INCLUDED

// NOTO: Debug build code for tracking suballocations.

#ifndef BUFFER_DEBUG
#define BUFFER_DEBUG false
#endif

#ifndef BUFFER_VERBOSE
#define BUFFER_VERBOSE false
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

typedef struct {
    u8* memory;
    u64 size;
    
#if BUFFER_DEBUG
    String label;
    BufferType type;
    struct Buffer* children[BUFFER_MAX_TRACKED_SUBALLOCATIONS];
    u32 children_len;
#endif
} Buffer;

Buffer buffer_alloc(Buffer* buffer, u64 at_byte, u64 size, String label);
Buffer buffer_alloc_typed(Buffer* buffer, u64 at_byte, u64 size, String label, BufferType type);
Buffer buffer_from_memory(void* memory, u64 size, String label);
Buffer buffer_from_memory_typed(void* memory, u64 size, String label, BufferType type);
// Defined here to clear up circular dependency.
String string_from_buffer(Buffer* buffer, u64 byte_index, u64 capacity);
Buffer buffer_malloc(u64 size, String label);
// This is only useful for when BUFFER_DEBUG is active.
void buffer_clear_suballocations(Buffer* buffer);

#ifdef CSM_IMPLEMENTATION

Buffer buffer_alloc(Buffer* buffer, u64 at_byte, u64 size, String label) {
    return buffer_alloc_typed(buffer, at_byte, size, label, BUFFER_TYPE_BUFFER | BUFFER_TYPE_SUB);
}

Buffer buffer_alloc_typed(Buffer* parent, u64 at_byte, u64 size, String label, BufferType type) {
    if(at_byte + size > parent->size) {
        fprintf(stderr, "buffer_alloc_typed: overflow\n");
        panic();
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

String string_from_buffer(Buffer* buffer, u64 byte_index, u64 capacity) {
    String string;
    string.text = (char*)buffer_alloc(buffer, byte_index, capacity, string_const("string_from_buffer")).memory;
    string.len = 0;
    string.capacity = capacity;
    return string;
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
#endif
