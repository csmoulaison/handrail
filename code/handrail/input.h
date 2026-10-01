#ifndef handrail_input_h_INCLUDED
#define handrail_input_h_INCLUDED

// Game-defined input actions, evaluated from platform events. The game names
// its actions (indices below INPUT_MAX_ACTIONS), gives each a kind, and binds
// platform sources to them in an InputMap. Rebinding is editing the map.
//
// Each device evaluates every action on its own: device 0 is the keyboard and
// mouse, device 1 + i is gamepad slot i. The game decides how devices map to
// players; input_any merges them all, for single player.

#ifndef INPUT_MAX_ACTIONS
#define INPUT_MAX_ACTIONS 64
#endif

#ifndef INPUT_MAX_BINDINGS_PER_ACTION
#define INPUT_MAX_BINDINGS_PER_ACTION 16
#endif

#ifndef INPUT_DEFAULT_STICK_DEADZONE
#define INPUT_DEFAULT_STICK_DEADZONE 0.2f
#endif

#ifndef INPUT_DEFAULT_TRIGGER_DEADZONE
#define INPUT_DEFAULT_TRIGGER_DEADZONE 0.1f
#endif

#define INPUT_DEVICE_KEYBOARD_MOUSE 0
#define INPUT_DEVICES_LEN           (1 + PLATFORM_MAX_GAMEPADS)

typedef enum {
    // Held while its value.x is at least 0.5
    INPUT_ACTION_BUTTON,
    // value.x, -1 to 1
    INPUT_ACTION_AXIS1,
    // value, length at most 1
    INPUT_ACTION_AXIS2,
} InputActionKind;

typedef enum {
    INPUT_SOURCE_NONE,
    INPUT_SOURCE_KEY,            // code is a PlatformKey
    INPUT_SOURCE_MOUSE_BUTTON,   // code is a PlatformMouseButton
    INPUT_SOURCE_GAMEPAD_BUTTON, // code is a PlatformGamepadButton
    INPUT_SOURCE_GAMEPAD_AXIS,   // code is a PlatformGamepadAxis
    INPUT_SOURCE_GAMEPAD_STICK,  // code is 0 for the left stick, 1 for the right. Both axes, with a radial deadzone.
} InputSourceType;

// A source's value is 0 or 1 for keys and buttons, the deadzoned value for an
// axis, and a v2 for a stick. A binding adds value * scale to its action, so a
// key on a 2D move action gives its direction as the scale, e.g. (1, 0) for D,
// and a stick axis bound to a button with scale -1 presses it when pushed negative.
typedef struct {
    InputSourceType type;
    u32             code;
    v2              scale;
} InputBinding;

typedef struct {
    InputActionKind kind;
    InputBinding    bindings[INPUT_MAX_BINDINGS_PER_ACTION];
    i32             bindings_len;
} InputAction;

typedef struct {
    InputAction actions[INPUT_MAX_ACTIONS];
    // One past the highest action bound
    i32         actions_len;
    // Axis magnitudes up to the deadzone read as 0, and the rest is rescaled to start from 0
    f32         stick_deadzone;
    f32         trigger_deadzone;
} InputMap;

typedef struct {
    bool held;
    // Since the last input_update. A tap within one frame sets both.
    bool pressed;
    bool released;
    v2   value;
} InputActionState;

typedef struct {
    bool             connected;
    // Raw source state, kept up to date from events
    bool             keys[PLATFORM_KEY_COUNT];
    bool             mouse_buttons[PLATFORM_MOUSE_BUTTON_COUNT];
    bool             gamepad_buttons[PLATFORM_GAMEPAD_BUTTON_COUNT];
    f32              gamepad_axes[PLATFORM_GAMEPAD_AXIS_COUNT];
    // Indexed by action
    InputActionState actions[INPUT_MAX_ACTIONS];
} InputDevice;

typedef struct {
    InputDevice devices[INPUT_DEVICES_LEN];
} Input;

