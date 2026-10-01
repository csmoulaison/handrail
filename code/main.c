// config.h is provided by the specific game
#include "config.h"

#define HANDRAIL_IMPLEMENTATION
#define HANDRAIL_INCLUDE_VK
#include "handrail/core.h"

#include "generated/asset_data.c"

// Memory sizes. Render frame memory is not here: it is GPU-visible memory
// owned by the Vulkan backend.
#ifndef GAME_STACK_SIZE
#define GAME_STACK_SIZE (MEGABYTE * 4)
#endif
#ifndef RENDER_STACK_SIZE
#define RENDER_STACK_SIZE (MEGABYTE * 4)
#endif
#ifndef PLATFORM_FRAME_STACK_SIZE
#define PLATFORM_FRAME_STACK_SIZE (MEGABYTE * 4)
#endif

#define ROOT_MEMORY_SIZE (sizeof(Context) + GAME_STACK_SIZE + RENDER_STACK_SIZE + PLATFORM_FRAME_STACK_SIZE)

// Audio settings. The format itself (AUDIO_SAMPLE_RATE, AUDIO_CHANNEL_COUNT) is
// in game.h, since the game writes it.
#define AUDIO_BYTES_PER_FRAME (sizeof(f32) * AUDIO_CHANNEL_COUNT)
// How far ahead of playback the main loop keeps the game's audio
#ifndef AUDIO_LATENCY_FRAMES
#define AUDIO_LATENCY_FRAMES (AUDIO_SAMPLE_RATE / 12)
#endif

// The game library and the functions loaded from it
typedef struct {
    DynamicLibrary      library;
    GameInitFunction*   init;
    GameUpdateFunction* update;
    GameAudioCallback*  audio_callback;
} GameLibrary;

void game_library_update(GameLibrary* game) {
    if(dynamic_library_update(&game->library)) {
        log_print(LOG_HOT_RELOAD, "Loaded game library " STRING_FMT, STRING_ARG(game->library.path));
        game->init           = dynamic_library_load_function(game->library, string_const("game_init"));
        game->update         = dynamic_library_load_function(game->library, string_const("game_update"));
        game->audio_callback = dynamic_library_load_function(game->library, string_const("game_audio_callback"));
    }
}

// WINDOWS
#if PLATFORM == PLATFORM_WINDOWS

#define COBJMACROS
#include <objbase.h>
#include <windowsx.h>
#include <avrt.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <xinput.h>

// Defined here rather than linked from uuid.lib, so their names can't collide with the SDK's
static const GUID WASAPI_CLSID_MM_DEVICE_ENUMERATOR = { 0xbcde0395, 0xe52f, 0x467c, { 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e } };
static const GUID WASAPI_IID_MM_DEVICE_ENUMERATOR   = { 0xa95664d2, 0x9614, 0x4f35, { 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6 } };
static const GUID WASAPI_IID_AUDIO_CLIENT           = { 0x1cb9ad4c, 0xdbfa, 0x4c32, { 0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2 } };
static const GUID WASAPI_IID_AUDIO_CLIENT3          = { 0x7ed4ee07, 0x8e67, 0x4cd4, { 0x8c, 0x1a, 0x2b, 0x7a, 0x59, 0x87, 0xad, 0x42 } };
static const GUID WASAPI_IID_AUDIO_RENDER_CLIENT    = { 0xf294acfc, 0x3146, 0x4483, { 0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2 } };
static const GUID WASAPI_SUBTYPE_IEEE_FLOAT         = { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

#define HR_VERIFY(statement) { \
    HRESULT hr_result = (statement); \
    if(FAILED(hr_result)) { \
        char hr_message[512] = {}; \
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, hr_result, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), hr_message, sizeof(hr_message), NULL); \
        log_exit("HRESULT 0x%08lx: %s: %s", (unsigned long)hr_result, #statement, hr_message); \
    } \
}

// The main thread writes game audio into a ring buffer and the audio thread
// copies it out to WASAPI. The ring is mapped twice back to back in virtual
// memory, so a read or write of up to ring_size bytes starting at any offset is
// contiguous. The offsets count bytes written and read since startup and wrap
// around; only their difference and their value masked by ring_size - 1 matter.
// Each offset has one writer, so no lock is needed. The audio thread doesn't
// log, so it counts trouble for the main loop to report.
typedef struct {
    IAudioClient* client;
    HANDLE        event;
    HANDLE        thread;
    volatile LONG stop;
    u8*           ring;
    u32           ring_size;
    volatile LONG read_offset;
    volatile LONG write_offset;
    volatile LONG underrun_count;
    volatile LONG priority_failed;
} Wasapi;

// A gamepad slot's state as of the last poll, which the next poll diffs against
typedef struct {
    bool         connected;
    XINPUT_STATE state;
} XinputGamepad;

typedef struct {
    bool          close_requested;
    Log           log;
    Profile       profile;
    BufferTracker buffers;
    HWND          hwnd;
    Platform      platform;
    GameLibrary   game;
    Wasapi        audio;
    XinputGamepad gamepads[PLATFORM_MAX_GAMEPADS];
    u64           gamepad_scan_ns;
} Context;

// Left and right modifiers share a virtual key. The extended-key bit (24) of
// lparam marks the right ctrl and alt, and numpad enter; the scan code in bits
// 16-23 tells the shifts apart.
PlatformKey platform_key_from_win32(WPARAM virtual_key, LPARAM lparam) {
    bool extended = (lparam & (1 << 24)) != 0;
    if(virtual_key >= 'A' && virtual_key <= 'Z') return (PlatformKey)(PLATFORM_KEY_A + (virtual_key - 'A'));
    if(virtual_key >= '0' && virtual_key <= '9') return (PlatformKey)(PLATFORM_KEY_0 + (virtual_key - '0'));
    if(virtual_key >= VK_F1 && virtual_key <= VK_F12) return (PlatformKey)(PLATFORM_KEY_F1 + (virtual_key - VK_F1));
    if(virtual_key >= VK_NUMPAD0 && virtual_key <= VK_NUMPAD9) return (PlatformKey)(PLATFORM_KEY_NUMPAD_0 + (virtual_key - VK_NUMPAD0));
    switch(virtual_key) {
        case VK_ESCAPE:     return PLATFORM_KEY_ESCAPE;
        case VK_TAB:        return PLATFORM_KEY_TAB;
        case VK_SPACE:      return PLATFORM_KEY_SPACE;
        case VK_RETURN:     return extended ? PLATFORM_KEY_NUMPAD_ENTER : PLATFORM_KEY_ENTER;
        case VK_BACK:       return PLATFORM_KEY_BACKSPACE;
        case VK_DELETE:     return PLATFORM_KEY_DELETE;
        case VK_INSERT:     return PLATFORM_KEY_INSERT;
        case VK_HOME:       return PLATFORM_KEY_HOME;
        case VK_END:        return PLATFORM_KEY_END;
        case VK_PRIOR:      return PLATFORM_KEY_PAGE_UP;
        case VK_NEXT:       return PLATFORM_KEY_PAGE_DOWN;
        case VK_UP:         return PLATFORM_KEY_UP;
        case VK_DOWN:       return PLATFORM_KEY_DOWN;
        case VK_LEFT:       return PLATFORM_KEY_LEFT;
        case VK_RIGHT:      return PLATFORM_KEY_RIGHT;
        case VK_SHIFT:      return MapVirtualKeyA((lparam >> 16) & 0xff, MAPVK_VSC_TO_VK_EX) == VK_RSHIFT ? PLATFORM_KEY_RIGHT_SHIFT : PLATFORM_KEY_LEFT_SHIFT;
        case VK_CONTROL:    return extended ? PLATFORM_KEY_RIGHT_CTRL : PLATFORM_KEY_LEFT_CTRL;
        case VK_MENU:       return extended ? PLATFORM_KEY_RIGHT_ALT : PLATFORM_KEY_LEFT_ALT;
        case VK_CAPITAL:    return PLATFORM_KEY_CAPS_LOCK;
        case VK_OEM_3:      return PLATFORM_KEY_GRAVE;
        case VK_OEM_MINUS:  return PLATFORM_KEY_MINUS;
        case VK_OEM_PLUS:   return PLATFORM_KEY_EQUALS;
        case VK_OEM_4:      return PLATFORM_KEY_LEFT_BRACKET;
        case VK_OEM_6:      return PLATFORM_KEY_RIGHT_BRACKET;
        case VK_OEM_5:      return PLATFORM_KEY_BACKSLASH;
        case VK_OEM_1:      return PLATFORM_KEY_SEMICOLON;
        case VK_OEM_7:      return PLATFORM_KEY_APOSTROPHE;
        case VK_OEM_COMMA:  return PLATFORM_KEY_COMMA;
        case VK_OEM_PERIOD: return PLATFORM_KEY_PERIOD;
        case VK_OEM_2:      return PLATFORM_KEY_SLASH;
        case VK_ADD:        return PLATFORM_KEY_NUMPAD_ADD;
        case VK_SUBTRACT:   return PLATFORM_KEY_NUMPAD_SUBTRACT;
        case VK_MULTIPLY:   return PLATFORM_KEY_NUMPAD_MULTIPLY;
        case VK_DIVIDE:     return PLATFORM_KEY_NUMPAD_DIVIDE;
        case VK_DECIMAL:    return PLATFORM_KEY_NUMPAD_DECIMAL;
        default:            return PLATFORM_KEY_NONE;
    }
}

