#ifndef handrail_string_h_INCLUDED
#define handrail_string_h_INCLUDED

#include <ctype.h>
#include <errno.h>
#include <limits.h>

typedef struct {
    char* text;
    u64 capacity;
    u64 len;
} String;

// Return a new string directly from a memory location.
String string_init(void* buffer, u64 capacity);
// Return a new string from a literal cstring. Capacity is set to the string length.
String string_const(char* literal);
// Return a new empty string backed by memory in buffer, starting at byte_index.
String string_from_buffer(Buffer* buffer, u64 byte_index, u64 capacity);
// Return a new empty string allocated from stack.
String string_from_stack(Stack* stack, u64 capacity);

// Return true if both strings have equal length and the same characters.
bool string_equals(String a, String b);

// Set the string length to 0, leaving its capacity intact.
void string_clear(String* string);
// Convert all characters in string to uppercase.
void string_to_upper(String* string);
// Convert all characters in string to lowercase.
void string_to_lower(String* string);
// Convert all instances of a character in string to another.
void string_replace_char(String* string, char original, char replacement);
// Convert all instances of a substring in string to another.
void string_replace_substring(String* dst, String original, String replacement);

// Write a buffer of chars to dst.
void string_write(String* string, char* chars, u64 len);
// Write src to the end of dst.
void string_cat(String* dst, String src);
// Write a single char to dst
void string_write_char(String* dst, char c);
// Writes a 0 byte to the string.
void string_write_null_terminator(String* string);
// Write a string representation of an integer to dst.
void string_print_int(String* string, i64 n);
// Write a string representation of an f64 to dst.
void string_print_float(String* string, f64 n);

// Maximum number of arguments to string_format, including the format itself.
#define STRING_FORMAT_ARGS_MAX 16

typedef enum {
    STRING_FORMAT_ARG_INT,
    STRING_FORMAT_ARG_UINT,
    STRING_FORMAT_ARG_FLOAT,
    STRING_FORMAT_ARG_STRING,
} StringFormatArgType;

// A string_format argument, tagged with its type so any integer or float size can be passed.
typedef struct {
    StringFormatArgType type;
    union {
        i64 i;
        u64 u;
        f64 f;
        String s;
    };
} StringFormatArg;

StringFormatArg string_format_arg_int(i64 n);
StringFormatArg string_format_arg_uint(u64 n);
StringFormatArg string_format_arg_float(f64 n);
StringFormatArg string_format_arg_string(String s);
StringFormatArg string_format_arg_cstring(const char* s);

// Wrap a value in a StringFormatArg based on its type. Standard types are listed
// rather than the u8..i64 aliases, since the aliases map to different standard
// types per platform and _Generic rejects duplicates.
#define string_format_arg(x) _Generic((x),                  \
    _Bool:              string_format_arg_int,              \
    char:               string_format_arg_int,              \
    signed char:        string_format_arg_int,              \
    short:              string_format_arg_int,              \
    int:                string_format_arg_int,              \
    long:               string_format_arg_int,              \
    long long:          string_format_arg_int,              \
    unsigned char:      string_format_arg_uint,             \
    unsigned short:     string_format_arg_uint,             \
    unsigned int:       string_format_arg_uint,             \
    unsigned long:      string_format_arg_uint,             \
    unsigned long long: string_format_arg_uint,             \
    float:              string_format_arg_float,            \
    double:             string_format_arg_float,            \
    String:             string_format_arg_string,           \
    char*:              string_format_arg_cstring,          \
    const char*:        string_format_arg_cstring)(x)

// Count the variadic arguments, up to STRING_FORMAT_ARGS_MAX.
#define STRING_FORMAT_COUNT(...) STRING_FORMAT_COUNT_(__VA_ARGS__, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define STRING_FORMAT_COUNT_(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, n, ...) n

