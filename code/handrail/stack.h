#ifndef handrail_stack_h_INCLUDED
#define handrail_stack_h_INCLUDED

struct Stack {
    union {
        Buffer buffer;
        struct {
            u8* memory;
            u64 size;
        };
    };
    u64 head;
    // Highest head reached since initialization, surviving clears
    u64 high_water;
};

Stack stack_init(Buffer buffer, String label);
Stack stack_from_memory(u8* memory, u64 size, String label);
Stack stack_from_buffer(Buffer* buffer, u64 at_byte, u64 size, String label);
Stack stack_from_stack(Stack* stack, u64 size, String label);

void   stack_clear(Stack* stack);
void   stack_clear_to_zero(Stack* stack);
void*  stack_alloc(Stack* stack, u64 size);
void*  stack_alloc_zero(Stack* stack, u64 size);
Buffer stack_alloc_labeled(Stack* stack, u64 size, String label);
Buffer stack_alloc_typed(Stack* stack, u64 size, String label, BufferType type);
// Log the stack's current and peak usage on LOG_MEMORY
void   stack_log_usage(Stack* stack);

#endif

#if defined(HANDRAIL_IMPLEMENTATION_PASS) && !defined(handrail_stack_h_IMPLEMENTED)
#define handrail_stack_h_IMPLEMENTED

Stack stack_init(Buffer buffer, String label) {
    Stack stack = {};
    stack.buffer = buffer;
    stack.head = 0;
    stack.high_water = 0;
    log_print(LOG_MEMORY, STRING_FMT ": Stack initialized. %" PRIu64 " bytes",
              STRING_ARG(label), buffer.size);
    return stack;
}

Stack stack_from_memory(u8* memory, u64 size, String label)
{
    return stack_init(buffer_from_memory_typed(memory, size, label, BUFFER_TYPE_RAW | BUFFER_TYPE_STACK), label);
}

Stack stack_from_buffer(Buffer* buffer, u64 at_byte, u64 size, String label) {
    return stack_init(buffer_alloc_typed(buffer, at_byte, size, label, BUFFER_TYPE_SUB | BUFFER_TYPE_STACK), label);
}

Stack stack_from_stack(Stack* stack, u64 size, String label) {
    return stack_init(stack_alloc_typed(stack, size, label, BUFFER_TYPE_SUB | BUFFER_TYPE_STACK), label);
}

void stack_clear(Stack* stack)
{
    stack->head = 0;
    buffer_clear_suballocations(&stack->buffer);
    log_print(LOG_MEMORY_VERBOSE, STRING_FMT ": Stack cleared", STRING_ARG(buffer_label(&stack->buffer)));
}

void stack_clear_to_zero(Stack* stack)
{
    memset(stack->memory, 0, stack->head);
    stack_clear(stack);
}

void* stack_alloc(Stack* stack, u64 size)
{
    return stack_alloc_typed(stack, size, string_const("stack_alloc"), BUFFER_TYPE_SUB).memory;
}

void* stack_alloc_zero(Stack* stack, u64 size) {
    void* mem = stack_alloc(stack, size);
    memset(mem, 0, size);
    return mem;
}

Buffer stack_alloc_labeled(Stack* stack, u64 size, String label) {
    return stack_alloc_typed(stack, size, label, BUFFER_TYPE_SUB);
}

Buffer stack_alloc_typed(Stack* stack, u64 size, String label, BufferType type) {
    assert(stack->memory != NULL);
    if(stack->head + size > stack->size) {
        log_exit(STRING_FMT ": Stack overflow. Size: %" PRIu64 ", requested size: %" PRIu64,
                 STRING_ARG(buffer_label(&stack->buffer)), stack->size, stack->head + size);
    }

    log_print(LOG_MEMORY_VERBOSE, STRING_FMT ": Stack allocation from %" PRIu64 "-%" PRIu64 ". %" PRIu64 " bytes",
              STRING_ARG(buffer_label(&stack->buffer)), stack->head, stack->head + size, size);

    Buffer buffer = buffer_alloc_typed(&stack->buffer, stack->head, size, label, type);
    u64 half = stack->size / 2;
    bool crossed_half = stack->head <= half && stack->head + size > half;
    stack->head += size;
    if(stack->head > stack->high_water) {
        stack->high_water = stack->head;
    }

    // Only log when crossing the halfway mark, not on every allocation after it
    if(crossed_half) {
        log_print(LOG_MEMORY, STRING_FMT ": Stack more than half full", STRING_ARG(buffer_label(&stack->buffer)));
    }

    return buffer;
}

void stack_log_usage(Stack* stack) {
    log_print(LOG_MEMORY, STRING_FMT ": Stack usage %" PRIu64 " bytes, peak %" PRIu64 " of %" PRIu64 " bytes (%.1f%%)",
              STRING_ARG(buffer_label(&stack->buffer)), stack->head, stack->high_water, stack->size,
              stack->size > 0 ? 100.0 * (f64)stack->high_water / (f64)stack->size : 0.0);
}

#endif