// Modifier keys held as of the message being processed
u32 platform_modifiers_from_win32() {
    u32 modifiers = PLATFORM_MODIFIER_NONE;
    if(GetKeyState(VK_SHIFT)   & 0x8000) modifiers |= PLATFORM_MODIFIER_SHIFT;
    if(GetKeyState(VK_CONTROL) & 0x8000) modifiers |= PLATFORM_MODIFIER_CTRL;
    if(GetKeyState(VK_MENU)    & 0x8000) modifiers |= PLATFORM_MODIFIER_ALT;
    return modifiers;
}

// A gamepad's axes as platform values, indexed by PlatformGamepadAxis
void xinput_axes(XINPUT_GAMEPAD* gamepad, f32* out_axes) {
    out_axes[PLATFORM_GAMEPAD_AXIS_LEFT_X]        = f32_max((f32)gamepad->sThumbLX / 32767.0f, -1.0f);
    out_axes[PLATFORM_GAMEPAD_AXIS_LEFT_Y]        = f32_max((f32)gamepad->sThumbLY / 32767.0f, -1.0f);
    out_axes[PLATFORM_GAMEPAD_AXIS_RIGHT_X]       = f32_max((f32)gamepad->sThumbRX / 32767.0f, -1.0f);
    out_axes[PLATFORM_GAMEPAD_AXIS_RIGHT_Y]       = f32_max((f32)gamepad->sThumbRY / 32767.0f, -1.0f);
    out_axes[PLATFORM_GAMEPAD_AXIS_LEFT_TRIGGER]  = (f32)gamepad->bLeftTrigger / 255.0f;
    out_axes[PLATFORM_GAMEPAD_AXIS_RIGHT_TRIGGER] = (f32)gamepad->bRightTrigger / 255.0f;
}

// Poll XInput and push what changed since the last poll as events. Checking an
// empty slot is slow, so empty slots are only checked about once a second.
void xinput_poll(Context* context) {
    static const struct { WORD mask; PlatformGamepadButton button; } button_map[] = {
        { XINPUT_GAMEPAD_A,              PLATFORM_GAMEPAD_BUTTON_SOUTH },
        { XINPUT_GAMEPAD_B,              PLATFORM_GAMEPAD_BUTTON_EAST },
        { XINPUT_GAMEPAD_X,              PLATFORM_GAMEPAD_BUTTON_WEST },
        { XINPUT_GAMEPAD_Y,              PLATFORM_GAMEPAD_BUTTON_NORTH },
        { XINPUT_GAMEPAD_LEFT_SHOULDER,  PLATFORM_GAMEPAD_BUTTON_LEFT_SHOULDER },
        { XINPUT_GAMEPAD_RIGHT_SHOULDER, PLATFORM_GAMEPAD_BUTTON_RIGHT_SHOULDER },
        { XINPUT_GAMEPAD_LEFT_THUMB,     PLATFORM_GAMEPAD_BUTTON_LEFT_STICK },
        { XINPUT_GAMEPAD_RIGHT_THUMB,    PLATFORM_GAMEPAD_BUTTON_RIGHT_STICK },
        { XINPUT_GAMEPAD_START,          PLATFORM_GAMEPAD_BUTTON_START },
        { XINPUT_GAMEPAD_BACK,           PLATFORM_GAMEPAD_BUTTON_BACK },
        { XINPUT_GAMEPAD_DPAD_UP,        PLATFORM_GAMEPAD_BUTTON_DPAD_UP },
        { XINPUT_GAMEPAD_DPAD_DOWN,      PLATFORM_GAMEPAD_BUTTON_DPAD_DOWN },
        { XINPUT_GAMEPAD_DPAD_LEFT,      PLATFORM_GAMEPAD_BUTTON_DPAD_LEFT },
        { XINPUT_GAMEPAD_DPAD_RIGHT,     PLATFORM_GAMEPAD_BUTTON_DPAD_RIGHT },
    };
    u64 now = profile_time_ns();
    bool scan = now - context->gamepad_scan_ns >= 1000000000ull;
    if(scan) {
        context->gamepad_scan_ns = now;
    }
    i32 slots_len = min(PLATFORM_MAX_GAMEPADS, XUSER_MAX_COUNT);
    for(i32 i = 0; i < slots_len; i++) {
        XinputGamepad* gamepad = &context->gamepads[i];
        if(!gamepad->connected && !scan) {
            continue;
        }
        XINPUT_STATE state = {};
        bool connected = XInputGetState(i, &state) == ERROR_SUCCESS;
        if(!connected) {
            if(gamepad->connected) {
                log_print(LOG_PLATFORM, "Gamepad %i disconnected", i);
                platform_push_event(&context->platform, (PlatformEvent){ .type = PLATFORM_EVENT_GAMEPAD_DISCONNECT, .gamepad = { .index = i } });
            }
            gamepad->connected = false;
            continue;
        }

        // Diff a new gamepad against a resting one, so held buttons and moved axes arrive as events
        if(!gamepad->connected) {
            log_print(LOG_PLATFORM, "Gamepad %i connected (XInput)", i);
            platform_push_event(&context->platform, (PlatformEvent){ .type = PLATFORM_EVENT_GAMEPAD_CONNECT, .gamepad = { .index = i } });
            gamepad->connected = true;
            gamepad->state = (XINPUT_STATE){};
        }

        // Buttons
        WORD changed = state.Gamepad.wButtons ^ gamepad->state.Gamepad.wButtons;
        for(i32 b = 0; b < (i32)(sizeof(button_map) / sizeof(button_map[0])); b++) {
            if(changed & button_map[b].mask) {
                bool down = (state.Gamepad.wButtons & button_map[b].mask) != 0;
                platform_push_event(&context->platform, (PlatformEvent){
                    .type = down ? PLATFORM_EVENT_GAMEPAD_BUTTON_DOWN : PLATFORM_EVENT_GAMEPAD_BUTTON_UP,
                    .gamepad = { .index = i, .button = button_map[b].button } });
            }
        }

        // Axes
        f32 axes[PLATFORM_GAMEPAD_AXIS_COUNT];
        f32 previous_axes[PLATFORM_GAMEPAD_AXIS_COUNT];
        xinput_axes(&state.Gamepad, axes);
        xinput_axes(&gamepad->state.Gamepad, previous_axes);
        for(i32 a = 0; a < PLATFORM_GAMEPAD_AXIS_COUNT; a++) {
            if(axes[a] != previous_axes[a]) {
                platform_push_event(&context->platform, (PlatformEvent){
                    .type = PLATFORM_EVENT_GAMEPAD_AXIS, .gamepad = { .index = i, .axis = (PlatformGamepadAxis)a, .value = axes[a] } });
            }
        }
        gamepad->state = state;
    }
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    // The context is passed to CreateWindowEx and kept in the window's user data.
    // Messages that arrive before WM_CREATE have no context.
    Context* context = NULL;
    if(message == WM_CREATE) {
        CREATESTRUCTA* create = (CREATESTRUCTA*)lparam;
        context = (Context*)create->lpCreateParams;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)context);
    } else {
        context = (Context*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    }
    if(context == NULL) {
        return DefWindowProcA(hwnd, message, wparam, lparam);
    }
    log_print(LOG_PLATFORM_VERBOSE, "Win32 message 0x%x", message);

    switch(message) {
        case WM_CLOSE: {
            log_print(LOG_INFO, "Close requested by the window");
            context->close_requested = true;
            return 0;
        } break;
        case WM_ACTIVATE: {
            log_print(LOG_PLATFORM, LOWORD(wparam) == WA_INACTIVE ? "Window focus lost" : "Window focus gained");
            if(LOWORD(wparam) == WA_INACTIVE) {
                platform_push_event(&context->platform, (PlatformEvent){ .type = PLATFORM_EVENT_DEFOCUS });
            }
        } break;
        case WM_SIZE: {
            iv2 window_size = iv2_new(LOWORD(lparam), HIWORD(lparam));
            if(!iv2_eq(window_size, context->platform.window_size)) {
                log_print(LOG_PLATFORM, "Window resized %ix%i -> %ix%i",
                          context->platform.window_size.x, context->platform.window_size.y, window_size.x, window_size.y);
                context->platform.window_size = window_size;
            }
            return 0;
        } break;
        // Alt, F10, and keys pressed with alt arrive as system keys. They're kept
        // from DefWindowProc, which would open the window menu on alt or F10,
        // except alt+F4 so it still closes the window.
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            PlatformKey key = platform_key_from_win32(wparam, lparam);
            if(key == PLATFORM_KEY_NONE) {
                log_print(LOG_PLATFORM_VERBOSE, "Unmapped key: virtual key 0x%x", (u32)wparam);
            }
            // Bit 30 is set if the key was already down, i.e. this is an auto-repeat
            PlatformEventType type = (lparam & (1 << 30)) ? PLATFORM_EVENT_KEYREPEAT : PLATFORM_EVENT_KEYDOWN;
            platform_push_event(&context->platform, (PlatformEvent){
                .type = type, .modifiers = platform_modifiers_from_win32(), .key = key });
            if(message == WM_KEYDOWN || wparam != VK_F4) {
                return 0;
            }
        } break;
        case WM_KEYUP:
        case WM_SYSKEYUP: {
            PlatformKey key = platform_key_from_win32(wparam, lparam);
            platform_push_event(&context->platform, (PlatformEvent){
                .type = PLATFORM_EVENT_KEYUP, .modifiers = platform_modifiers_from_win32(), .key = key });
            return 0;
        } break;
        // Generated from keydowns by TranslateMessage
        case WM_CHAR: {
            // TODO: Combine UTF-16 surrogate pairs. Their halves are dropped for now.
            if(wparam >= 0xD800 && wparam <= 0xDFFF) return 0;
            platform_push_event(&context->platform, (PlatformEvent){
                .type = PLATFORM_EVENT_CHAR, .modifiers = platform_modifiers_from_win32(), .codepoint = (u32)wparam });
            return 0;
        } break;
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP: {
            // Signed coordinates, since captured drags report positions outside the window
            iv2 position = iv2_new(GET_X_LPARAM(lparam), context->platform.window_size.y - GET_Y_LPARAM(lparam));
            context->platform.mouse_position = position;
            if(message == WM_MOUSEMOVE) {
                return 0;
            }
            bool down = message == WM_LBUTTONDOWN || message == WM_MBUTTONDOWN || message == WM_RBUTTONDOWN;
            PlatformMouseButton button = PLATFORM_MOUSE_BUTTON_LEFT;
            if(message == WM_MBUTTONDOWN || message == WM_MBUTTONUP) {
                button = PLATFORM_MOUSE_BUTTON_MIDDLE;
            } else if(message == WM_RBUTTONDOWN || message == WM_RBUTTONUP) {
                button = PLATFORM_MOUSE_BUTTON_RIGHT;
            }
            // Capture the mouse while any button is held, so drags keep reporting outside the window
            if(down) {
                SetCapture(hwnd);
            } else if((wparam & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON)) == 0) {
                ReleaseCapture();
            }
            platform_push_event(&context->platform, (PlatformEvent){
                .type = down ? PLATFORM_EVENT_MOUSE_DOWN : PLATFORM_EVENT_MOUSE_UP,
                .mouse = { .button = button, .position = position } });
            return 0;
        } break;
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL: {
            // Wheel messages carry screen coordinates
            POINT point = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
            ScreenToClient(hwnd, &point);
            iv2 position = iv2_new(point.x, context->platform.window_size.y - point.y);
            f32 notches = (f32)GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA;
            v2 delta = message == WM_MOUSEWHEEL ? v2_new(0.0f, notches) : v2_new(notches, 0.0f);
            platform_push_event(&context->platform, (PlatformEvent){
                .type = PLATFORM_EVENT_MOUSE_SCROLL, .scroll = { .delta = delta, .position = position } });
            return 0;
        } break;
        default: break;
    }
    return DefWindowProcA(hwnd, message, wparam, lparam);
}

