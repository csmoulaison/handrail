#ifndef handrail_platform_h_INCLUDED
#define handrail_platform_h_INCLUDED

#ifndef PLATFORM_MAX_EVENTS_PER_FRAME
#define PLATFORM_MAX_EVENTS_PER_FRAME 1024
#endif

// Gamepad slots. A connected gamepad keeps its slot index until it disconnects.
#ifndef PLATFORM_MAX_GAMEPADS
#define PLATFORM_MAX_GAMEPADS 4
#endif

// Physical keys, named for their US layout legends. Letters, digits, F keys
// and numpad digits are contiguous, so platforms can map them by range.
typedef enum {
    PLATFORM_KEY_NONE,
    PLATFORM_KEY_A,
    PLATFORM_KEY_B,
    PLATFORM_KEY_C,
    PLATFORM_KEY_D,
    PLATFORM_KEY_E,
    PLATFORM_KEY_F,
    PLATFORM_KEY_G,
    PLATFORM_KEY_H,
    PLATFORM_KEY_I,
    PLATFORM_KEY_J,
    PLATFORM_KEY_K,
    PLATFORM_KEY_L,
    PLATFORM_KEY_M,
    PLATFORM_KEY_N,
    PLATFORM_KEY_O,
    PLATFORM_KEY_P,
    PLATFORM_KEY_Q,
    PLATFORM_KEY_R,
    PLATFORM_KEY_S,
    PLATFORM_KEY_T,
    PLATFORM_KEY_U,
    PLATFORM_KEY_V,
    PLATFORM_KEY_W,
    PLATFORM_KEY_X,
    PLATFORM_KEY_Y,
    PLATFORM_KEY_Z,
    PLATFORM_KEY_0,
    PLATFORM_KEY_1,
    PLATFORM_KEY_2,
    PLATFORM_KEY_3,
    PLATFORM_KEY_4,
    PLATFORM_KEY_5,
    PLATFORM_KEY_6,
    PLATFORM_KEY_7,
    PLATFORM_KEY_8,
    PLATFORM_KEY_9,
    PLATFORM_KEY_F1,
    PLATFORM_KEY_F2,
    PLATFORM_KEY_F3,
    PLATFORM_KEY_F4,
    PLATFORM_KEY_F5,
    PLATFORM_KEY_F6,
    PLATFORM_KEY_F7,
    PLATFORM_KEY_F8,
    PLATFORM_KEY_F9,
    PLATFORM_KEY_F10,
    PLATFORM_KEY_F11,
    PLATFORM_KEY_F12,
    PLATFORM_KEY_ESCAPE,
    PLATFORM_KEY_TAB,
    PLATFORM_KEY_SPACE,
    PLATFORM_KEY_ENTER,
    PLATFORM_KEY_BACKSPACE,
    PLATFORM_KEY_DELETE,
    PLATFORM_KEY_INSERT,
    PLATFORM_KEY_HOME,
    PLATFORM_KEY_END,
    PLATFORM_KEY_PAGE_UP,
    PLATFORM_KEY_PAGE_DOWN,
    PLATFORM_KEY_UP,
    PLATFORM_KEY_DOWN,
    PLATFORM_KEY_LEFT,
    PLATFORM_KEY_RIGHT,
    PLATFORM_KEY_LEFT_SHIFT,
    PLATFORM_KEY_RIGHT_SHIFT,
    PLATFORM_KEY_LEFT_CTRL,
    PLATFORM_KEY_RIGHT_CTRL,
    PLATFORM_KEY_LEFT_ALT,
    PLATFORM_KEY_RIGHT_ALT,
    PLATFORM_KEY_CAPS_LOCK,
    PLATFORM_KEY_GRAVE,
    PLATFORM_KEY_MINUS,
    PLATFORM_KEY_EQUALS,
    PLATFORM_KEY_LEFT_BRACKET,
    PLATFORM_KEY_RIGHT_BRACKET,
    PLATFORM_KEY_BACKSLASH,
    PLATFORM_KEY_SEMICOLON,
    PLATFORM_KEY_APOSTROPHE,
    PLATFORM_KEY_COMMA,
    PLATFORM_KEY_PERIOD,
    PLATFORM_KEY_SLASH,
    PLATFORM_KEY_NUMPAD_0,
    PLATFORM_KEY_NUMPAD_1,
    PLATFORM_KEY_NUMPAD_2,
    PLATFORM_KEY_NUMPAD_3,
    PLATFORM_KEY_NUMPAD_4,
    PLATFORM_KEY_NUMPAD_5,
    PLATFORM_KEY_NUMPAD_6,
    PLATFORM_KEY_NUMPAD_7,
    PLATFORM_KEY_NUMPAD_8,
    PLATFORM_KEY_NUMPAD_9,
    PLATFORM_KEY_NUMPAD_ADD,
    PLATFORM_KEY_NUMPAD_SUBTRACT,
    PLATFORM_KEY_NUMPAD_MULTIPLY,
    PLATFORM_KEY_NUMPAD_DIVIDE,
    PLATFORM_KEY_NUMPAD_DECIMAL,
    PLATFORM_KEY_NUMPAD_ENTER,
    PLATFORM_KEY_COUNT
} PlatformKey;