// Clear a map and set the default deadzones
void             input_map_init(InputMap* map);
// Add a binding to an action, and set the action's kind
void             input_map_bind(InputMap* map, i32 action, InputActionKind kind, InputBinding binding);
// Apply a frame's platform events to the devices' raw state and re-evaluate
// their actions after each one, so edges within the frame aren't lost
void             input_update(Input* input, InputMap* map, PlatformEvent* events, i32 events_len);
// An action merged over every connected device: flags are OR'd, and the value
// of largest magnitude wins
InputActionState input_any(Input* input, i32 action);
// Accumulate frame states into latched ones, for a fixed-timestep simulation
// that may run no step this frame: edges are OR'd, held and value replaced.
// Clear the edges with input_clear_edges once a step has used them.
void             input_latch(InputActionState* latched, InputActionState* frame, i32 states_len);
void             input_clear_edges(InputActionState* states, i32 states_len);
// The binding an event would make when rebinding: a key, mouse or gamepad
// button press, or a gamepad axis pushed past half way (scaled by its sign).
// False for any other event.
bool             input_binding_from_event(PlatformEvent* event, InputBinding* out_binding);
// The bound source's display name
String           input_binding_name(InputBinding binding);

#endif

#if defined(HANDRAIL_IMPLEMENTATION_PASS) && !defined(handrail_input_h_IMPLEMENTED)
#define handrail_input_h_IMPLEMENTED

void input_map_init(InputMap* map) {
    memset(map, 0, sizeof(InputMap));
    map->stick_deadzone = INPUT_DEFAULT_STICK_DEADZONE;
    map->trigger_deadzone = INPUT_DEFAULT_TRIGGER_DEADZONE;
}

void input_map_bind(InputMap* map, i32 action, InputActionKind kind, InputBinding binding) {
    assert(action >= 0 && action < INPUT_MAX_ACTIONS);
    InputAction* bound = &map->actions[action];
    assert(bound->bindings_len < INPUT_MAX_BINDINGS_PER_ACTION);
    bound->kind = kind;
    bound->bindings[bound->bindings_len] = binding;
    bound->bindings_len++;
    map->actions_len = action + 1 > map->actions_len ? action + 1 : map->actions_len;
}

// Re-evaluate every action from the device's raw state, and set edges where held changed
static void input_device_evaluate(InputDevice* device, InputMap* map) {
    for(i32 a = 0; a < map->actions_len; a++) {
        InputAction* action = &map->actions[a];

        // Sum the bindings' contributions
        v2 value = v2_zero();
        for(i32 b = 0; b < action->bindings_len; b++) {
            InputBinding* binding = &action->bindings[b];
            v2 source = v2_zero();
            switch(binding->type) {
                case INPUT_SOURCE_KEY: {
                    if(binding->code < PLATFORM_KEY_COUNT && device->keys[binding->code]) source = v2_new(1.0f, 1.0f);
                } break;
                case INPUT_SOURCE_MOUSE_BUTTON: {
                    if(binding->code < PLATFORM_MOUSE_BUTTON_COUNT && device->mouse_buttons[binding->code]) source = v2_new(1.0f, 1.0f);
                } break;
                case INPUT_SOURCE_GAMEPAD_BUTTON: {
                    if(binding->code < PLATFORM_GAMEPAD_BUTTON_COUNT && device->gamepad_buttons[binding->code]) source = v2_new(1.0f, 1.0f);
                } break;
                case INPUT_SOURCE_GAMEPAD_AXIS: {
                    if(binding->code >= PLATFORM_GAMEPAD_AXIS_COUNT) break;
                    f32 axis = device->gamepad_axes[binding->code];
                    bool trigger = binding->code == PLATFORM_GAMEPAD_AXIS_LEFT_TRIGGER || binding->code == PLATFORM_GAMEPAD_AXIS_RIGHT_TRIGGER;
                    f32 deadzone = trigger ? map->trigger_deadzone : map->stick_deadzone;
                    f32 magnitude = fabsf(axis);
                    if(magnitude > deadzone) {
                        f32 scaled = (magnitude - deadzone) / (1.0f - deadzone);
                        source = v2_new(copysignf(scaled, axis), copysignf(scaled, axis));
                    }
                } break;
                case INPUT_SOURCE_GAMEPAD_STICK: {
                    if(binding->code > 1) break;
                    v2 stick = binding->code == 0
                        ? v2_new(device->gamepad_axes[PLATFORM_GAMEPAD_AXIS_LEFT_X], device->gamepad_axes[PLATFORM_GAMEPAD_AXIS_LEFT_Y])
                        : v2_new(device->gamepad_axes[PLATFORM_GAMEPAD_AXIS_RIGHT_X], device->gamepad_axes[PLATFORM_GAMEPAD_AXIS_RIGHT_Y]);
                    f32 magnitude = v2_magnitude(stick);
                    if(magnitude > map->stick_deadzone) {
                        f32 scaled = f32_min((magnitude - map->stick_deadzone) / (1.0f - map->stick_deadzone), 1.0f);
                        source = v2_scale(stick, scaled / magnitude);
                    }
                } break;
                default: break;
            }
            value = v2_add(value, v2_mult(source, binding->scale));
        }

        // Clamp to the kind's range and decide whether it's held
        bool held = false;
        switch(action->kind) {
            case INPUT_ACTION_BUTTON: {
                value = v2_new(f32_clamp(value.x, -1.0f, 1.0f), 0.0f);
                held = value.x >= 0.5f;
            } break;
            case INPUT_ACTION_AXIS1: {
                value = v2_new(f32_clamp(value.x, -1.0f, 1.0f), 0.0f);
                held = fabsf(value.x) >= 0.5f;
            } break;
            case INPUT_ACTION_AXIS2: {
                f32 magnitude = v2_magnitude(value);
                if(magnitude > 1.0f) value = v2_scale(value, 1.0f / magnitude);
                held = magnitude >= 0.5f;
            } break;
        }

        InputActionState* state = &device->actions[a];
        state->pressed  |= held && !state->held;
        state->released |= !held && state->held;
        state->held = held;
        state->value = value;
    }
}