// Owns the WASAPI stream after initialization. Every time WASAPI asks for more
// audio, copy what the main thread has queued in the ring, padding with silence
// if it has fallen behind (for instance while the window is being dragged,
// which blocks the main loop). Doesn't log, since the log isn't thread safe.
DWORD WINAPI wasapi_audio_thread(LPVOID arg) {
    Wasapi* audio = (Wasapi*)arg;

    // Ask the scheduler for audio priority. Failing that, run at normal priority.
    DWORD task_index = 0;
    HANDLE task = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);
    if(task == NULL) {
        InterlockedExchange(&audio->priority_failed, 1);
    }

    IAudioRenderClient* playback = NULL;
    HR_VERIFY(IAudioClient_GetService(audio->client, &WASAPI_IID_AUDIO_RENDER_CLIENT, (void**)&playback));
    UINT32 buffer_frames = 0;
    HR_VERIFY(IAudioClient_GetBufferSize(audio->client, &buffer_frames));
    HR_VERIFY(IAudioClient_Start(audio->client));

    u32 ring_mask = audio->ring_size - 1;
    while(WaitForSingleObject(audio->event, INFINITE) == WAIT_OBJECT_0) {
        if(ReadAcquire(&audio->stop)) {
            break;
        }

        UINT32 padding_frames = 0;
        HR_VERIFY(IAudioClient_GetCurrentPadding(audio->client, &padding_frames));
        u32 free_frames = buffer_frames - padding_frames;
        if(free_frames == 0) {
            continue;
        }
        BYTE* output = NULL;
        HR_VERIFY(IAudioRenderClient_GetBuffer(playback, free_frames, &output));

        // Copy what's queued, and silence for the rest
        u32 read_offset = (u32)audio->read_offset;
        u32 write_offset = (u32)ReadAcquire(&audio->write_offset);
        u32 queued_frames = (write_offset - read_offset) / AUDIO_BYTES_PER_FRAME;
        u32 copy_frames = min(queued_frames, free_frames);
        // Silence before the main loop's first write is expected, not an underrun
        if(copy_frames < free_frames && write_offset != 0) {
            InterlockedIncrement(&audio->underrun_count);
        }
        memcpy(output, audio->ring + (read_offset & ring_mask), copy_frames * AUDIO_BYTES_PER_FRAME);
        memset(output + copy_frames * AUDIO_BYTES_PER_FRAME, 0, (free_frames - copy_frames) * AUDIO_BYTES_PER_FRAME);
        HR_VERIFY(IAudioRenderClient_ReleaseBuffer(playback, free_frames, 0));
        InterlockedAdd(&audio->read_offset, (LONG)(copy_frames * AUDIO_BYTES_PER_FRAME));
    }

    HR_VERIFY(IAudioClient_Stop(audio->client));
    IAudioRenderClient_Release(playback);
    if(task != NULL) {
        AvRevertMmThreadCharacteristics(task);
    }
    return 0;
}