typedef enum {
    PLATFORM_MOUSE_BUTTON_LEFT,
    PLATFORM_MOUSE_BUTTON_MIDDLE,
    PLATFORM_MOUSE_BUTTON_RIGHT,
    PLATFORM_MOUSE_BUTTON_COUNT
} PlatformMouseButton;

// Gamepad buttons in a standard layout, whatever the controller. The face
// buttons are named by position: south is Xbox A, PlayStation cross.
typedef enum {
    PLATFORM_GAMEPAD_BUTTON_SOUTH,
    PLATFORM_GAMEPAD_BUTTON_EAST,
    PLATFORM_GAMEPAD_BUTTON_WEST,
    PLATFORM_GAMEPAD_BUTTON_NORTH,
    PLATFORM_GAMEPAD_BUTTON_LEFT_SHOULDER,
    PLATFORM_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    PLATFORM_GAMEPAD_BUTTON_LEFT_STICK,
    PLATFORM_GAMEPAD_BUTTON_RIGHT_STICK,
    PLATFORM_GAMEPAD_BUTTON_START,
    PLATFORM_GAMEPAD_BUTTON_BACK,
    PLATFORM_GAMEPAD_BUTTON_GUIDE,
    PLATFORM_GAMEPAD_BUTTON_DPAD_UP,
    PLATFORM_GAMEPAD_BUTTON_DPAD_DOWN,
    PLATFORM_GAMEPAD_BUTTON_DPAD_LEFT,
    PLATFORM_GAMEPAD_BUTTON_DPAD_RIGHT,
    PLATFORM_GAMEPAD_BUTTON_COUNT
} PlatformGamepadButton;

// Gamepad axes, raw with no deadzone. Sticks are -1 to 1 with positive y up,
// triggers 0 to 1.
typedef enum {
    PLATFORM_GAMEPAD_AXIS_LEFT_X,
    PLATFORM_GAMEPAD_AXIS_LEFT_Y,
    PLATFORM_GAMEPAD_AXIS_RIGHT_X,
    PLATFORM_GAMEPAD_AXIS_RIGHT_Y,
    PLATFORM_GAMEPAD_AXIS_LEFT_TRIGGER,
    PLATFORM_GAMEPAD_AXIS_RIGHT_TRIGGER,
    PLATFORM_GAMEPAD_AXIS_COUNT
} PlatformGamepadAxis;

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
    // Gamepad events carry the gamepad's slot index. A gamepad's axes are at
    // rest when it connects, unless axis events say otherwise right after.
    PLATFORM_EVENT_GAMEPAD_CONNECT,
    PLATFORM_EVENT_GAMEPAD_DISCONNECT,
    PLATFORM_EVENT_GAMEPAD_BUTTON_DOWN,
    PLATFORM_EVENT_GAMEPAD_BUTTON_UP,
    // An axis's new value
    PLATFORM_EVENT_GAMEPAD_AXIS,
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
        struct {
            i32                   index;
            PlatformGamepadButton button;
            PlatformGamepadAxis   axis;
            f32                   value;
        } gamepad;
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

void   platform_push_event(Platform* platform, PlatformEvent event);
// Display names, for logs and rebinding UIs
String platform_key_name(PlatformKey key);
String platform_mouse_button_name(PlatformMouseButton button);
String platform_gamepad_button_name(PlatformGamepadButton button);
String platform_gamepad_axis_name(PlatformGamepadAxis axis);

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

