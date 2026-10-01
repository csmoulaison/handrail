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
} PlatformEventType;

typedef struct {
    PlatformEventType type;
    union {
        PlatformKey key;
        // Position is in window pixels, origin bottom left
        struct {
            PlatformMouseButton button;
            iv2                 position;
        } mouse;
    };
} PlatformEvent;

typedef struct {
    Log*          log;
    Profile*      profile;
    // Scratch memory for the game, cleared after every frame
    Stack*        frame_stack;
    iv2           window_size;
    // In window pixels, origin bottom left. Outside the window while a drag is captured.
    iv2           mouse_position;
    PlatformEvent events[PLATFORM_MAX_EVENTS_PER_FRAME];
    i32           events_len;
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