i32 WINAPI WinMain(HINSTANCE hinstance, HINSTANCE prev_hinstance, PSTR cmd_line, i32 show_cmd) {
    // Allocate memory
    void* mem = VirtualAlloc(NULL, ROOT_MEMORY_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    assert(mem != NULL);
    Stack root_stack = stack_from_memory(mem, ROOT_MEMORY_SIZE, string_const("Root"));
    Buffer context_buffer = stack_alloc_labeled(&root_stack, sizeof(Context), string_const("Context"));
    Context* context = (Context*)context_buffer.memory;
    memset(context, 0, sizeof(Context));

    // Track buffers from here on, including the two made before the tracker existed
    buffer_tracker_bind(&context->buffers);
    buffer_track(root_stack.buffer, string_const("Root"), BUFFER_TYPE_RAW | BUFFER_TYPE_STACK);
    buffer_track(context_buffer, string_const("Context"), BUFFER_TYPE_SUB);
    context->platform.buffers = &context->buffers;
    Stack game_stack           = stack_from_stack(&root_stack, GAME_STACK_SIZE, string_const("Game"));
    Stack render_stack         = stack_from_stack(&root_stack, RENDER_STACK_SIZE, string_const("Renderer"));
    Stack platform_frame_stack = stack_from_stack(&root_stack, PLATFORM_FRAME_STACK_SIZE, string_const("PlatformFrame"));

    // Open a console for stdout and stderr, which a /SUBSYSTEM:WINDOWS program doesn't get
    if(!AllocConsole()) {
        log_exit("AllocConsole failed (error %lu)", GetLastError());
    }
    FILE* console_file;
    freopen_s(&console_file, "CONOUT$", "w", stdout);
    freopen_s(&console_file, "CONOUT$", "w", stderr);
    freopen_s(&console_file, "CONIN$",  "r", stdin);

    // Initialize logging
    log_init(&context->log, LOG_TARGETS, string_const(LOG_FILE_PATH));
    log_bind(&context->log);
    context->platform.log = &context->log;

    // Initialize profiling
    profile_init(&context->profile);
    profile_bind(&context->profile);
    context->platform.profile = &context->profile;
    context->platform.frame_stack = &platform_frame_stack;
    log_print(LOG_INFO, "%s starting on Windows, built " __DATE__ " " __TIME__ ", log mask 0x%" PRIx64 ", targets 0x%x",
              GAME_NAME, (u64)(LOG_MASK), (u32)(LOG_TARGETS));
    log_print(LOG_MEMORY, "Root memory %" PRIu64 " bytes: context %" PRIu64 ", game %" PRIu64 ", renderer %" PRIu64 ", platform frame %" PRIu64,
              (u64)ROOT_MEMORY_SIZE, (u64)sizeof(Context), (u64)GAME_STACK_SIZE, (u64)RENDER_STACK_SIZE, (u64)PLATFORM_FRAME_STACK_SIZE);

    // Create window, sized so its client area is the requested size
    WNDCLASSA window_class = {};
    window_class.lpfnWndProc   = window_proc;
    window_class.hInstance     = hinstance;
    window_class.hCursor       = LoadCursor(NULL, IDC_ARROW);
    window_class.lpszClassName = GAME_NAME;
    if(!RegisterClassA(&window_class)) {
        log_exit("RegisterClassA failed (error %lu)", GetLastError());
    }

    RECT window_rect = { 0, 0, 1280, 720 };
    AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, FALSE);
    context->hwnd = CreateWindowExA(
        0, GAME_NAME, GAME_NAME, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, window_rect.right - window_rect.left, window_rect.bottom - window_rect.top,
        NULL, NULL, hinstance, context);
    if(context->hwnd == NULL) {
        log_exit("CreateWindowExA failed (error %lu)", GetLastError());
    }
    RECT client_rect;
    GetClientRect(context->hwnd, &client_rect);
    context->platform.window_size = iv2_new(client_rect.right, client_rect.bottom);
    log_print(LOG_PLATFORM, "Window created, 1280x720 requested, client area %ix%i",
              context->platform.window_size.x, context->platform.window_size.y);

    // Initialize WASAPI audio on the default playback device
    Wasapi* audio = &context->audio;
    HR_VERIFY(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED));
    IMMDeviceEnumerator* enumerator = NULL;
    HR_VERIFY(CoCreateInstance(&WASAPI_CLSID_MM_DEVICE_ENUMERATOR, NULL, CLSCTX_ALL, &WASAPI_IID_MM_DEVICE_ENUMERATOR, (void**)&enumerator));
    IMMDevice* device = NULL;
    HR_VERIFY(IMMDeviceEnumerator_GetDefaultAudioEndpoint(enumerator, eRender, eConsole, &device));
    IMMDeviceEnumerator_Release(enumerator);
    HR_VERIFY(IMMDevice_Activate(device, &WASAPI_IID_AUDIO_CLIENT, CLSCTX_ALL, NULL, (void**)&audio->client));
    IMMDevice_Release(device);
    WAVEFORMATEX* mix_format = NULL;
    HR_VERIFY(IAudioClient_GetMixFormat(audio->client, &mix_format));
    log_print(LOG_AUDIO, "WASAPI mix format: %lu Hz, %u channels, %u bits",
              (unsigned long)mix_format->nSamplesPerSec, (u32)mix_format->nChannels, (u32)mix_format->wBitsPerSample);
    CoTaskMemFree(mix_format);

    WAVEFORMATEXTENSIBLE format = {};
    format.Format.wFormatTag           = WAVE_FORMAT_EXTENSIBLE;
    format.Format.nChannels            = (WORD)AUDIO_CHANNEL_COUNT;
    format.Format.nSamplesPerSec       = (DWORD)AUDIO_SAMPLE_RATE;
    format.Format.nAvgBytesPerSec      = (DWORD)(AUDIO_SAMPLE_RATE * AUDIO_BYTES_PER_FRAME);
    format.Format.nBlockAlign          = (WORD)AUDIO_BYTES_PER_FRAME;
    format.Format.wBitsPerSample       = (WORD)(sizeof(f32) * 8);
    format.Format.cbSize               = sizeof(format) - sizeof(format.Format);
    format.Samples.wValidBitsPerSample = (WORD)(sizeof(f32) * 8);
    format.dwChannelMask               = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    format.SubFormat                   = WASAPI_SUBTYPE_IEEE_FLOAT;

    // Prefer IAudioClient3's low latency shared mode, which is only available
    // if the device's mix format matches ours. Otherwise fall back to a default
    // period and let WASAPI convert.
    bool client_initialized = false;
    IAudioClient3* client3 = NULL;
    if(SUCCEEDED(IAudioClient_QueryInterface(audio->client, &WASAPI_IID_AUDIO_CLIENT3, (void**)&client3))) {
        UINT32 period_default = 0;
        UINT32 period_fundamental = 0;
        UINT32 period_min = 0;
        UINT32 period_max = 0;
        if(SUCCEEDED(IAudioClient3_GetSharedModeEnginePeriod(client3, (WAVEFORMATEX*)&format, &period_default, &period_fundamental, &period_min, &period_max))
        && SUCCEEDED(IAudioClient3_InitializeSharedAudioStream(client3, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, period_min, (WAVEFORMATEX*)&format, NULL))) {
            client_initialized = true;
            log_print(LOG_AUDIO, "WASAPI low latency period: %u frames", period_min);
        }
        IAudioClient3_Release(client3);
    }
    if(!client_initialized) {
        REFERENCE_TIME period = 0;
        HR_VERIFY(IAudioClient_GetDevicePeriod(audio->client, &period, NULL));
        DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
        HR_VERIFY(IAudioClient_Initialize(audio->client, AUDCLNT_SHAREMODE_SHARED, flags, period, 0, (WAVEFORMATEX*)&format, NULL));
        log_print(LOG_AUDIO, "WASAPI default period: %lld ns", (long long)period * 100);
    }
    audio->event = CreateEventA(NULL, FALSE, FALSE, NULL);
    assert(audio->event != NULL);
    HR_VERIFY(IAudioClient_SetEventHandle(audio->client, audio->event));

    // Map the ring buffer twice back to back: reserve a placeholder for both
    // halves, split it, and map one section into each half.
    audio->ring_size = u32_round_up_to_power_of_2(max(64 * 1024, AUDIO_SAMPLE_RATE * AUDIO_BYTES_PER_FRAME));
    assert(AUDIO_LATENCY_FRAMES * AUDIO_BYTES_PER_FRAME < audio->ring_size);
    u8* placeholder_1 = (u8*)VirtualAlloc2(NULL, NULL, 2 * audio->ring_size, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, NULL, 0);
    assert(placeholder_1 != NULL);
    u8* placeholder_2 = placeholder_1 + audio->ring_size;
    assert(VirtualFree(placeholder_1, audio->ring_size, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER));
    HANDLE section = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, audio->ring_size, NULL);
    assert(section != NULL);
    void* view_1 = MapViewOfFile3(section, NULL, placeholder_1, 0, audio->ring_size, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0);
    void* view_2 = MapViewOfFile3(section, NULL, placeholder_2, 0, audio->ring_size, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0);
    assert(view_1 != NULL && view_2 != NULL);
    // The views keep the section alive
    CloseHandle(section);
    audio->ring = (u8*)view_1;
    audio->read_offset = 0;
    audio->write_offset = 0;
    audio->stop = 0;
    audio->underrun_count = 0;
    audio->priority_failed = 0;
    log_print(LOG_AUDIO, "WASAPI ring buffer %u bytes, target latency %u frames", audio->ring_size, (u32)AUDIO_LATENCY_FRAMES);
    audio->thread = CreateThread(NULL, 0, wasapi_audio_thread, audio, 0, NULL);
    assert(audio->thread != NULL);

    // Initialize renderer and game
    VkContext* vk = (VkContext*)stack_alloc_zero(&render_stack, sizeof(VkContext));
    Stack render_scratch_stack = stack_from_stack(&render_stack, MEGABYTE, string_const("RendererScratch"));
    VkPlatformWindow vk_window = { .hwnd = context->hwnd, .hinstance = hinstance };
    vk_init(vk, string_const(GAME_NAME), vk_window, context->platform.window_size, &render_scratch_stack);

    dynamic_library_init(&context->game.library, string_const(GAME_LIB_NAME));
    game_library_update(&context->game);
    if(context->game.init == NULL) {
        log_exit("Couldn't load the game library %s. It is found relative to the working directory.", GAME_LIB_NAME);
    }
    RenderSetup* render_setup = (RenderSetup*)stack_alloc_zero(&render_stack, sizeof(RenderSetup));
    log_print(LOG_ASSET, "Asset pack: %" PRIu64 " bytes", (u64)ASSET_PACK_SIZE);
    context->game.init(game_stack.memory, asset_pack_data, render_setup, &context->platform);
    vk_load_assets(vk, render_setup);

    // Loop
    u64 frame_count = 0;
    f64 frame_time_total = 0.0;
    bool was_minimized = false;
    LONG reported_underrun_count = 0;
    bool priority_reported = false;
    u64 frame_start = profile_time_ns();
    while(context->close_requested == false) {
        profile_begin(PROFILE_FRAME);

        // A minimized window has no area to render to, so only keep audio going
        bool minimized = context->platform.window_size.x == 0 || context->platform.window_size.y == 0;
        if(minimized != was_minimized) {
            log_print(LOG_PLATFORM, minimized ? "Window minimized" : "Window restored");
            was_minimized = minimized;
        }

        // Wait for a free frame and swapchain image before pumping messages, so the
        // game sees input from after the wait rather than before it. A resize or
        // minimize seen in this frame's messages takes effect next frame.
        RenderFrame render_frame;
        if(!minimized) {
            profile_begin(PROFILE_RENDER_BEGIN);
            vk_frame_begin(vk, context->platform.window_size, &render_frame);
            profile_end(PROFILE_RENDER_BEGIN);
        }
        context->platform.time_ns = profile_time_ns();

        // Pump Win32 messages, which window_proc turns into platform state and events,
        // then poll gamepads
        profile_begin(PROFILE_PLATFORM_EVENTS);
        MSG message;
        while(PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
        xinput_poll(context);
        profile_end(PROFILE_PLATFORM_EVENTS);
        if(context->close_requested) {
            break;
        }

        // Update game, which writes the frame straight into GPU memory
        if(!minimized) {
            profile_begin(PROFILE_GAME_UPDATE);
            context->game.update(game_stack.memory, asset_pack_data, &render_frame, &context->platform);
            profile_end(PROFILE_GAME_UPDATE);
        }

        // Top the audio ring up to the target latency. The game writes straight into it.
        profile_begin(PROFILE_AUDIO);
        u32 write_offset = (u32)audio->write_offset;
        u32 read_offset = (u32)ReadAcquire(&audio->read_offset);
        i32 queued_frames = (i32)((write_offset - read_offset) / AUDIO_BYTES_PER_FRAME);
        i32 frames_len = AUDIO_LATENCY_FRAMES - queued_frames;
        if(frame_count > 0 && queued_frames < AUDIO_LATENCY_FRAMES / 4) {
            log_print(LOG_WARN, "Audio nearly drained: %i frames queued, target %u", queued_frames, (u32)AUDIO_LATENCY_FRAMES);
        }
        log_print(LOG_AUDIO_VERBOSE, "WASAPI ring %i frames queued, writing %i frames", queued_frames, frames_len);
        if(frames_len > 0) {
            f32* samples = (f32*)(audio->ring + (write_offset & (audio->ring_size - 1)));
            context->game.audio_callback(game_stack.memory, asset_pack_data, samples, frames_len);
            InterlockedAdd(&audio->write_offset, (LONG)(frames_len * AUDIO_BYTES_PER_FRAME));
        }
        profile_end(PROFILE_AUDIO);

        // Report what the audio thread counted, since it doesn't log
        LONG underrun_count = ReadAcquire(&audio->underrun_count);
        if(underrun_count != reported_underrun_count) {
            log_print(LOG_WARN, "WASAPI underrun: %ld periods padded with silence (%ld total)",
                      (long)(underrun_count - reported_underrun_count), (long)underrun_count);
            reported_underrun_count = underrun_count;
        }
        if(!priority_reported && ReadAcquire(&audio->priority_failed)) {
            log_print(LOG_WARN, "Audio thread couldn't get Pro Audio scheduling priority");
            priority_reported = true;
        }

        // Render
        if(!minimized) {
            profile_begin(PROFILE_RENDER_END);
            vk_frame_end(vk, &render_frame);
            profile_end(PROFILE_RENDER_END);
        } else {
            Sleep(10);
        }

        // Prepare for next frame
        stack_clear(&platform_frame_stack);
        game_library_update(&context->game);
        context->platform.events_len = 0;

        // Frame timing
        u64 frame_end = profile_time_ns();
        f64 frame_time = (f64)(frame_end - frame_start) / 1e9;
        frame_start = frame_end;
        frame_time_total += frame_time;
        frame_count++;
        log_print(LOG_RENDER_VERBOSE, "Frame %" PRIu64 ": %.2f ms", frame_count, frame_time * 1000.0);
        profile_end(PROFILE_FRAME);
        profile_frame_end();
    }

    // Stop the audio thread before the process tears down the memory it uses
    InterlockedExchange(&audio->stop, 1);
    SetEvent(audio->event);
    WaitForSingleObject(audio->thread, INFINITE);

    // Shut down
    log_print(LOG_INFO, "Shutting down after %" PRIu64 " frames, average frame time %.2f ms",
              frame_count, frame_count > 0 ? frame_time_total / (f64)frame_count * 1000.0 : 0.0);
    profile_log(LOG_INFO);
    stack_log_usage(&root_stack);
    stack_log_usage(&game_stack);
    stack_log_usage(&render_stack);
    stack_log_usage(&render_scratch_stack);
    stack_log_usage(&platform_frame_stack);
    return 0;
}

