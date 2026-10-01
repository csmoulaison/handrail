#ifndef handrail_profile_h_INCLUDED
#define handrail_profile_h_INCLUDED

// Timers are bits in a u64 mask, one bit per timer. The engine owns the low 32
// bits and the game owns the high 32 bits, as with log layers:
//
//     #define PROFILE_GAME_PHYSICS PROFILE_GAME_TIMER(0)
//
//     profile_begin(PROFILE_GAME_PHYSICS);
//     ...
//     profile_end(PROFILE_GAME_PHYSICS);
//
// A timer can run several times per frame. Its time and call count add up over
// the frame, and profile_frame_end records the total time in a history ring and
// the call count as last_calls. Averages, maxima, and graphs are computed from
// the history when needed (profile_history_ns). Different timers nest freely,
// but a timer can't be started while it is already running.
//
// PROFILE_MASK selects the enabled timers at build time. Calls on a disabled
// timer compile out entirely. Profiling is main-thread only: never call it from
// the WASAPI audio thread.

// Engine timers
#define PROFILE_FRAME           (1ull << 0)
#define PROFILE_PLATFORM_EVENTS (1ull << 1)
#define PROFILE_RENDER_BEGIN    (1ull << 2)
#define PROFILE_GAME_UPDATE     (1ull << 3)
#define PROFILE_AUDIO           (1ull << 4)
#define PROFILE_RENDER_END      (1ull << 5)
#define PROFILE_GPU             (1ull << 6)

#define PROFILE_ENGINE_TIMERS_LEN 32
#define PROFILE_TIMERS_LEN        64
#define PROFILE_ENGINE_ALL        0x00000000FFFFFFFFull
#define PROFILE_GAME_ALL          0xFFFFFFFF00000000ull

// Game timers, n is in [0, 32)
#define PROFILE_GAME_TIMER(n) (1ull << (PROFILE_ENGINE_TIMERS_LEN + (n)))

#ifndef PROFILE_MASK
#define PROFILE_MASK 0
#endif

// Frames of per-timer history, which averages, maxima, and graphs are computed from
#ifndef PROFILE_HISTORY_LEN
#define PROFILE_HISTORY_LEN 512
#endif

#ifndef PROFILE_NAME_MAX
#define PROFILE_NAME_MAX 24
#endif

typedef struct {
    u64 start_ns;    // Nonzero while running
    // This frame so far
    u64 accum_ns;
    u32 accum_calls;
    u32 last_calls;  // In the last completed frame
    // Per-frame totals. The oldest entry is at Profile.history_index.
    u64 history[PROFILE_HISTORY_LEN];
} ProfileTimer;

typedef struct {
    ProfileTimer timers[PROFILE_TIMERS_LEN];
    // Names set by the game. Engine timers are named by a static table.
    char         game_names[PROFILE_TIMERS_LEN - PROFILE_ENGINE_TIMERS_LEN][PROFILE_NAME_MAX];
    // Slot the next completed frame is written to
    u32          history_index;
    u64          frame_count;
} Profile;

// Start or stop a timer. Compiled out if the timer is not in PROFILE_MASK.
#define profile_begin(timer) do { if(((timer) & (PROFILE_MASK)) != 0) profile_timer_begin(timer); } while(0)
#define profile_end(timer)   do { if(((timer) & (PROFILE_MASK)) != 0) profile_timer_end(timer); } while(0)
// Add a duration measured some other way (e.g. GPU timestamps) to a timer's
// current frame, as one call. Compiled out if the timer is not in PROFILE_MASK.
#define profile_add(timer, ns) do { if(((timer) & (PROFILE_MASK)) != 0) profile_record((timer), (ns)); } while(0)

// Monotonic high-resolution clock, in nanoseconds. Works whether or not profiling is enabled.
u64           profile_time_ns(void);
void          profile_init(Profile* profile);
// Set the profile this module records to. Each module (executable, game
// library) must bind separately. Timers in an unbound module do nothing.
void          profile_bind(Profile* profile);
// Name a game timer in the bound profile. Names longer than PROFILE_NAME_MAX-1 are truncated.
void          profile_set_name(u64 timer, String name);
// Close the frame: record each timer's total in its history and its call count
// in last_calls. Called once per frame by the platform loop, with no timers running.
void          profile_frame_end(void);
// A timer's total for a completed frame in the bound profile, frames_ago 0
// being the last one. 0 past the recorded history or in an unbound module.
u64           profile_history_ns(u64 timer, u32 frames_ago);
// Frames recorded by the bound profile so far, not capped at PROFILE_HISTORY_LEN.
u64           profile_frames_recorded(void);
// Log name, last, average, and max time, and calls, for every enabled timer that has run.
void          profile_log(u64 log_layer);
// Use profile_begin, profile_end, and profile_add rather than calling these directly.
void          profile_timer_begin(u64 timer);
void          profile_timer_end(u64 timer);
void          profile_record(u64 timer, u64 ns);

#endif

#if defined(HANDRAIL_IMPLEMENTATION_PASS) && !defined(handrail_profile_h_IMPLEMENTED)
#define handrail_profile_h_IMPLEMENTED

#if PLATFORM == PLATFORM_LINUX
#include <time.h>
#endif

static Profile* profile_global = NULL;

static char* profile_engine_timer_names[PROFILE_ENGINE_TIMERS_LEN] = {
    "frame", "platform_events", "render_begin", "game_update", "audio", "render_end", "gpu",
};

static i32 profile_timer_index(u64 timer) {
    return u64_lowest_bit_index(timer);
}

