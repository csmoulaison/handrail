#ifndef handrail_platform_h_INCLUDED
#define handrail_platform_h_INCLUDED

#ifndef PLATFORM_MAX_EVENTS_PER_FRAME
#define PLATFORM_MAX_EVENTS_PER_FRAME 1024
#endif

typedef enum {
    PLATFORM_KEY_NONE,
    PLATFORM_KEY_ESCAPE,
    PLATFORM_KEY_SPACE,
    PLATFORM_KEY_ENTER,
    PLATFORM_KEY_TAB,
    PLATFORM_KEY_W,
    PLATFORM_KEY_A,
    PLATFORM_KEY_S,
    PLATFORM_KEY_D,
    PLATFORM_KEY_Q,
    PLATFORM_KEY_E,
    PLATFORM_KEY_R,
    PLATFORM_KEY_M,
    PLATFORM_KEY_G,
    PLATFORM_KEY_UP,
    PLATFORM_KEY_LEFT,
    PLATFORM_KEY_DOWN,
    PLATFORM_KEY_RIGHT,
    PLATFORM_KEY_F3,
    PLATFORM_KEY_BACKSPACE,
    PLATFORM_KEY_DELETE,
    PLATFORM_KEY_H,
    PLATFORM_KEY_J,
    PLATFORM_KEY_K,
    PLATFORM_KEY_L,
} PlatformKey;

typedef enum {
    PLATFORM_MOUSE_BUTTON_LEFT,
    PLATFORM_MOUSE_BUTTON_MIDDLE,
    PLATFORM_MOUSE_BUTTON_RIGHT,
} PlatformMouseButton;

typedef enum {
    PLATFORM_EVENT_NONE,
    PLATFORM_EVENT_KEYDOWN,
    PLATFORM_EVENT_KEYUP,
    PLATFORM_EVENT_DEFOCUS,
    PLATFORM_EVENT_MOUSE_DOWN,
    PLATFORM_EVENT_MOUSE_UP,
    PLATFORM_EVENT_MOUSE_SCROLL,
    // A held key's OS auto-repeat. Kept apart from KEYDOWN so games that act
    // on presses don't have to filter repeats.
    PLATFORM_EVENT_KEYREPEAT,
    // Text input, after keyboard layout and modifiers are applied. Control
    // characters are sent too, e.g. 0x13 for ctrl+s.
    PLATFORM_EVENT_CHAR,
} PlatformEventType;

// Bit flags for modifier keys
typedef enum {
    PLATFORM_MODIFIER_NONE  = 0,
    PLATFORM_MODIFIER_SHIFT = 1 << 0,
    PLATFORM_MODIFIER_CTRL  = 1 << 1,
    PLATFORM_MODIFIER_ALT   = 1 << 2,
} PlatformModifier;

typedef struct {
    PlatformEventType type;
    // PlatformModifier flags held when a key or char event happened
    u32               modifiers;
    union {
        PlatformKey key;
        // Unicode code point
        u32         codepoint;
        // Position is in window pixels, origin bottom left
        struct {
            PlatformMouseButton button;
            iv2                 position;
        } mouse;
        // Wheel notches, fractional on smooth wheels. Positive y is away from
        // the user, positive x is to the right. Position is as for mouse.
        struct {
            v2  delta;
            iv2 position;
        } scroll;
    };
} PlatformEvent;

typedef struct {
    Log*           log;
    Profile*       profile;
    // Every tracked buffer, for debug views
    BufferTracker* buffers;
    // Scratch memory for the game, cleared after every frame
    Stack*         frame_stack;
    iv2            window_size;
    // Monotonic nanoseconds, sampled once per frame after the frame wait. With
    // present wait that tracks vblank, so intervals are even enough for game clocks.
    u64            time_ns;
    // In window pixels, origin bottom left. Outside the window while a drag is captured.
    iv2            mouse_position;
    PlatformEvent  events[PLATFORM_MAX_EVENTS_PER_FRAME];
    i32            events_len;
} Platform;

void platform_push_event(Platform* platform, PlatformEvent event);

#endif

#if defined(HANDRAIL_IMPLEMENTATION_PASS) && !defined(handrail_platform_h_IMPLEMENTED)
#define handrail_platform_h_IMPLEMENTED

void platform_push_event(Platform* platform, PlatformEvent event) {
    if(platform->events_len >= PLATFORM_MAX_EVENTS_PER_FRAME) {
        log_print(LOG_WARN, "Platform event queue full (PLATFORM_MAX_EVENTS_PER_FRAME is %i). Dropping event type %i",
                  PLATFORM_MAX_EVENTS_PER_FRAME, (i32)event.type);
        return;
    }
    log_print(LOG_PLATFORM_VERBOSE, "Event pushed: type %i, key %i", (i32)event.type, (i32)event.key);
    platform->events[platform->events_len] = event;
    platform->events_len++;
}

#endif