#endif

// LINUX
#if PLATFORM == PLATFORM_LINUX

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <alsa/asoundlib.h>
#include <alloca.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>

#define ALSA_VERIFY(alsa_function) { \
    i32 alsa_error; \
    if((alsa_error = alsa_function) < 0) { \
        log_exit("ALSA error: %s", snd_strerror(alsa_error)); \
    } \
} 

// An open evdev gamepad. fd is -1 for an empty slot.
typedef struct {
    i32                 fd;
    char                path[280];
    // Ranges of the absolute axes, indexed by ABS_* code
    struct input_absinfo abs[ABS_HAT0Y + 1];
} EvdevGamepad;

typedef struct {
    bool                close_requested;
    Log                 log;
    Profile             profile;
    BufferTracker       buffers;

    Display*            display;
    Window              window;
    snd_pcm_t*          alsa_pcm;
    Platform            platform;

    GameLibrary         game;

    // Gamepads, one evdev device per slot. inotify reports new devices.
    EvdevGamepad        gamepads[PLATFORM_MAX_GAMEPADS];
    i32                 input_watch_fd;
} Context;

// Keysyms are looked up unshifted (index 0), so letters are lowercase and
// numpad keys are their numlock-off keysyms.
PlatformKey platform_key_from_xlib_keysym(u32 keysym) {
    if(keysym >= XK_a && keysym <= XK_z) return (PlatformKey)(PLATFORM_KEY_A + (keysym - XK_a));
    if(keysym >= XK_0 && keysym <= XK_9) return (PlatformKey)(PLATFORM_KEY_0 + (keysym - XK_0));
    if(keysym >= XK_F1 && keysym <= XK_F12) return (PlatformKey)(PLATFORM_KEY_F1 + (keysym - XK_F1));
    if(keysym >= XK_KP_0 && keysym <= XK_KP_9) return (PlatformKey)(PLATFORM_KEY_NUMPAD_0 + (keysym - XK_KP_0));
    switch(keysym) {
        case XK_Escape:           return PLATFORM_KEY_ESCAPE;
        case XK_Tab:              return PLATFORM_KEY_TAB;
        case XK_ISO_Left_Tab:     return PLATFORM_KEY_TAB;
        case XK_space:            return PLATFORM_KEY_SPACE;
        case XK_Return:           return PLATFORM_KEY_ENTER;
        case XK_BackSpace:        return PLATFORM_KEY_BACKSPACE;
        case XK_Delete:           return PLATFORM_KEY_DELETE;
        case XK_Insert:           return PLATFORM_KEY_INSERT;
        case XK_Home:             return PLATFORM_KEY_HOME;
        case XK_End:              return PLATFORM_KEY_END;
        case XK_Prior:            return PLATFORM_KEY_PAGE_UP;
        case XK_Next:             return PLATFORM_KEY_PAGE_DOWN;
        case XK_Up:               return PLATFORM_KEY_UP;
        case XK_Down:             return PLATFORM_KEY_DOWN;
        case XK_Left:             return PLATFORM_KEY_LEFT;
        case XK_Right:            return PLATFORM_KEY_RIGHT;
        case XK_Shift_L:          return PLATFORM_KEY_LEFT_SHIFT;
        case XK_Shift_R:          return PLATFORM_KEY_RIGHT_SHIFT;
        case XK_Control_L:        return PLATFORM_KEY_LEFT_CTRL;
        case XK_Control_R:        return PLATFORM_KEY_RIGHT_CTRL;
        case XK_Alt_L:            return PLATFORM_KEY_LEFT_ALT;
        case XK_Alt_R:            return PLATFORM_KEY_RIGHT_ALT;
        case XK_ISO_Level3_Shift: return PLATFORM_KEY_RIGHT_ALT;
        case XK_Caps_Lock:        return PLATFORM_KEY_CAPS_LOCK;
        case XK_grave:            return PLATFORM_KEY_GRAVE;
        case XK_minus:            return PLATFORM_KEY_MINUS;
        case XK_equal:            return PLATFORM_KEY_EQUALS;
        case XK_bracketleft:      return PLATFORM_KEY_LEFT_BRACKET;
        case XK_bracketright:     return PLATFORM_KEY_RIGHT_BRACKET;
        case XK_backslash:        return PLATFORM_KEY_BACKSLASH;
        case XK_semicolon:        return PLATFORM_KEY_SEMICOLON;
        case XK_apostrophe:       return PLATFORM_KEY_APOSTROPHE;
        case XK_comma:            return PLATFORM_KEY_COMMA;
        case XK_period:           return PLATFORM_KEY_PERIOD;
        case XK_slash:            return PLATFORM_KEY_SLASH;
        case XK_KP_Insert:        return PLATFORM_KEY_NUMPAD_0;
        case XK_KP_End:           return PLATFORM_KEY_NUMPAD_1;
        case XK_KP_Down:          return PLATFORM_KEY_NUMPAD_2;
        case XK_KP_Next:          return PLATFORM_KEY_NUMPAD_3;
        case XK_KP_Left:          return PLATFORM_KEY_NUMPAD_4;
        case XK_KP_Begin:         return PLATFORM_KEY_NUMPAD_5;
        case XK_KP_Right:         return PLATFORM_KEY_NUMPAD_6;
        case XK_KP_Home:          return PLATFORM_KEY_NUMPAD_7;
        case XK_KP_Up:            return PLATFORM_KEY_NUMPAD_8;
        case XK_KP_Prior:         return PLATFORM_KEY_NUMPAD_9;
        case XK_KP_Add:           return PLATFORM_KEY_NUMPAD_ADD;
        case XK_KP_Subtract:      return PLATFORM_KEY_NUMPAD_SUBTRACT;
        case XK_KP_Multiply:      return PLATFORM_KEY_NUMPAD_MULTIPLY;
        case XK_KP_Divide:        return PLATFORM_KEY_NUMPAD_DIVIDE;
        case XK_KP_Delete:        return PLATFORM_KEY_NUMPAD_DECIMAL;
        case XK_KP_Decimal:       return PLATFORM_KEY_NUMPAD_DECIMAL;
        case XK_KP_Enter:         return PLATFORM_KEY_NUMPAD_ENTER;
        default:                  return PLATFORM_KEY_NONE;
    }
}

// Modifier keys held as of a key event, from its state mask
// An evdev absolute axis as a platform value: sticks -1 to 1 with positive y
// up, triggers 0 to 1. A stick's center is its rest; a trigger's minimum is.
f32 evdev_axis_value(EvdevGamepad* gamepad, i32 code, i32 value) {
    struct input_absinfo* info = &gamepad->abs[code];
    if(info->maximum <= info->minimum) return 0.0f;
    f32 t = (f32)(value - info->minimum) / (f32)(info->maximum - info->minimum);
    if(code == ABS_Z || code == ABS_RZ) return f32_clamp(t, 0.0f, 1.0f);
    f32 centered = f32_clamp(t * 2.0f - 1.0f, -1.0f, 1.0f);
    // Integer ranges have no exact center, so the rest position can read slightly off 0
    if(value == (info->minimum + info->maximum + 1) / 2 || value == (info->minimum + info->maximum) / 2) centered = 0.0f;
    return (code == ABS_Y || code == ABS_RY) ? -centered : centered;
}

