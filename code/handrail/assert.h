#ifndef handrail_assert_h_INCLUDED
#define handrail_assert_h_INCLUDED

#if PLATFORM == PLATFORM_LINUX
#include <execinfo.h>
#endif

#define DEBUG_ASSERTIONS 1
#define DEBUG_STRICT_ASSERTIONS 1

// Both report through the log, whatever LOG_MASK is, so failures reach every
// log target including the log file.
#define panic() do { log_write(LOG_ERROR, __FILE__, __LINE__, "Panic"); print_callstack(); exit(1); } while(0)

#undef assert
#define assert(assertion) do { if(!(assertion)) { log_write(LOG_ERROR, __FILE__, __LINE__, "Assertion failed: %s", #assertion); print_callstack(); exit(1); } } while(0)

#if DEBUG_ASSERTIONS
    #define debug_assert(assertion) assert(assertion)
#else
    #define debug_assert
#endif

#if DEBUG_STRICT_ASSERTIONS
    #define strict_assert(assertion) assert(assertion)
#else
    #define strict_assert
#endif

#endif

#if defined(HANDRAIL_IMPLEMENTATION_PASS) && !defined(handrail_assert_h_IMPLEMENTED)
#define handrail_assert_h_IMPLEMENTED

void print_callstack() {
#if PLATFORM == PLATFORM_LINUX
    void* callstack[128];
    int frames = backtrace(callstack, 128);
    char** strs = backtrace_symbols(callstack, frames);
    char header[] = "--- Call Stack ---\n";
    log_write_raw(LOG_ERROR, header, sizeof(header) - 1);
    for (int i = 0; i < frames; i++) {
        char line[LOG_LINE_MAX];
        i32 len = snprintf(line, LOG_LINE_MAX, "%s\n", strs[i]);
        log_write_raw(LOG_ERROR, line, len < LOG_LINE_MAX ? len : LOG_LINE_MAX - 1);
    }
    free(strs); 
#elif PLATFORM == PLATFORM_WINDOWS
    // TODO: Walk the stack with CaptureStackBackTrace and DbgHelp
#elif PLATFORM == PLATFORM_WEB
#endif
}

#endif
