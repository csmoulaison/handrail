#ifndef handrail_buffer_h_INCLUDED
#define handrail_buffer_h_INCLUDED

#ifndef BUFFER_LABEL_MAX_LENGTH
#define BUFFER_LABEL_MAX_LENGTH 64
#endif

#ifndef BUFFER_TRACKER_MAX_RECORDS
#define BUFFER_TRACKER_MAX_RECORDS 256
#endif

typedef u8 BufferType;
#define BUFFER_TYPE_RAW    (1 << 0)
#define BUFFER_TYPE_BUFFER (1 << 1)
#define BUFFER_TYPE_STACK  (1 << 2)
#define BUFFER_TYPE_SUB    (1 << 3)

struct Buffer {
    u8* memory;
    u64 size;
};

// One tracked buffer. The label is copied, so it outlives the module that made
// it (a hot reloaded game library).
typedef struct {
    u8*        memory;
    u64        size;
    BufferType type;
    u32        label_len;
    char       label[BUFFER_LABEL_MAX_LENGTH];
} BufferRecord;

// Every buffer made while a tracker is bound, in allocation order. The hierarchy
// isn't stored: a record's parent is the smallest other record containing it.
typedef struct {
    BufferRecord records[BUFFER_TRACKER_MAX_RECORDS];
    i32          records_len;
    // Records not stored because the table was full
    u32          dropped;
} BufferTracker;

Buffer buffer_alloc(Buffer* buffer, u64 at_byte, u64 size, String label);
Buffer buffer_alloc_typed(Buffer* buffer, u64 at_byte, u64 size, String label, BufferType type);
Buffer buffer_from_memory(void* memory, u64 size, String label);
Buffer buffer_from_memory_typed(void* memory, u64 size, String label, BufferType type);
Buffer buffer_malloc(u64 size, String label);
// Forget the tracked buffers inside this one, but not this one itself
void   buffer_clear_suballocations(Buffer* buffer);
// The tracked label of a buffer, or "buffer" if it isn't tracked
String buffer_label(Buffer* buffer);

// Track buffers in this tracker from now on. NULL stops tracking.
void   buffer_tracker_bind(BufferTracker* tracker);
// Record a buffer in the bound tracker, if there is one
void   buffer_track(Buffer buffer, String label, BufferType type);

#endif

#if defined(HANDRAIL_IMPLEMENTATION_PASS) && !defined(handrail_buffer_h_IMPLEMENTED)
#define handrail_buffer_h_IMPLEMENTED

static BufferTracker* buffer_tracker_global = NULL;

void buffer_tracker_bind(BufferTracker* tracker) {
    buffer_tracker_global = tracker;
}

void buffer_track(Buffer buffer, String label, BufferType type) {
    BufferTracker* tracker = buffer_tracker_global;
    if(tracker == NULL) return;
    if(tracker->records_len >= BUFFER_TRACKER_MAX_RECORDS) {
        tracker->dropped++;
        return;
    }

    BufferRecord* record = &tracker->records[tracker->records_len++];
    record->memory = buffer.memory;
    record->size = buffer.size;
    record->type = type;
    record->label_len = label.len < BUFFER_LABEL_MAX_LENGTH ? (u32)label.len : BUFFER_LABEL_MAX_LENGTH;
    memcpy(record->label, label.text, record->label_len);
}

String buffer_label(Buffer* buffer) {
    BufferTracker* tracker = buffer_tracker_global;
    if(tracker != NULL) {
        for(i32 i = 0; i < tracker->records_len; i++) {
            BufferRecord* record = &tracker->records[i];
            if(record->memory == buffer->memory && record->size == buffer->size) {
                return (String){ .text = record->label, .capacity = BUFFER_LABEL_MAX_LENGTH, .len = record->label_len };
            }
        }
    }
    return string_const("buffer");
}

Buffer buffer_alloc(Buffer* buffer, u64 at_byte, u64 size, String label) {
    return buffer_alloc_typed(buffer, at_byte, size, label, BUFFER_TYPE_BUFFER | BUFFER_TYPE_SUB);
}

Buffer buffer_alloc_typed(Buffer* parent, u64 at_byte, u64 size, String label, BufferType type) {
    if(at_byte + size > parent->size) {
        log_exit(STRING_FMT ": Buffer overflow. Size: %" PRIu64 ", requested %" PRIu64 "-%" PRIu64,
                 STRING_ARG(buffer_label(parent)), parent->size, at_byte, at_byte + size);
    }

    Buffer child = { .memory = &parent->memory[at_byte], .size = size };
    buffer_track(child, label, type);
    return child;
}

Buffer buffer_from_memory(void* memory, u64 size, String label) {
    return buffer_from_memory_typed(memory, size, label, BUFFER_TYPE_RAW);
}

Buffer buffer_from_memory_typed(void* memory, u64 size, String label, BufferType type) {
    Buffer buffer = { .memory = (u8*)memory, .size = size };
    buffer_track(buffer, label, type);
    return buffer;
}

Buffer buffer_malloc(u64 size, String label) {
    void* memory = malloc(size);
    return buffer_from_memory(memory, size, label);
}

void buffer_clear_suballocations(Buffer* buffer) {
    BufferTracker* tracker = buffer_tracker_global;
    if(tracker == NULL) return;

    // Compact in place, keeping allocation order. A record equal to the buffer
    // is the buffer itself, so it stays.
    u8* min = buffer->memory;
    u8* max = buffer->memory + buffer->size;
    i32 kept = 0;
    for(i32 i = 0; i < tracker->records_len; i++) {
        BufferRecord* record = &tracker->records[i];
        bool inside = record->memory >= min && record->memory + record->size <= max;
        bool same = record->memory == buffer->memory && record->size == buffer->size;
        if(inside && !same) continue;
        if(kept != i) {
            tracker->records[kept] = *record;
        }
        kept++;
    }
    tracker->records_len = kept;
}

#endif