// Apply string_format_arg to each variadic argument, comma separated.
#define STRING_FORMAT_CAT(a, b)  STRING_FORMAT_CAT_(a, b)
#define STRING_FORMAT_CAT_(a, b) a##b
#define STRING_FORMAT_MAP(...)   STRING_FORMAT_CAT(STRING_FORMAT_MAP_, STRING_FORMAT_COUNT(__VA_ARGS__))(__VA_ARGS__)
#define STRING_FORMAT_MAP_1(a)       string_format_arg(a)
#define STRING_FORMAT_MAP_2(a, ...)  string_format_arg(a), STRING_FORMAT_MAP_1(__VA_ARGS__)
#define STRING_FORMAT_MAP_3(a, ...)  string_format_arg(a), STRING_FORMAT_MAP_2(__VA_ARGS__)
#define STRING_FORMAT_MAP_4(a, ...)  string_format_arg(a), STRING_FORMAT_MAP_3(__VA_ARGS__)
#define STRING_FORMAT_MAP_5(a, ...)  string_format_arg(a), STRING_FORMAT_MAP_4(__VA_ARGS__)
#define STRING_FORMAT_MAP_6(a, ...)  string_format_arg(a), STRING_FORMAT_MAP_5(__VA_ARGS__)
#define STRING_FORMAT_MAP_7(a, ...)  string_format_arg(a), STRING_FORMAT_MAP_6(__VA_ARGS__)
#define STRING_FORMAT_MAP_8(a, ...)  string_format_arg(a), STRING_FORMAT_MAP_7(__VA_ARGS__)
#define STRING_FORMAT_MAP_9(a, ...)  string_format_arg(a), STRING_FORMAT_MAP_8(__VA_ARGS__)
#define STRING_FORMAT_MAP_10(a, ...) string_format_arg(a), STRING_FORMAT_MAP_9(__VA_ARGS__)
#define STRING_FORMAT_MAP_11(a, ...) string_format_arg(a), STRING_FORMAT_MAP_10(__VA_ARGS__)
#define STRING_FORMAT_MAP_12(a, ...) string_format_arg(a), STRING_FORMAT_MAP_11(__VA_ARGS__)
#define STRING_FORMAT_MAP_13(a, ...) string_format_arg(a), STRING_FORMAT_MAP_12(__VA_ARGS__)
#define STRING_FORMAT_MAP_14(a, ...) string_format_arg(a), STRING_FORMAT_MAP_13(__VA_ARGS__)
#define STRING_FORMAT_MAP_15(a, ...) string_format_arg(a), STRING_FORMAT_MAP_14(__VA_ARGS__)
#define STRING_FORMAT_MAP_16(a, ...) string_format_arg(a), STRING_FORMAT_MAP_15(__VA_ARGS__)

// Write formatted text to the end of dst. The format is a String or cstring.
// Specifiers: %s (String or cstring), %i (any integer), %f (any float), %% (literal %).
// Takes at most 15 arguments after the format. A specifier that doesn't match its
// argument, or a mismatched argument count, panics.
#define string_format(dst, ...) \
    string_format((dst), (StringFormatArg[]){ STRING_FORMAT_MAP(__VA_ARGS__) }, STRING_FORMAT_COUNT(__VA_ARGS__))
// Return a new string allocated from stack, holding the formatted text. Capacity is
// one byte more than the length, leaving room for a null terminator. Same
// specifiers as string_format.
#define string_format_from_stack(stack, ...) \
    string_format_from_stack((stack), (StringFormatArg[]){ STRING_FORMAT_MAP(__VA_ARGS__) }, STRING_FORMAT_COUNT(__VA_ARGS__))

// The functions share names with the macros above, which call them. Parentheses
// around the name stop the macro from expanding. args[0] is the format.
void   (string_format)(String* dst, StringFormatArg* args, i32 args_len);
String (string_format_from_stack)(Stack* stack, StringFormatArg* args, i32 args_len);

typedef struct {
    String* string;
    u64 head;
} StringReader;

StringReader string_reader_init(String* string);
char string_read_char(StringReader* reader);
void string_read_line(StringReader* reader, String* dst);
void string_read_string_token(StringReader* reader, String* dst, char delimiter);
i64  string_read_int_token(StringReader* reader, char delimiter);
f64  string_read_float_token(StringReader* reader, char delimiter);

#endif

#if defined(HANDRAIL_IMPLEMENTATION_PASS) && !defined(handrail_string_h_IMPLEMENTED)
#define handrail_string_h_IMPLEMENTED

String string_init(void* memory, u64 capacity) {
    String string;
    string.text = memory;
    string.len = 0;
    string.capacity = capacity;
    return string;
}

String string_const(char* literal) {
    u64 len = strlen(literal);
    String string;
    string.text = literal;
    string.len = len;
    string.capacity = len;
    return string;
}

String string_from_buffer(Buffer* buffer, u64 byte_index, u64 capacity) {
    String string;
    string.text = (char*)buffer_alloc(buffer, byte_index, capacity, string_const("string_from_buffer")).memory;
    string.len = 0;
    string.capacity = capacity;
    return string;
}

String string_from_stack(Stack* stack, u64 capacity) {
    String string;
    string.text = stack_alloc(stack, capacity);
    string.len = 0;
    string.capacity = capacity;
    return string;
}