// A timer's name by index, written to buf if it has to be made up
static char* profile_timer_name(i32 index, char* buf) {
    char* name = NULL;
    if(index < PROFILE_ENGINE_TIMERS_LEN) {
        name = profile_engine_timer_names[index];
    } else if(profile_global != NULL && profile_global->game_names[index - PROFILE_ENGINE_TIMERS_LEN][0] != '\0') {
        name = profile_global->game_names[index - PROFILE_ENGINE_TIMERS_LEN];
    }
    if(name == NULL) {
        snprintf(buf, PROFILE_NAME_MAX, "%s:%i", index < PROFILE_ENGINE_TIMERS_LEN ? "engine" : "game",
                 index < PROFILE_ENGINE_TIMERS_LEN ? index : index - PROFILE_ENGINE_TIMERS_LEN);
        name = buf;
    }
    return name;
}

u64 profile_time_ns(void) {
#if PLATFORM == PLATFORM_WINDOWS
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    // Split into seconds and remainder so the multiply can't overflow
    u64 seconds = (u64)(counter.QuadPart / frequency.QuadPart);
    u64 remainder = (u64)(counter.QuadPart % frequency.QuadPart);
    return seconds * 1000000000ull + remainder * 1000000000ull / (u64)frequency.QuadPart;
#elif PLATFORM == PLATFORM_LINUX
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (u64)time.tv_sec * 1000000000ull + (u64)time.tv_nsec;
#elif PLATFORM == PLATFORM_WEB
    // TODO: Web clock
    return 0;
#endif
}

void profile_init(Profile* profile) {
    memset(profile, 0, sizeof(Profile));
}

void profile_bind(Profile* profile) {
    profile_global = profile;
}

void profile_set_name(u64 timer, String name) {
    assert(profile_global != NULL);
    i32 index = profile_timer_index(timer);
    assert(index >= PROFILE_ENGINE_TIMERS_LEN);
    char* dst = profile_global->game_names[index - PROFILE_ENGINE_TIMERS_LEN];
    u64 len = name.len < PROFILE_NAME_MAX - 1 ? name.len : PROFILE_NAME_MAX - 1;
    memcpy(dst, name.text, len);
    dst[len] = '\0';
}

void profile_frame_end(void) {
    if(profile_global == NULL) return;
    Profile* profile = profile_global;
    for(i32 i = 0; i < PROFILE_TIMERS_LEN; i++) {
        ProfileTimer* timer = &profile->timers[i];
        if(timer->start_ns != 0) {
            char buf[PROFILE_NAME_MAX];
            fprintf(stderr, "Profile timer %i (%s) is still running at the end of the frame\n", i, profile_timer_name(i, buf));
            panic();
        }
        timer->history[profile->history_index] = timer->accum_ns;
        timer->last_calls = timer->accum_calls;
        timer->accum_ns = 0;
        timer->accum_calls = 0;
    }
    profile->history_index = (profile->history_index + 1) % PROFILE_HISTORY_LEN;
    profile->frame_count++;
}

u64 profile_history_ns(u64 timer, u32 frames_ago) {
    if(profile_global == NULL || frames_ago >= PROFILE_HISTORY_LEN || frames_ago >= profile_global->frame_count) return 0;
    u32 slot = (profile_global->history_index + PROFILE_HISTORY_LEN - 1 - frames_ago) % PROFILE_HISTORY_LEN;
    return profile_global->timers[profile_timer_index(timer)].history[slot];
}

u64 profile_frames_recorded(void) {
    return profile_global != NULL ? profile_global->frame_count : 0;
}

void profile_log(u64 log_layer) {
    if(profile_global == NULL) return;
    u64 filled = profile_global->frame_count < PROFILE_HISTORY_LEN ? profile_global->frame_count : PROFILE_HISTORY_LEN;
    for(i32 i = 0; i < PROFILE_TIMERS_LEN; i++) {
        ProfileTimer* timer = &profile_global->timers[i];
        if(((1ull << i) & (PROFILE_MASK)) == 0) {
            continue;
        }

        // Average and max over the recorded history
        u64 sum = 0;
        u64 max = 0;
        for(u32 j = 0; j < filled; j++) {
            u64 ns = profile_history_ns(1ull << i, j);
            sum += ns;
            if(ns > max) {
                max = ns;
            }
        }
        if(max == 0 && timer->last_calls == 0) {
            continue;
        }
        char buf[PROFILE_NAME_MAX];
        log_print(log_layer, "Profile %-24s last %8.3f ms, average %8.3f ms, max %8.3f ms, %u calls last frame",
                  profile_timer_name(i, buf), (f64)profile_history_ns(1ull << i, 0) / 1e6, (f64)sum / (f64)filled / 1e6,
                  (f64)max / 1e6, timer->last_calls);
    }
}

void profile_timer_begin(u64 timer) {
    if(profile_global == NULL) return;
    ProfileTimer* profile_timer = &profile_global->timers[profile_timer_index(timer)];
    assert(profile_timer->start_ns == 0);
    profile_timer->start_ns = profile_time_ns();
}

void profile_timer_end(u64 timer) {
    u64 end_ns = profile_time_ns();
    if(profile_global == NULL) return;
    ProfileTimer* profile_timer = &profile_global->timers[profile_timer_index(timer)];
    assert(profile_timer->start_ns != 0);
    profile_timer->accum_ns += end_ns - profile_timer->start_ns;
    profile_timer->start_ns = 0;
    profile_timer->accum_calls++;
}

void profile_record(u64 timer, u64 ns) {
    if(profile_global == NULL) return;
    ProfileTimer* profile_timer = &profile_global->timers[profile_timer_index(timer)];
    profile_timer->accum_ns += ns;
    profile_timer->accum_calls++;
}

#endif