void input_update(Input* input, InputMap* map, PlatformEvent* events, i32 events_len) {
    input->devices[INPUT_DEVICE_KEYBOARD_MOUSE].connected = true;
    for(i32 d = 0; d < INPUT_DEVICES_LEN; d++) {
        input_clear_edges(input->devices[d].actions, INPUT_MAX_ACTIONS);
    }

    for(i32 i = 0; i < events_len; i++) {
        PlatformEvent* event = &events[i];
        InputDevice* device = &input->devices[INPUT_DEVICE_KEYBOARD_MOUSE];
        bool gamepad_event = event->type >= PLATFORM_EVENT_GAMEPAD_CONNECT && event->type <= PLATFORM_EVENT_GAMEPAD_AXIS;
        if(gamepad_event) {
            if(event->gamepad.index < 0 || event->gamepad.index >= PLATFORM_MAX_GAMEPADS) continue;
            device = &input->devices[1 + event->gamepad.index];
        }

        // Update the raw state. Connecting or disconnecting starts from rest, so
        // anything held is released.
        switch(event->type) {
            case PLATFORM_EVENT_KEYDOWN:
            case PLATFORM_EVENT_KEYUP: {
                if((u32)event->key < PLATFORM_KEY_COUNT) device->keys[event->key] = event->type == PLATFORM_EVENT_KEYDOWN;
            } break;
            case PLATFORM_EVENT_MOUSE_DOWN:
            case PLATFORM_EVENT_MOUSE_UP: {
                if((u32)event->mouse.button < PLATFORM_MOUSE_BUTTON_COUNT) device->mouse_buttons[event->mouse.button] = event->type == PLATFORM_EVENT_MOUSE_DOWN;
            } break;
            case PLATFORM_EVENT_DEFOCUS: {
                memset(device->keys, 0, sizeof(device->keys));
                memset(device->mouse_buttons, 0, sizeof(device->mouse_buttons));
            } break;
            case PLATFORM_EVENT_GAMEPAD_CONNECT:
            case PLATFORM_EVENT_GAMEPAD_DISCONNECT: {
                memset(device->gamepad_buttons, 0, sizeof(device->gamepad_buttons));
                memset(device->gamepad_axes, 0, sizeof(device->gamepad_axes));
            } break;
            case PLATFORM_EVENT_GAMEPAD_BUTTON_DOWN:
            case PLATFORM_EVENT_GAMEPAD_BUTTON_UP: {
                if((u32)event->gamepad.button < PLATFORM_GAMEPAD_BUTTON_COUNT) device->gamepad_buttons[event->gamepad.button] = event->type == PLATFORM_EVENT_GAMEPAD_BUTTON_DOWN;
            } break;
            case PLATFORM_EVENT_GAMEPAD_AXIS: {
                if((u32)event->gamepad.axis < PLATFORM_GAMEPAD_AXIS_COUNT) device->gamepad_axes[event->gamepad.axis] = event->gamepad.value;
            } break;
            default: {
                continue;
            } break;
        }
        input_device_evaluate(device, map);
        if(event->type == PLATFORM_EVENT_GAMEPAD_CONNECT) device->connected = true;
        if(event->type == PLATFORM_EVENT_GAMEPAD_DISCONNECT) device->connected = false;
    }

    // Once more for every device, so map changes take effect without waiting for an event
    for(i32 d = 0; d < INPUT_DEVICES_LEN; d++) {
        input_device_evaluate(&input->devices[d], map);
    }
}