bool string_equals(String a, String b) {
    if(a.len != b.len) {
        return false;
    }
    for(i32 i = 0; i < a.len; i++) {
        if(a.text[i] != b.text[i]) {
            return false;
        }
    }
    return true;
}

void string_clear(String* string) {
    string->len = 0;
}

void string_to_upper(String* string) {
    for(i32 i = 0; i < string->len; i++) {
        string->text[i] = toupper(string->text[i]);
    }
}

void string_to_lower(String* string) {
    for(i32 i = 0; i < string->len; i++) {
        string->text[i] = tolower(string->text[i]);
    }
}

void string_replace_char(String* string, char original, char replacement) {
    for(i32 i = 0; i < string->len; i++) {
        if(string->text[i] == original) {
            string->text[i] = replacement;
        }
    }
}

void string_replace_substring(String* dst, String original, String replacement) {
    i32 string_delta = replacement.len - original.len;
    char* buf = (char*)alloca(dst->capacity);
    String tmp = string_init(buf, dst->capacity);
    for(i32 i = 0; i < dst->len;) {
        bool match = true;
        for(i32 j = 0; j < original.len; j++) {
            if(dst->text[i + j] != original.text[j]) {
                match = false;
                break;
            }
        }
        if(match == true) {
            string_cat(&tmp, replacement);
            i += original.len;
        } else {
            string_write_char(&tmp, dst->text[i]);
            i += 1;
        }
    }
    string_clear(dst);
    string_cat(dst, tmp);
}

void string_write(String* dst, char* src, u64 len) {
    if(dst->len + len > dst->capacity) {
        fprintf(stderr, "String capacity overflow. Capacity: %" PRIu64 ", Write len: %" PRIu64 "\n", dst->capacity, len);
        panic();
    }
    memcpy(&dst->text[dst->len], src, len);
    dst->len += len;
}

void string_write_char(String* dst, char c) {
    string_write(dst, &c, 1);
}

void string_write_null_terminator(String* string) {
    string_write_char(string, '\0');
}

void string_cat(String* dst, String src) {
    string_write(dst, src.text, src.len);
}

void string_print_int(String* dst, i64 n) {
    char buf[256];
    u64 len = snprintf(buf, 256, "%" PRId64, n);
    assert(len < 256);
    string_write(dst, buf, len);
}

void string_print_float(String* dst, f64 n) {
    char buf[256];
    u64 len = snprintf(buf, 256, "%lf", n);
    assert(len < 256);
    string_write(dst, buf, len);
}

StringFormatArg string_format_arg_int(i64 n) {
    return (StringFormatArg){ .type = STRING_FORMAT_ARG_INT, .i = n };
}

StringFormatArg string_format_arg_uint(u64 n) {
    return (StringFormatArg){ .type = STRING_FORMAT_ARG_UINT, .u = n };
}

StringFormatArg string_format_arg_float(f64 n) {
    return (StringFormatArg){ .type = STRING_FORMAT_ARG_FLOAT, .f = n };
}

StringFormatArg string_format_arg_string(String s) {
    return (StringFormatArg){ .type = STRING_FORMAT_ARG_STRING, .s = s };
}

StringFormatArg string_format_arg_cstring(const char* s) {
    return string_format_arg_string(string_const((char*)s));
}

// Write chars to dst if it isn't NULL, and return len either way.
static u64 string_format_emit(String* dst, char* chars, u64 len) {
    if(dst != NULL) {
        string_write(dst, chars, len);
    }
    return len;
}

static void string_format_mismatch(String format, char specifier, i32 arg_index) {
    fprintf(stderr, "string_format: argument %i doesn't match specifier '%%%c' in format \"" STRING_FMT "\"\n",
            arg_index, specifier, STRING_ARG(format));
    panic();
}

