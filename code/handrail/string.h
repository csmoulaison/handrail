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

#ifdef CSM_IMPLEMENTATION

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

// NOTO: factor int/float conversions into string_to_* functions.
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
#endif
