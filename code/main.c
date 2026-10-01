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

typedef struct {
    bool          close_requested;
    Log           log;
    Profile       profile;
    BufferTracker buffers;
    HWND          hwnd;
    Platform      platform;
    GameLibrary   game;
    Wasapi        audio;
} Context;

PlatformKey platform_key_from_win32_virtual_key(WPARAM virtual_key) {
    switch(virtual_key) {
        case 'W': {
            return PLATFORM_KEY_W;
        } break;
        case 'A': {
            return PLATFORM_KEY_A;
        } break;
        case 'S': {
            return PLATFORM_KEY_S;
        } break;
        case 'D': {
            return PLATFORM_KEY_D;
        } break;
        case 'Q': {
            return PLATFORM_KEY_Q;
        } break;
        case 'E': {
            return PLATFORM_KEY_E;
        } break;
        case 'G': {
            return PLATFORM_KEY_G;
        } break;
        case 'M': {
            return PLATFORM_KEY_M;
        } break;
        case 'R': {
            return PLATFORM_KEY_R;
        } break;
        case VK_UP: {
            return PLATFORM_KEY_UP;
        } break;
        case VK_LEFT: {
            return PLATFORM_KEY_LEFT;
        } break;
        case VK_DOWN: {
            return PLATFORM_KEY_DOWN;
        } break;
        case VK_RIGHT: {
            return PLATFORM_KEY_RIGHT;
        } break;
        case VK_ESCAPE: {
            return PLATFORM_KEY_ESCAPE;
        } break;
        case VK_TAB: {
            return PLATFORM_KEY_TAB;
        } break;
        case VK_SPACE: {
            return PLATFORM_KEY_SPACE;
        } break;
        case VK_RETURN: {
            return PLATFORM_KEY_ENTER;
        } break;
        case VK_F3: {
            return PLATFORM_KEY_F3;
        } break;
        case VK_BACK: {
            return PLATFORM_KEY_BACKSPACE;
        } break;
        case VK_DELETE: {
            return PLATFORM_KEY_DELETE;
        } break;
        case 'H': {
            return PLATFORM_KEY_H;
        } break;
        case 'J': {
            return PLATFORM_KEY_J;
        } break;
        case 'K': {
            return PLATFORM_KEY_K;
        } break;
        case 'L': {
            return PLATFORM_KEY_L;
        } break;
        default: return PLATFORM_KEY_NONE;
    }
    return PLATFORM_KEY_NONE;
}

// Modifier keys held as of the message being processed
u32 platform_modifiers_from_win32() {
    u32 modifiers = PLATFORM_MODIFIER_NONE;
    if(GetKeyState(VK_SHIFT)   & 0x8000) modifiers |= PLATFORM_MODIFIER_SHIFT;
    if(GetKeyState(VK_CONTROL) & 0x8000) modifiers |= PLATFORM_MODIFIER_CTRL;
    if(GetKeyState(VK_MENU)    & 0x8000) modifiers |= PLATFORM_MODIFIER_ALT;
    return modifiers;
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
        case WM_KEYDOWN: {
            PlatformKey key = platform_key_from_win32_virtual_key(wparam);
            if(key == PLATFORM_KEY_NONE) {
                log_print(LOG_PLATFORM_VERBOSE, "Unmapped key: virtual key 0x%x", (u32)wparam);
            }
            // Bit 30 is set if the key was already down, i.e. this is an auto-repeat
            PlatformEventType type = (lparam & (1 << 30)) ? PLATFORM_EVENT_KEYREPEAT : PLATFORM_EVENT_KEYDOWN;
            platform_push_event(&context->platform, (PlatformEvent){
                .type = type, .modifiers = platform_modifiers_from_win32(), .key = key });
            return 0;
        } break;
        case WM_KEYUP: {
            PlatformKey key = platform_key_from_win32_virtual_key(wparam);
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

        // Pump Win32 messages, which window_proc turns into platform state and events
        profile_begin(PROFILE_PLATFORM_EVENTS);
        MSG message;
        while(PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
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

#define ALSA_VERIFY(alsa_function) { \
    i32 alsa_error; \
    if((alsa_error = alsa_function) < 0) { \
        log_exit("ALSA error: %s", snd_strerror(alsa_error)); \
    } \
} 

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
} Context;

PlatformKey platform_key_from_xlib_keysym(u32 keysym) {
    switch(keysym) {
        case XK_w: {
            return PLATFORM_KEY_W;
        } break;
        case XK_a: {
            return PLATFORM_KEY_A;
        } break;
        case XK_s: {
            return PLATFORM_KEY_S;
        } break;
        case XK_d: {
            return PLATFORM_KEY_D;
        } break;
        case XK_q: {
            return PLATFORM_KEY_Q;
        } break;
        case XK_e: {
            return PLATFORM_KEY_E;
        } break;
        case XK_g: {
            return PLATFORM_KEY_G;
        } break;
        case XK_m: {
            return PLATFORM_KEY_M;
        } break;
        case XK_r: {
            return PLATFORM_KEY_R;
        } break;
        case XK_Up: {
            return PLATFORM_KEY_UP;
        } break;
        case XK_Left: {
            return PLATFORM_KEY_LEFT;
        } break;
        case XK_Down: {
            return PLATFORM_KEY_DOWN;
        } break;
        case XK_Right: {
            return PLATFORM_KEY_RIGHT;
        } break;
        case XK_Escape: {
            return PLATFORM_KEY_ESCAPE;
        } break;
        case XK_Tab: {
            return PLATFORM_KEY_TAB;
        } break;
        case XK_space: {
            return PLATFORM_KEY_SPACE;
        } break;
        case XK_Return: {
            return PLATFORM_KEY_ENTER;
        } break;
        case XK_F3: {
            return PLATFORM_KEY_F3;
        } break;
        case XK_BackSpace: {
            return PLATFORM_KEY_BACKSPACE;
        } break;
        case XK_Delete: {
            return PLATFORM_KEY_DELETE;
        } break;
        case XK_h: {
            return PLATFORM_KEY_H;
        } break;
        case XK_j: {
            return PLATFORM_KEY_J;
        } break;
        case XK_k: {
            return PLATFORM_KEY_K;
        } break;
        case XK_l: {
            return PLATFORM_KEY_L;
        } break;
        default: return PLATFORM_KEY_NONE;
    }
    return PLATFORM_KEY_NONE;
}

// Modifier keys held as of a key event, from its state mask
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
