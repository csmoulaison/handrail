#ifndef handrail_stack_h_INCLUDED
#define handrail_stack_h_INCLUDED

#define STACK_ARRAY(pointer_to_stack, type, count) (type*)stack_alloc(pointer_to_stack, count * sizeof(type))

typedef struct {
    union {
        Buffer buffer;
        struct {
            u8* memory;
            u64 size;
        };
    };
	u64 head;
} Stack;

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
String string_from_stack(Stack* stack, u64 capacity);

#ifdef CSM_IMPLEMENTATION

Stack stack_init(Buffer buffer, String label) {
    Stack stack = {};
    stack.buffer = buffer;
    stack.head = 0;

#if BUFFER_DEBUG
	stack.buffer.label = label;
    #if BUFFER_VERBOSE
    String log = string_init((char[4096]){}, 4096);
    string_cat(&log, stack.buffer.label);
    string_cat(&log, string_const(": Stack initialized. "));
    string_print_int(&log, buffer.size);
    string_cat(&log, string_const(" bytes"));
    string_write_null_terminator(&log);
	printf("%s\n", log.text);
    #endif
#endif
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

#if BUFFER_VERBOSE
    String log = string_init((char[4096]){}, 4096);
    string_cat(&log, stack->buffer.label);
    string_cat(&log, string_const(": Stack cleared."));
    string_write_null_terminator(&log);
	printf("%s\n", log.text);
#endif
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
#if BUFFER_VERBOSE
        String log = string_init((char[4096]){}, 4096);
        string_cat(&log, stack->buffer.label);
        string_cat(&log, string_const(": Stack overflow. Size: "));
        string_print_int(&log, stack->size);
        string_cat(&log, string_const(", Requested Size: "));
        string_print_int(&log, stack->head + size);
        string_write_null_terminator(&log);
		printf("%s\n", log.text);
#endif
		panic();
	}

#if BUFFER_VERBOSE
    String log = string_init((char[4096]){}, 4096);
    string_cat(&log, stack->buffer.label);
    string_cat(&log, string_const(": Stack allocation from "));
    string_print_int(&log, stack->head);
    string_cat(&log, string_const("-"));
    string_print_int(&log, stack->head + size);
    string_cat(&log, string_const(". "));
    string_print_int(&log, size);
    string_cat(&log, string_const(" bytes"));
    string_write_null_terminator(&log);
	printf("%s\n", log.text);
#endif

	Buffer buffer = buffer_alloc_typed(&stack->buffer, stack->head, size, label, type);
	stack->head += size;

#if BUFFER_VERBOSE
	if(stack->head > stack->size / 2) {
        String log = string_init((char[4096]){}, 4096);
        string_cat(&log, stack->buffer.label);
        string_cat(&log, string_const(": Stack more than half full"));
        string_write_null_terminator(&log);
    	printf("%s\n", log.text);
	}
#endif

    return buffer;
}

String string_from_stack(Stack* stack, u64 capacity) {
    String string;
    string.text = stack_alloc(stack, capacity);
    string.len = 0;
    string.capacity = capacity;
    return string;
}

#endif
#endif
