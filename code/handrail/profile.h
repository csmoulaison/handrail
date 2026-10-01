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
// the frame, and profile_frame_end moves the totals into last_ns, the history
// ring, and the average and max over that ring. Different timers nest freely,
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

// Frames of per-timer history kept for averages, maxima, and graphs
#ifndef PROFILE_HISTORY_LEN
#define PROFILE_HISTORY_LEN 512
#endif

#ifndef PROFILE_NAME_MAX
#define PROFILE_NAME_MAX 24
#endif

typedef struct {
    bool running;
    u64  start_ns;
    // This frame so far
    u64  accum_ns;
    u32  accum_calls;
    // The last completed frame
    u64  last_ns;
    u32  last_calls;
    // Over the history ring
    u64  average_ns;
    u64  max_ns;
    u64  history_sum;
    // Per-frame totals. The oldest entry is at Profile.history_index.
    u64  history[PROFILE_HISTORY_LEN];
    char name[PROFILE_NAME_MAX];
} ProfileTimer;

typedef struct {
    ProfileTimer timers[PROFILE_TIMERS_LEN];
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
// Name a timer in the bound profile. Names longer than PROFILE_NAME_MAX-1 are truncated.
void          profile_set_name(u64 timer, String name);
// Close the frame: move each timer's totals into its last values and history.
// Called once per frame by the platform loop, with no timers running.
void          profile_frame_end(void);
// A timer's results in the bound profile, or NULL if the module is unbound.
ProfileTimer* profile_get(u64 timer);
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
    assert(timer != 0);
    i32 index = 0;
    while((timer & 1) == 0) {
        timer >>= 1;
        index++;
    }
    return index;
}

static void profile_name_write(Profile* profile, i32 index, String name) {
    u64 len = name.len < PROFILE_NAME_MAX - 1 ? name.len : PROFILE_NAME_MAX - 1;
    memcpy(profile->timers[index].name, name.text, len);
    profile->timers[index].name[len] = '\0';
}

u64 profile_time_ns(void) {
#if PLATFORM == PLATFORM_WINDOWS
    static LARGE_INTEGER frequency = { 0 };
    if(frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
    }
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
    for(i32 i = 0; i < PROFILE_ENGINE_TIMERS_LEN; i++) {
        if(profile_engine_timer_names[i] != NULL) {
            profile_name_write(profile, i, string_const(profile_engine_timer_names[i]));
        }
    }
}

void profile_bind(Profile* profile) {
    profile_global = profile;
}

void profile_set_name(u64 timer, String name) {
    assert(profile_global != NULL);
    profile_name_write(profile_global, profile_timer_index(timer), name);
}

void profile_frame_end(void) {
    if(profile_global == NULL) return;
    Profile* profile = profile_global;
    u64 history_len = profile->frame_count + 1 < PROFILE_HISTORY_LEN ? profile->frame_count + 1 : PROFILE_HISTORY_LEN;
    for(i32 i = 0; i < PROFILE_TIMERS_LEN; i++) {
        ProfileTimer* timer = &profile->timers[i];
        if(timer->running) {
            fprintf(stderr, "Profile timer %i (%s) is still running at the end of the frame\n", i, timer->name);
            panic();
        }

        // A timer that hasn't run this frame or in the history has nothing to update
        if(timer->accum_calls == 0 && timer->history_sum == 0 && timer->last_calls == 0) {
            continue;
        }

        // Record the frame, replacing the oldest entry in the ring
        u64 evicted = timer->history[profile->history_index];
        timer->last_ns = timer->accum_ns;
        timer->last_calls = timer->accum_calls;
        timer->history[profile->history_index] = timer->accum_ns;
        timer->accum_ns = 0;
        timer->accum_calls = 0;

        // Average and max over the filled part of the ring. The max only needs
        // a rescan when the entry that held it was evicted.
        timer->history_sum = timer->history_sum - evicted + timer->last_ns;
        timer->average_ns = timer->history_sum / history_len;
        if(timer->last_ns >= timer->max_ns) {
            timer->max_ns = timer->last_ns;
        } else if(evicted == timer->max_ns) {
            u64 max = 0;
            for(i32 j = 0; j < PROFILE_HISTORY_LEN; j++) {
                if(timer->history[j] > max) {
                    max = timer->history[j];
                }
            }
            timer->max_ns = max;
        }
    }
    profile->history_index = (profile->history_index + 1) % PROFILE_HISTORY_LEN;
    profile->frame_count++;
}

ProfileTimer* profile_get(u64 timer) {
    if(profile_global == NULL) return NULL;
    return &profile_global->timers[profile_timer_index(timer)];
}

void profile_log(u64 log_layer) {
    if(profile_global == NULL) return;
    for(i32 i = 0; i < PROFILE_TIMERS_LEN; i++) {
        ProfileTimer* timer = &profile_global->timers[i];
        if(((1ull << i) & (PROFILE_MASK)) == 0 || timer->max_ns == 0) {
            continue;
        }
        char game_name[PROFILE_NAME_MAX];
        char* name = timer->name;
        if(name[0] == '\0') {
            snprintf(game_name, PROFILE_NAME_MAX, "game:%i", i - PROFILE_ENGINE_TIMERS_LEN);
            name = game_name;
        }
        log_print(log_layer, "Profile %-24s last %8.3f ms, average %8.3f ms, max %8.3f ms, %u calls last frame",
                  name, (f64)timer->last_ns / 1e6, (f64)timer->average_ns / 1e6, (f64)timer->max_ns / 1e6, timer->last_calls);
    }
}

void profile_timer_begin(u64 timer) {
    if(profile_global == NULL) return;
    ProfileTimer* profile_timer = &profile_global->timers[profile_timer_index(timer)];
    assert(!profile_timer->running);
    profile_timer->running = true;
    profile_timer->start_ns = profile_time_ns();
}

void profile_timer_end(u64 timer) {
    u64 end_ns = profile_time_ns();
    if(profile_global == NULL) return;
    ProfileTimer* profile_timer = &profile_global->timers[profile_timer_index(timer)];
    assert(profile_timer->running);
    profile_timer->running = false;
    profile_timer->accum_ns += end_ns - profile_timer->start_ns;
    profile_timer->accum_calls++;
}

void profile_record(u64 timer, u64 ns) {
    if(profile_global == NULL) return;
    ProfileTimer* profile_timer = &profile_global->timers[profile_timer_index(timer)];
    profile_timer->accum_ns += ns;
    profile_timer->accum_calls++;
}

#endif