InputActionState input_any(Input* input, i32 action) {
    assert(action >= 0 && action < INPUT_MAX_ACTIONS);
    InputActionState merged = {};
    for(i32 d = 0; d < INPUT_DEVICES_LEN; d++) {
        if(!input->devices[d].connected) continue;
        InputActionState* state = &input->devices[d].actions[action];
        merged.held     |= state->held;
        merged.pressed  |= state->pressed;
        merged.released |= state->released;
        if(v2_magnitude(state->value) > v2_magnitude(merged.value)) merged.value = state->value;
    }
    return merged;
}

void input_latch(InputActionState* latched, InputActionState* frame, i32 states_len) {
    for(i32 i = 0; i < states_len; i++) {
        latched[i].pressed  |= frame[i].pressed;
        latched[i].released |= frame[i].released;
        latched[i].held = frame[i].held;
        latched[i].value = frame[i].value;
    }
}

void input_clear_edges(InputActionState* states, i32 states_len) {
    for(i32 i = 0; i < states_len; i++) {
        states[i].pressed = false;
        states[i].released = false;
    }
}

bool input_binding_from_event(PlatformEvent* event, InputBinding* out_binding) {
    switch(event->type) {
        case PLATFORM_EVENT_KEYDOWN: {
            if(event->key == PLATFORM_KEY_NONE) return false;
            *out_binding = (InputBinding){ INPUT_SOURCE_KEY, event->key, v2_new(1.0f, 1.0f) };
            return true;
        } break;
        case PLATFORM_EVENT_MOUSE_DOWN: {
            *out_binding = (InputBinding){ INPUT_SOURCE_MOUSE_BUTTON, event->mouse.button, v2_new(1.0f, 1.0f) };
            return true;
        } break;
        case PLATFORM_EVENT_GAMEPAD_BUTTON_DOWN: {
            *out_binding = (InputBinding){ INPUT_SOURCE_GAMEPAD_BUTTON, event->gamepad.button, v2_new(1.0f, 1.0f) };
            return true;
        } break;
        case PLATFORM_EVENT_GAMEPAD_AXIS: {
            if(fabsf(event->gamepad.value) < 0.5f) return false;
            f32 sign = event->gamepad.value > 0.0f ? 1.0f : -1.0f;
            *out_binding = (InputBinding){ INPUT_SOURCE_GAMEPAD_AXIS, event->gamepad.axis, v2_new(sign, sign) };
            return true;
        } break;
        default: break;
    }
    return false;
}

String input_binding_name(InputBinding binding) {
    switch(binding.type) {
        case INPUT_SOURCE_KEY:            return platform_key_name((PlatformKey)binding.code);
        case INPUT_SOURCE_MOUSE_BUTTON:   return platform_mouse_button_name((PlatformMouseButton)binding.code);
        case INPUT_SOURCE_GAMEPAD_BUTTON: return platform_gamepad_button_name((PlatformGamepadButton)binding.code);
        case INPUT_SOURCE_GAMEPAD_AXIS:   return platform_gamepad_axis_name((PlatformGamepadAxis)binding.code);
        case INPUT_SOURCE_GAMEPAD_STICK:  return string_const(binding.code == 0 ? "Pad Left Stick" : "Pad Right Stick");
        default:                          return string_const("None");
    }
}

#endif