// The platform button for an evdev key code, or -1
i32 evdev_gamepad_button(u16 code) {
    switch(code) {
        case BTN_SOUTH:  return PLATFORM_GAMEPAD_BUTTON_SOUTH;
        case BTN_EAST:   return PLATFORM_GAMEPAD_BUTTON_EAST;
        case BTN_WEST:   return PLATFORM_GAMEPAD_BUTTON_WEST;
        case BTN_NORTH:  return PLATFORM_GAMEPAD_BUTTON_NORTH;
        case BTN_TL:     return PLATFORM_GAMEPAD_BUTTON_LEFT_SHOULDER;
        case BTN_TR:     return PLATFORM_GAMEPAD_BUTTON_RIGHT_SHOULDER;
        case BTN_THUMBL: return PLATFORM_GAMEPAD_BUTTON_LEFT_STICK;
        case BTN_THUMBR: return PLATFORM_GAMEPAD_BUTTON_RIGHT_STICK;
        case BTN_START:  return PLATFORM_GAMEPAD_BUTTON_START;
        case BTN_SELECT: return PLATFORM_GAMEPAD_BUTTON_BACK;
        case BTN_MODE:   return PLATFORM_GAMEPAD_BUTTON_GUIDE;
        case BTN_DPAD_UP:    return PLATFORM_GAMEPAD_BUTTON_DPAD_UP;
        case BTN_DPAD_DOWN:  return PLATFORM_GAMEPAD_BUTTON_DPAD_DOWN;
        case BTN_DPAD_LEFT:  return PLATFORM_GAMEPAD_BUTTON_DPAD_LEFT;
        case BTN_DPAD_RIGHT: return PLATFORM_GAMEPAD_BUTTON_DPAD_RIGHT;
        default:         return -1;
    }
}

// Open every evdev device in /dev/input that looks like a gamepad and isn't
// open yet, and give it a free slot. Devices that can't be opened, usually for
// lack of permission, are skipped quietly; udev may not have granted access
// yet, and the next scan retries.
void evdev_scan(Context* context) {
    DIR* dir = opendir("/dev/input");
    if(dir == NULL) {
        return;
    }
    struct dirent* entry;
    while((entry = readdir(dir)) != NULL) {
        if(strncmp(entry->d_name, "event", 5) != 0) {
            continue;
        }
        char path[280];
        snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);
        i32 slot = -1;
        bool already_open = false;
        for(i32 i = 0; i < PLATFORM_MAX_GAMEPADS; i++) {
            if(context->gamepads[i].fd >= 0 && strcmp(context->gamepads[i].path, path) == 0) {
                already_open = true;
            }
            if(context->gamepads[i].fd < 0 && slot < 0) {
                slot = i;
            }
        }
        if(already_open || slot < 0) {
            continue;
        }
        i32 fd = open(path, O_RDONLY | O_NONBLOCK);
        if(fd < 0) {
            continue;
        }

        // A gamepad has the south face button
        u8 key_bits[(KEY_MAX + 7) / 8] = {};
        ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits);
        if(!(key_bits[BTN_SOUTH / 8] & (1 << (BTN_SOUTH % 8)))) {
            close(fd);
            continue;
        }

        EvdevGamepad* gamepad = &context->gamepads[slot];
        gamepad->fd = fd;
        snprintf(gamepad->path, sizeof(gamepad->path), "%s", path);
        char name[128] = "Unknown";
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        log_print(LOG_PLATFORM, "Gamepad %i connected: %s (%s)", slot, name, path);
        platform_push_event(&context->platform, (PlatformEvent){ .type = PLATFORM_EVENT_GAMEPAD_CONNECT, .gamepad = { .index = slot } });

        // Axis ranges. Axes already off rest are reported, as XInput does.
        for(i32 code = 0; code <= ABS_HAT0Y; code++) {
            gamepad->abs[code] = (struct input_absinfo){};
            ioctl(fd, EVIOCGABS(code), &gamepad->abs[code]);
        }
        static const i32 axis_codes[PLATFORM_GAMEPAD_AXIS_COUNT] = {
            [PLATFORM_GAMEPAD_AXIS_LEFT_X] = ABS_X, [PLATFORM_GAMEPAD_AXIS_LEFT_Y] = ABS_Y,
            [PLATFORM_GAMEPAD_AXIS_RIGHT_X] = ABS_RX, [PLATFORM_GAMEPAD_AXIS_RIGHT_Y] = ABS_RY,
            [PLATFORM_GAMEPAD_AXIS_LEFT_TRIGGER] = ABS_Z, [PLATFORM_GAMEPAD_AXIS_RIGHT_TRIGGER] = ABS_RZ,
        };
        for(i32 a = 0; a < PLATFORM_GAMEPAD_AXIS_COUNT; a++) {
            f32 value = evdev_axis_value(gamepad, axis_codes[a], gamepad->abs[axis_codes[a]].value);
            if(value != 0.0f) {
                platform_push_event(&context->platform, (PlatformEvent){
                    .type = PLATFORM_EVENT_GAMEPAD_AXIS, .gamepad = { .index = slot, .axis = (PlatformGamepadAxis)a, .value = value } });
            }
        }
    }
    closedir(dir);
}