// Walk the format, writing to dst unless it's NULL. Returns the formatted length,
// so the same walk both measures and writes.
static u64 string_format_walk(String* dst, StringFormatArg* args, i32 args_len) {
    assert(args_len > 0);
    assert(args[0].type == STRING_FORMAT_ARG_STRING);
    String format = args[0].s;
    i32 arg_index = 1;
    u64 len = 0;
    for(u64 i = 0; i < format.len; i++) {
        char c = format.text[i];
        if(c != '%') {
            len += string_format_emit(dst, &c, 1);
            continue;
        }

        // Read the specifier
        i++;
        if(i == format.len) {
            fprintf(stderr, "string_format: format ends with '%%': \"" STRING_FMT "\"\n", STRING_ARG(format));
            panic();
        }
        char specifier = format.text[i];
        if(specifier == '%') {
            len += string_format_emit(dst, &specifier, 1);
            continue;
        }
        if(arg_index >= args_len) {
            fprintf(stderr, "string_format: too few arguments for format \"" STRING_FMT "\"\n", STRING_ARG(format));
            panic();
        }
        StringFormatArg arg = args[arg_index];

        // Write the argument. %lf of a large f64 can be over 300 chars.
        char buf[512];
        i32 buf_len = 0;
        switch(specifier) {
            case 's': {
                if(arg.type != STRING_FORMAT_ARG_STRING) string_format_mismatch(format, specifier, arg_index);
                len += string_format_emit(dst, arg.s.text, arg.s.len);
            } break;
            case 'i': {
                if(arg.type == STRING_FORMAT_ARG_INT) {
                    buf_len = snprintf(buf, sizeof(buf), "%" PRId64, arg.i);
                } else if(arg.type == STRING_FORMAT_ARG_UINT) {
                    buf_len = snprintf(buf, sizeof(buf), "%" PRIu64, arg.u);
                } else {
                    string_format_mismatch(format, specifier, arg_index);
                }
                assert(buf_len >= 0 && buf_len < sizeof(buf));
                len += string_format_emit(dst, buf, buf_len);
            } break;
            case 'f': {
                if(arg.type != STRING_FORMAT_ARG_FLOAT) string_format_mismatch(format, specifier, arg_index);
                buf_len = snprintf(buf, sizeof(buf), "%lf", arg.f);
                assert(buf_len >= 0 && buf_len < sizeof(buf));
                len += string_format_emit(dst, buf, buf_len);
            } break;
            default: {
                fprintf(stderr, "string_format: unknown specifier '%%%c' in format \"" STRING_FMT "\"\n",
                        specifier, STRING_ARG(format));
                panic();
            } break;
        }
        arg_index++;
    }
    if(arg_index != args_len) {
        fprintf(stderr, "string_format: %i arguments given, format \"" STRING_FMT "\" uses %i\n",
                args_len - 1, STRING_ARG(format), arg_index - 1);
        panic();
    }
    return len;
}

void (string_format)(String* dst, StringFormatArg* args, i32 args_len) {
    string_format_walk(dst, args, args_len);
}

String (string_format_from_stack)(Stack* stack, StringFormatArg* args, i32 args_len) {
    u64 len = string_format_walk(NULL, args, args_len);
    String string = string_from_stack(stack, len + 1);
    string_format_walk(&string, args, args_len);
    return string;
}

StringReader string_reader_init(String* string) {
    StringReader reader;
    reader.string = string;
    reader.head = 0;
    return reader;
}

char string_read_char(StringReader* reader) {
    if(reader->head == reader->string->len) {
        return 0;
    }
    reader->head++;
    return reader->string->text[reader->head - 1];
}

void string_read_line(StringReader* reader, String* dst) {
    char c;
    while((c = string_read_char(reader)) != '\n') {
        if(c == 0) {
            return;
        }
        string_write_char(dst, c);
    }
}

void string_read_string_token(StringReader* reader, String* dst, char delimiter) {
    char c = 0;
    u32 len = 0;
    while((c = string_read_char(reader)) != 0 && c != '\n' && c != delimiter) {
        string_write_char(dst, c);
        len++;
    }
    assert(len > 0);
    return;
}

// TODO: factor int/float conversions into string_to_* functions.
// See also file.h todo.
i64 string_read_int_token(StringReader* reader, char delimiter) {
    String tmp = string_init((char[256]){}, 256);
    string_read_string_token(reader, &tmp, delimiter);
    string_write_null_terminator(&tmp);
    char* end;
    i64 n = strtol(tmp.text, &end, 10);
    errno = 0;
    if (end == tmp.text) {
        fprintf(stderr, "Could not read int token. No digits found.\n");
        panic();
    } else if (errno == ERANGE || n > INT_MAX || n < INT_MIN) {
        fprintf(stderr, "Error: Value out of range for an int.\n");
        panic();
    }
    return n;
}

f64 string_read_float_token(StringReader* reader, char delimiter) { 
    String tmp = string_init((char[256]){}, 256);
    string_read_string_token(reader, &tmp, delimiter);
    string_write_null_terminator(&tmp);
    char* end;
    f64 n = strtof(tmp.text, &end);
    if (end == tmp.text) {
        fprintf(stderr, "Could not read float token.\n");
        panic();
    }
    return n;
    panic(); 
}

#endif