static char* platform_key_names[PLATFORM_KEY_COUNT] = {
    [PLATFORM_KEY_NONE] = "None",
    [PLATFORM_KEY_A] = "A",
    [PLATFORM_KEY_B] = "B",
    [PLATFORM_KEY_C] = "C",
    [PLATFORM_KEY_D] = "D",
    [PLATFORM_KEY_E] = "E",
    [PLATFORM_KEY_F] = "F",
    [PLATFORM_KEY_G] = "G",
    [PLATFORM_KEY_H] = "H",
    [PLATFORM_KEY_I] = "I",
    [PLATFORM_KEY_J] = "J",
    [PLATFORM_KEY_K] = "K",
    [PLATFORM_KEY_L] = "L",
    [PLATFORM_KEY_M] = "M",
    [PLATFORM_KEY_N] = "N",
    [PLATFORM_KEY_O] = "O",
    [PLATFORM_KEY_P] = "P",
    [PLATFORM_KEY_Q] = "Q",
    [PLATFORM_KEY_R] = "R",
    [PLATFORM_KEY_S] = "S",
    [PLATFORM_KEY_T] = "T",
    [PLATFORM_KEY_U] = "U",
    [PLATFORM_KEY_V] = "V",
    [PLATFORM_KEY_W] = "W",
    [PLATFORM_KEY_X] = "X",
    [PLATFORM_KEY_Y] = "Y",
    [PLATFORM_KEY_Z] = "Z",
    [PLATFORM_KEY_0] = "0",
    [PLATFORM_KEY_1] = "1",
    [PLATFORM_KEY_2] = "2",
    [PLATFORM_KEY_3] = "3",
    [PLATFORM_KEY_4] = "4",
    [PLATFORM_KEY_5] = "5",
    [PLATFORM_KEY_6] = "6",
    [PLATFORM_KEY_7] = "7",
    [PLATFORM_KEY_8] = "8",
    [PLATFORM_KEY_9] = "9",
    [PLATFORM_KEY_F1] = "F1",
    [PLATFORM_KEY_F2] = "F2",
    [PLATFORM_KEY_F3] = "F3",
    [PLATFORM_KEY_F4] = "F4",
    [PLATFORM_KEY_F5] = "F5",
    [PLATFORM_KEY_F6] = "F6",
    [PLATFORM_KEY_F7] = "F7",
    [PLATFORM_KEY_F8] = "F8",
    [PLATFORM_KEY_F9] = "F9",
    [PLATFORM_KEY_F10] = "F10",
    [PLATFORM_KEY_F11] = "F11",
    [PLATFORM_KEY_F12] = "F12",
    [PLATFORM_KEY_ESCAPE] = "Escape",
    [PLATFORM_KEY_TAB] = "Tab",
    [PLATFORM_KEY_SPACE] = "Space",
    [PLATFORM_KEY_ENTER] = "Enter",
    [PLATFORM_KEY_BACKSPACE] = "Backspace",
    [PLATFORM_KEY_DELETE] = "Delete",
    [PLATFORM_KEY_INSERT] = "Insert",
    [PLATFORM_KEY_HOME] = "Home",
    [PLATFORM_KEY_END] = "End",
    [PLATFORM_KEY_PAGE_UP] = "Page Up",
    [PLATFORM_KEY_PAGE_DOWN] = "Page Down",
    [PLATFORM_KEY_UP] = "Up",
    [PLATFORM_KEY_DOWN] = "Down",
    [PLATFORM_KEY_LEFT] = "Left",
    [PLATFORM_KEY_RIGHT] = "Right",
    [PLATFORM_KEY_LEFT_SHIFT] = "Left Shift",
    [PLATFORM_KEY_RIGHT_SHIFT] = "Right Shift",
    [PLATFORM_KEY_LEFT_CTRL] = "Left Ctrl",
    [PLATFORM_KEY_RIGHT_CTRL] = "Right Ctrl",
    [PLATFORM_KEY_LEFT_ALT] = "Left Alt",
    [PLATFORM_KEY_RIGHT_ALT] = "Right Alt",
    [PLATFORM_KEY_CAPS_LOCK] = "Caps Lock",
    [PLATFORM_KEY_GRAVE] = "`",
    [PLATFORM_KEY_MINUS] = "-",
    [PLATFORM_KEY_EQUALS] = "=",
    [PLATFORM_KEY_LEFT_BRACKET] = "[",
    [PLATFORM_KEY_RIGHT_BRACKET] = "]",
    [PLATFORM_KEY_BACKSLASH] = "\\",
    [PLATFORM_KEY_SEMICOLON] = ";",
    [PLATFORM_KEY_APOSTROPHE] = "'",
    [PLATFORM_KEY_COMMA] = ",",
    [PLATFORM_KEY_PERIOD] = ".",
    [PLATFORM_KEY_SLASH] = "/",
    [PLATFORM_KEY_NUMPAD_0] = "Numpad 0",
    [PLATFORM_KEY_NUMPAD_1] = "Numpad 1",
    [PLATFORM_KEY_NUMPAD_2] = "Numpad 2",
    [PLATFORM_KEY_NUMPAD_3] = "Numpad 3",
    [PLATFORM_KEY_NUMPAD_4] = "Numpad 4",
    [PLATFORM_KEY_NUMPAD_5] = "Numpad 5",
    [PLATFORM_KEY_NUMPAD_6] = "Numpad 6",
    [PLATFORM_KEY_NUMPAD_7] = "Numpad 7",
    [PLATFORM_KEY_NUMPAD_8] = "Numpad 8",
    [PLATFORM_KEY_NUMPAD_9] = "Numpad 9",
    [PLATFORM_KEY_NUMPAD_ADD] = "Numpad +",
    [PLATFORM_KEY_NUMPAD_SUBTRACT] = "Numpad -",
    [PLATFORM_KEY_NUMPAD_MULTIPLY] = "Numpad *",
    [PLATFORM_KEY_NUMPAD_DIVIDE] = "Numpad /",
    [PLATFORM_KEY_NUMPAD_DECIMAL] = "Numpad .",
    [PLATFORM_KEY_NUMPAD_ENTER] = "Numpad Enter",
};