// Pick up new gamepads, then turn every open gamepad's queued evdev events into
// platform events.
// TODO: Resync from EVIOCGKEY/EVIOCGABS after SYN_DROPPED, when the kernel's queue overflowed
// TODO: Per-controller quirks. Some drivers swap the north and west buttons, or put triggers elsewhere than ABS_Z/ABS_RZ.
void evdev_poll(Context* context) {
    // Any change in /dev/input prompts a rescan. Attribute changes count, since
    // udev grants access after the device node appears.
    bool rescan = false;
    char watch_buffer[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    while(context->input_watch_fd >= 0 && read(context->input_watch_fd, watch_buffer, sizeof(watch_buffer)) > 0) {
        rescan = true;
    }
    if(rescan) {
        evdev_scan(context);
    }

    for(i32 slot = 0; slot < PLATFORM_MAX_GAMEPADS; slot++) {
        EvdevGamepad* gamepad = &context->gamepads[slot];
        if(gamepad->fd < 0) {
            continue;
        }
        struct input_event events[64];
        ssize_t bytes;
        while((bytes = read(gamepad->fd, events, sizeof(events))) > 0) {
            i32 events_len = (i32)(bytes / sizeof(struct input_event));
            for(i32 i = 0; i < events_len; i++) {
                struct input_event* event = &events[i];
                if(event->type == EV_KEY) {
                    i32 button = evdev_gamepad_button(event->code);
                    // A value of 2 is the kernel's auto-repeat, which gamepads don't need
                    if(button < 0 || event->value == 2) {
                        continue;
                    }
                    platform_push_event(&context->platform, (PlatformEvent){
                        .type = event->value ? PLATFORM_EVENT_GAMEPAD_BUTTON_DOWN : PLATFORM_EVENT_GAMEPAD_BUTTON_UP,
                        .gamepad = { .index = slot, .button = (PlatformGamepadButton)button } });
                } else if(event->type == EV_ABS) {
                    switch(event->code) {
                        case ABS_X:  case ABS_Y:  case ABS_RX: case ABS_RY: case ABS_Z: case ABS_RZ: {
                            PlatformGamepadAxis axis = event->code == ABS_X  ? PLATFORM_GAMEPAD_AXIS_LEFT_X
                                                     : event->code == ABS_Y  ? PLATFORM_GAMEPAD_AXIS_LEFT_Y
                                                     : event->code == ABS_RX ? PLATFORM_GAMEPAD_AXIS_RIGHT_X
                                                     : event->code == ABS_RY ? PLATFORM_GAMEPAD_AXIS_RIGHT_Y
                                                     : event->code == ABS_Z  ? PLATFORM_GAMEPAD_AXIS_LEFT_TRIGGER
                                                     :                         PLATFORM_GAMEPAD_AXIS_RIGHT_TRIGGER;
                            platform_push_event(&context->platform, (PlatformEvent){ .type = PLATFORM_EVENT_GAMEPAD_AXIS,
                                .gamepad = { .index = slot, .axis = axis, .value = evdev_axis_value(gamepad, event->code, event->value) } });
                        } break;
                        // Many drivers report the dpad as a hat axis: -1, 0 or 1 per direction
                        case ABS_HAT0X: case ABS_HAT0Y: {
                            bool x = event->code == ABS_HAT0X;
                            i32 previous = gamepad->abs[event->code].value;
                            PlatformGamepadButton negative = x ? PLATFORM_GAMEPAD_BUTTON_DPAD_LEFT : PLATFORM_GAMEPAD_BUTTON_DPAD_UP;
                            PlatformGamepadButton positive = x ? PLATFORM_GAMEPAD_BUTTON_DPAD_RIGHT : PLATFORM_GAMEPAD_BUTTON_DPAD_DOWN;
                            if(previous < 0 && event->value >= 0) {
                                platform_push_event(&context->platform, (PlatformEvent){ .type = PLATFORM_EVENT_GAMEPAD_BUTTON_UP, .gamepad = { .index = slot, .button = negative } });
                            }
                            if(previous > 0 && event->value <= 0) {
                                platform_push_event(&context->platform, (PlatformEvent){ .type = PLATFORM_EVENT_GAMEPAD_BUTTON_UP, .gamepad = { .index = slot, .button = positive } });
                            }
                            if(event->value < 0 && previous >= 0) {
                                platform_push_event(&context->platform, (PlatformEvent){ .type = PLATFORM_EVENT_GAMEPAD_BUTTON_DOWN, .gamepad = { .index = slot, .button = negative } });
                            }
                            if(event->value > 0 && previous <= 0) {
                                platform_push_event(&context->platform, (PlatformEvent){ .type = PLATFORM_EVENT_GAMEPAD_BUTTON_DOWN, .gamepad = { .index = slot, .button = positive } });
                            }
                        } break;
                        default: break;
                    }
                    if(event->code <= ABS_HAT0Y) {
                        gamepad->abs[event->code].value = event->value;
                    }
                }
            }
        }

        // ENODEV means the gamepad was unplugged
        if(bytes < 0 && errno != EAGAIN) {
            log_print(LOG_PLATFORM, "Gamepad %i disconnected (%s)", slot, strerror(errno));
            close(gamepad->fd);
            gamepad->fd = -1;
            platform_push_event(&context->platform, (PlatformEvent){ .type = PLATFORM_EVENT_GAMEPAD_DISCONNECT, .gamepad = { .index = slot } });
        }
    }
}

u32 platform_modifiers_from_xlib_state(u32 state) {
    u32 modifiers = PLATFORM_MODIFIER_NONE;
    if(state & ShiftMask)   modifiers |= PLATFORM_MODIFIER_SHIFT;
    if(state & ControlMask) modifiers |= PLATFORM_MODIFIER_CTRL;
    if(state & Mod1Mask)    modifiers |= PLATFORM_MODIFIER_ALT;
    return modifiers;
}

i32 main(i32 argc, char** argv) {
    // Allocate memory
    void* mem = malloc(ROOT_MEMORY_SIZE);
    Stack root_stack = stack_from_memory(mem, ROOT_MEMORY_SIZE, string_const("Root"));
    Buffer context_buffer = stack_alloc_labeled(&root_stack, sizeof(Context), string_const("Context"));
    Context* context = (Context*)context_buffer.memory;
    memset(context, 0, sizeof(Context));

    // Track buffers from here on, including the two made before the tracker existed
    buffer_tracker_bind(&context->buffers);
    buffer_track(root_stack.buffer, string_const("Root"), BUFFER_TYPE_RAW | BUFFER_TYPE_STACK);
    buffer_track(context_buffer, string_const("Context"), BUFFER_TYPE_SUB);
    context->platform.buffers = &context->buffers;
    Stack game_stack           = stack_from_stack(&root_stack, GAME_STACK_SIZE, string_const("Game"));
    Stack render_stack         = stack_from_stack(&root_stack, RENDER_STACK_SIZE, string_const("Renderer"));
    Stack platform_frame_stack = stack_from_stack(&root_stack, PLATFORM_FRAME_STACK_SIZE, string_const("PlatformFrame"));

    // Initialize logging
    log_init(&context->log, LOG_TARGETS, string_const(LOG_FILE_PATH));
    log_bind(&context->log);
    context->platform.log = &context->log;

    // Initialize profiling
    profile_init(&context->profile);
    profile_bind(&context->profile);
    context->platform.profile = &context->profile;
    context->platform.frame_stack = &platform_frame_stack;
    log_print(LOG_INFO, "%s starting on Linux, built " __DATE__ " " __TIME__ ", log mask 0x%" PRIx64 ", targets 0x%x",
              GAME_NAME, (u64)(LOG_MASK), (u32)(LOG_TARGETS));
    log_print(LOG_MEMORY, "Root memory %" PRIu64 " bytes: context %" PRIu64 ", game %" PRIu64 ", renderer %" PRIu64 ", platform frame %" PRIu64,
              (u64)ROOT_MEMORY_SIZE, (u64)sizeof(Context), (u64)GAME_STACK_SIZE, (u64)RENDER_STACK_SIZE, (u64)PLATFORM_FRAME_STACK_SIZE);

    // Init Xlib window
    context->display = XOpenDisplay("");
    if(context->display == NULL) {
        log_exit("Couldn't open X display");
    }

    i32 screen = DefaultScreen(context->display);
    Window root_window = RootWindow(context->display, screen);
    XSetWindowAttributes set_window_attributes = {};
    set_window_attributes.background_pixmap = None;
    set_window_attributes.border_pixel = 0;
    set_window_attributes.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask | KeyReleaseMask | PointerMotionMask | ButtonPressMask | ButtonReleaseMask | FocusChangeMask;

    u32 window_width = 1280;
    u32 window_height = 720;
    context->window = XCreateWindow(context->display, root_window, 0, 0, window_width, window_height, 0, DefaultDepth(context->display, screen), InputOutput, DefaultVisual(context->display, screen), CWBorderPixel | CWEventMask, &set_window_attributes);
    if(context->window == 0) {
        log_exit("XCreateWindow failed");
    }
    XStoreName(context->display, context->window, GAME_NAME);
    XMapWindow(context->display, context->window);
    context->platform.window_size = iv2_new(window_width, window_height);
    // The window manager may pick another size, which arrives as a ConfigureNotify
    log_print(LOG_PLATFORM, "Window created, %ux%u requested", window_width, window_height);

    // Ask the window manager to tell us when the window is closed, rather than killing the connection
    Atom wm_delete_window = XInternAtom(context->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(context->display, context->window, &wm_delete_window, 1);

    // Initialize ALSA audio
    u32 sample_rate = AUDIO_SAMPLE_RATE;
    snd_pcm_hw_params_t* hw_params;
    ALSA_VERIFY(snd_pcm_open(&context->alsa_pcm, "default", SND_PCM_STREAM_PLAYBACK, 0));
    snd_pcm_hw_params_alloca(&hw_params);
    ALSA_VERIFY(snd_pcm_hw_params_any(context->alsa_pcm, hw_params));
    ALSA_VERIFY(snd_pcm_hw_params_set_access(context->alsa_pcm, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED));
    //ALSA_VERIFY(snd_pcm_hw_params_set_format(context->alsa_pcm, hw_params, SND_PCM_FORMAT_S16_LE));
    ALSA_VERIFY(snd_pcm_hw_params_set_format(context->alsa_pcm, hw_params, SND_PCM_FORMAT_FLOAT_LE));
    ALSA_VERIFY(snd_pcm_hw_params_set_rate_near(context->alsa_pcm, hw_params, &sample_rate, 0));
    ALSA_VERIFY(snd_pcm_hw_params_set_channels(context->alsa_pcm, hw_params, AUDIO_CHANNEL_COUNT));
    ALSA_VERIFY(snd_pcm_hw_params(context->alsa_pcm, hw_params));
    ALSA_VERIFY(snd_pcm_prepare(context->alsa_pcm));
    if(sample_rate != AUDIO_SAMPLE_RATE) {
        log_print(LOG_WARN, "ALSA is playing at %u Hz rather than %u Hz", sample_rate, AUDIO_SAMPLE_RATE);
    }
    snd_pcm_uframes_t alsa_period_frames = 0;
    snd_pcm_uframes_t alsa_buffer_frames = 0;
    u32 alsa_channels = 0;
    ALSA_VERIFY(snd_pcm_hw_params_get_period_size(hw_params, &alsa_period_frames, NULL));
    ALSA_VERIFY(snd_pcm_hw_params_get_buffer_size(hw_params, &alsa_buffer_frames));
    ALSA_VERIFY(snd_pcm_hw_params_get_channels(hw_params, &alsa_channels));
    log_print(LOG_AUDIO, "ALSA device %s: %u Hz, %u channels, f32, period %lu frames, buffer %lu frames, target latency %u frames",
              snd_pcm_name(context->alsa_pcm), sample_rate, alsa_channels,
              (unsigned long)alsa_period_frames, (unsigned long)alsa_buffer_frames, (u32)AUDIO_LATENCY_FRAMES);

    // Initialize renderer and game
    VkContext* vk = (VkContext*)stack_alloc_zero(&render_stack, sizeof(VkContext));
    Stack render_scratch_stack = stack_from_stack(&render_stack, MEGABYTE, string_const("RendererScratch"));
    VkPlatformWindow vk_window = { .display = context->display, .window = context->window };
    vk_init(vk, string_const(GAME_NAME), vk_window, context->platform.window_size, &render_scratch_stack);

    dynamic_library_init(&context->game.library, string_const(GAME_LIB_NAME));
    game_library_update(&context->game);
    if(context->game.init == NULL) {
        log_exit("Couldn't load the game library %s. It is found relative to the working directory.", GAME_LIB_NAME);
    }
    RenderSetup* render_setup = (RenderSetup*)stack_alloc_zero(&render_stack, sizeof(RenderSetup));
    log_print(LOG_ASSET, "Asset pack: %" PRIu64 " bytes", (u64)ASSET_PACK_SIZE);
    context->game.init(game_stack.memory, asset_pack_data, render_setup, &context->platform);
    vk_load_assets(vk, render_setup);

    // Find gamepads, and watch for more. Without inotify, only gamepads present at startup are found.
    for(i32 i = 0; i < PLATFORM_MAX_GAMEPADS; i++) {
        context->gamepads[i].fd = -1;
    }
    context->input_watch_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if(context->input_watch_fd < 0 || inotify_add_watch(context->input_watch_fd, "/dev/input", IN_CREATE | IN_ATTRIB) < 0) {
        log_print(LOG_WARN, "Couldn't watch /dev/input for gamepads (%s)", strerror(errno));
    }
    evdev_scan(context);

    // Loop
    u64 frame_count = 0;
    f64 frame_time_total = 0.0;
    u64 frame_start = profile_time_ns();
    while(context->close_requested == false) {
        profile_begin(PROFILE_FRAME);

        // Wait for a free frame and swapchain image before polling, so the game sees
        // input from after the wait rather than before it. A resize seen in this
        // frame's events reaches the swapchain next frame.
        RenderFrame render_frame;
        profile_begin(PROFILE_RENDER_BEGIN);
        vk_frame_begin(vk, context->platform.window_size, &render_frame);
        profile_end(PROFILE_RENDER_BEGIN);
        context->platform.time_ns = profile_time_ns();

        // Poll Xlib events
        profile_begin(PROFILE_PLATFORM_EVENTS);
        // Set by an auto-repeat's release, which is always directly followed by its press
        bool next_key_press_repeats = false;
        while(XPending(context->display)) {
            XEvent event;
            XNextEvent(context->display, &event);
            log_print(LOG_PLATFORM_VERBOSE, "X event type %i", event.type);
            switch(event.type) {
                case Expose:
                    break;
                case ClientMessage: {
                    if((Atom)event.xclient.data.l[0] == wm_delete_window) {
                        log_print(LOG_INFO, "Close requested by the window manager");
                        context->close_requested = true;
                    }
                } break;
                case FocusIn: {
                    log_print(LOG_PLATFORM, "Window focus gained");
                } break;
                case FocusOut: {
                    log_print(LOG_PLATFORM, "Window focus lost");
                    platform_push_event(&context->platform, (PlatformEvent){ .type = PLATFORM_EVENT_DEFOCUS });
                } break;
                case MotionNotify: {
                    context->platform.mouse_position = iv2_new(event.xmotion.x, context->platform.window_size.y - event.xmotion.y);
                } break;
                case ButtonPress:
                case ButtonRelease: {
                    // X buttons 4-7 are the scroll wheel: up, down, left, right. Each
                    // notch is a press and release; only the press is reported.
                    if(event.xbutton.button >= Button4 && event.xbutton.button <= 7) {
                        if(event.type == ButtonPress) {
                            u32 wheel = event.xbutton.button;
                            v2 delta = wheel == Button4 ? v2_new(0.0f, 1.0f)
                                     : wheel == Button5 ? v2_new(0.0f, -1.0f)
                                     : wheel == 6       ? v2_new(-1.0f, 0.0f)
                                     :                    v2_new(1.0f, 0.0f);
                            iv2 position = iv2_new(event.xbutton.x, context->platform.window_size.y - event.xbutton.y);
                            platform_push_event(&context->platform, (PlatformEvent){
                                .type = PLATFORM_EVENT_MOUSE_SCROLL, .scroll = { .delta = delta, .position = position } });
                        }
                        break;
                    }
                    if(event.xbutton.button < Button1 || event.xbutton.button > Button3) {
                        log_print(LOG_PLATFORM_VERBOSE, "Unmapped mouse button %u", event.xbutton.button);
                        break;
                    }
                    PlatformMouseButton button = event.xbutton.button == Button1 ? PLATFORM_MOUSE_BUTTON_LEFT
                                               : event.xbutton.button == Button2 ? PLATFORM_MOUSE_BUTTON_MIDDLE
                                               : PLATFORM_MOUSE_BUTTON_RIGHT;
                    iv2 position = iv2_new(event.xbutton.x, context->platform.window_size.y - event.xbutton.y);
                    context->platform.mouse_position = position;
                    platform_push_event(&context->platform, (PlatformEvent){
                        .type = event.type == ButtonPress ? PLATFORM_EVENT_MOUSE_DOWN : PLATFORM_EVENT_MOUSE_UP,
                        .mouse = { .button = button, .position = position } });
                } break;
                case ConfigureNotify: {
                    // ConfigureNotify also fires on moves, so only flag real size changes
                    XWindowAttributes window_attributes;
                    XGetWindowAttributes(context->display, context->window, &window_attributes);
                    iv2 window_size = iv2_new(window_attributes.width, window_attributes.height);
                    if(!iv2_eq(window_size, context->platform.window_size)) {
                        log_print(LOG_PLATFORM, "Window resized %ix%i -> %ix%i",
                                  context->platform.window_size.x, context->platform.window_size.y, window_size.x, window_size.y);
                        context->platform.window_size = window_size;
                    }
                } break;
                case KeyPress: {
                    u32 keysym = XLookupKeysym(&(event.xkey), 0);
                    PlatformKey key = platform_key_from_xlib_keysym(keysym);
                    if(key == PLATFORM_KEY_NONE) {
                        log_print(LOG_PLATFORM_VERBOSE, "Unmapped key: keysym 0x%x", keysym);
                    }
                    u32 modifiers = platform_modifiers_from_xlib_state(event.xkey.state);
                    platform_push_event(&context->platform, (PlatformEvent){
                        .type = next_key_press_repeats ? PLATFORM_EVENT_KEYREPEAT : PLATFORM_EVENT_KEYDOWN,
                        .modifiers = modifiers, .key = key });
                    next_key_press_repeats = false;

                    // Text input. XLookupString yields Latin-1, whose bytes are code points.
                    // TODO: UTF-8 input through an input method (XIM and Xutf8LookupString)
                    char text[8];
                    i32 text_len = XLookupString(&(event.xkey), text, sizeof(text), NULL, NULL);
                    for(i32 i = 0; i < text_len; i++) {
                        platform_push_event(&context->platform, (PlatformEvent){
                            .type = PLATFORM_EVENT_CHAR, .modifiers = modifiers, .codepoint = (u8)text[i] });
                    }
                } break;
                case KeyRelease: {
                    // X11 auto-repeats a held key as release and press pairs with the same
                    // time. Turning that off would turn it off for the user's whole X11
                    // session, so instead the pair is detected here: the release is dropped
                    // and the press is reported as a repeat.
                    bool is_repeat_key = false;
                    if (XPending(context->display)) {
                        XEvent next_event;
                        XPeekEvent(context->display, &next_event);
                        if (next_event.type == KeyPress && next_event.xkey.time == event.xkey.time 
                        && next_event.xkey.keycode == event.xkey.keycode) {
                            is_repeat_key = true;
                        }
                    }
                    if(is_repeat_key) {
                        next_key_press_repeats = true;
                    } else {
                        u64 keysym = XLookupKeysym(&(event.xkey), 0);
                        PlatformKey key = platform_key_from_xlib_keysym(keysym);
                        platform_push_event(&context->platform, (PlatformEvent){
                            .type = PLATFORM_EVENT_KEYUP, .modifiers = platform_modifiers_from_xlib_state(event.xkey.state), .key = key });
                    }
                } break;
                default: break;
            }
        }
        evdev_poll(context);
        profile_end(PROFILE_PLATFORM_EVENTS);

        // Update game, which writes the frame straight into GPU memory
        profile_begin(PROFILE_GAME_UPDATE);
        context->game.update(game_stack.memory, asset_pack_data, &render_frame, &context->platform);
        profile_end(PROFILE_GAME_UPDATE);

        // Update ALSA sound. An underrun (-EPIPE) stops the stream until it's recovered.
        profile_begin(PROFILE_AUDIO);
        snd_pcm_sframes_t available;
        snd_pcm_sframes_t delay;
        i32 avail_result = snd_pcm_avail_delay(context->alsa_pcm, &available, &delay);
        if(avail_result == -EPIPE) {
            log_print(LOG_WARN, "ALSA underrun, recovering");
            ALSA_VERIFY(snd_pcm_recover(context->alsa_pcm, avail_result, 1));
            ALSA_VERIFY(snd_pcm_avail_delay(context->alsa_pcm, &available, &delay));
        } else {
            ALSA_VERIFY(avail_result);
        }
        if(frame_count > 0 && delay < AUDIO_LATENCY_FRAMES / 4) {
            log_print(LOG_WARN, "Audio nearly drained: %li frames queued, target %u", (long)delay, (u32)AUDIO_LATENCY_FRAMES);
        }
        i32 frames_len = AUDIO_LATENCY_FRAMES - delay;
        if(frames_len > available) {
            log_print(LOG_WARN, "ALSA only has room for %li frames, wanted %i", (long)available, frames_len);
            frames_len = (i32)available;
        }
        log_print(LOG_AUDIO_VERBOSE, "ALSA available %li, delay %li, writing %i frames", (long)available, (long)delay, frames_len);
        if(frames_len > 0) {
            f32* samples = (f32*)stack_alloc(&platform_frame_stack, frames_len * AUDIO_BYTES_PER_FRAME);
            context->game.audio_callback(game_stack.memory, asset_pack_data, samples, frames_len);
            snd_pcm_sframes_t frames_written = snd_pcm_writei(context->alsa_pcm, samples, frames_len);
            if(frames_written == -EPIPE) {
                log_print(LOG_WARN, "ALSA underrun on write, recovering");
                ALSA_VERIFY(snd_pcm_recover(context->alsa_pcm, (i32)frames_written, 1));
            } else if(frames_written < 0) {
                ALSA_VERIFY((i32)frames_written);
            } else if(frames_written != frames_len) {
                log_print(LOG_WARN, "ALSA wrote %li of %i frames", (long)frames_written, frames_len);
            }
        }
        profile_end(PROFILE_AUDIO);

        // Render
        profile_begin(PROFILE_RENDER_END);
        vk_frame_end(vk, &render_frame);
        profile_end(PROFILE_RENDER_END);

        // Prepare for next frame
        stack_clear(&platform_frame_stack);
        game_library_update(&context->game);
        context->platform.events_len = 0;

        // Frame timing
        u64 frame_end = profile_time_ns();
        f64 frame_time = (f64)(frame_end - frame_start) / 1e9;
        frame_start = frame_end;
        frame_time_total += frame_time;
        frame_count++;
        log_print(LOG_RENDER_VERBOSE, "Frame %" PRIu64 ": %.2f ms", frame_count, frame_time * 1000.0);
        profile_end(PROFILE_FRAME);
        profile_frame_end();
    }

    // Shut down
    log_print(LOG_INFO, "Shutting down after %" PRIu64 " frames, average frame time %.2f ms",
              frame_count, frame_count > 0 ? frame_time_total / (f64)frame_count * 1000.0 : 0.0);
    profile_log(LOG_INFO);
    stack_log_usage(&root_stack);
    stack_log_usage(&game_stack);
    stack_log_usage(&render_stack);
    stack_log_usage(&render_scratch_stack);
    stack_log_usage(&platform_frame_stack);
    return 0;
}
#endif

// WEB
#if PLATFORM == PLATFORM_WEB

#endif