static char* platform_mouse_button_names[PLATFORM_MOUSE_BUTTON_COUNT] = {
    [PLATFORM_MOUSE_BUTTON_LEFT]   = "Mouse Left",
    [PLATFORM_MOUSE_BUTTON_MIDDLE] = "Mouse Middle",
    [PLATFORM_MOUSE_BUTTON_RIGHT]  = "Mouse Right",
};

static char* platform_gamepad_button_names[PLATFORM_GAMEPAD_BUTTON_COUNT] = {
    [PLATFORM_GAMEPAD_BUTTON_SOUTH]          = "Pad South",
    [PLATFORM_GAMEPAD_BUTTON_EAST]           = "Pad East",
    [PLATFORM_GAMEPAD_BUTTON_WEST]           = "Pad West",
    [PLATFORM_GAMEPAD_BUTTON_NORTH]          = "Pad North",
    [PLATFORM_GAMEPAD_BUTTON_LEFT_SHOULDER]  = "Pad Left Shoulder",
    [PLATFORM_GAMEPAD_BUTTON_RIGHT_SHOULDER] = "Pad Right Shoulder",
    [PLATFORM_GAMEPAD_BUTTON_LEFT_STICK]     = "Pad Left Stick Click",
    [PLATFORM_GAMEPAD_BUTTON_RIGHT_STICK]    = "Pad Right Stick Click",
    [PLATFORM_GAMEPAD_BUTTON_START]          = "Pad Start",
    [PLATFORM_GAMEPAD_BUTTON_BACK]           = "Pad Back",
    [PLATFORM_GAMEPAD_BUTTON_GUIDE]          = "Pad Guide",
    [PLATFORM_GAMEPAD_BUTTON_DPAD_UP]        = "Pad Up",
    [PLATFORM_GAMEPAD_BUTTON_DPAD_DOWN]      = "Pad Down",
    [PLATFORM_GAMEPAD_BUTTON_DPAD_LEFT]      = "Pad Left",
    [PLATFORM_GAMEPAD_BUTTON_DPAD_RIGHT]     = "Pad Right",
};

static char* platform_gamepad_axis_names[PLATFORM_GAMEPAD_AXIS_COUNT] = {
    [PLATFORM_GAMEPAD_AXIS_LEFT_X]        = "Pad Left Stick X",
    [PLATFORM_GAMEPAD_AXIS_LEFT_Y]        = "Pad Left Stick Y",
    [PLATFORM_GAMEPAD_AXIS_RIGHT_X]       = "Pad Right Stick X",
    [PLATFORM_GAMEPAD_AXIS_RIGHT_Y]       = "Pad Right Stick Y",
    [PLATFORM_GAMEPAD_AXIS_LEFT_TRIGGER]  = "Pad Left Trigger",
    [PLATFORM_GAMEPAD_AXIS_RIGHT_TRIGGER] = "Pad Right Trigger",
};

String platform_key_name(PlatformKey key) {
    if((u32)key >= PLATFORM_KEY_COUNT || platform_key_names[key] == NULL) return string_const("Unknown");
    return string_const(platform_key_names[key]);
}

String platform_mouse_button_name(PlatformMouseButton button) {
    if((u32)button >= PLATFORM_MOUSE_BUTTON_COUNT) return string_const("Unknown");
    return string_const(platform_mouse_button_names[button]);
}

String platform_gamepad_button_name(PlatformGamepadButton button) {
    if((u32)button >= PLATFORM_GAMEPAD_BUTTON_COUNT) return string_const("Unknown");
    return string_const(platform_gamepad_button_names[button]);
}

String platform_gamepad_axis_name(PlatformGamepadAxis axis) {
    if((u32)axis >= PLATFORM_GAMEPAD_AXIS_COUNT) return string_const("Unknown");
    return string_const(platform_gamepad_axis_names[axis]);
}

#endif
