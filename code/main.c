// config.h is provided by the specific game
#include "config.h"

#define CSM_IMPLEMENTATION
#define CSM_INCLUDE_VK
#define BUFFER_DEBUG true
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

// Audio settings
#ifndef AUDIO_SAMPLE_RATE
#define AUDIO_SAMPLE_RATE 44100
#endif

// WINDOWS
#if PLATFORM == PLATFORM_WINDOWS

typedef struct {
    i32 tmp;
} Context;

LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch(uMsg) {
        default: break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow) {
    // Create window
    // NOW: define window_proc
    WNDCLASS window_class = {};
    window_class.lpfnWndProc   = window_proc;
    window_class.hInstance     = hInstance;
    window_class.lpszClassName = GAME_NAME;
    RegisterClass(&window_class);

    HWND hwnd = CreateWindowEx(
        0, GAME_NAME, GAME_NAME,
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, hInstance, NULL);
    if(hwnd == NULL) {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    // Initialize renderer
    // NOW: Allocate from a root stack and run the game loop, as on Linux.
    void* mem = malloc(RENDER_STACK_SIZE);
    Stack render_stack = stack_from_memory(mem, RENDER_STACK_SIZE, string_const("Renderer"));
    VkContext* vk = (VkContext*)stack_alloc_zero(&render_stack, sizeof(VkContext));
    Stack scratch_stack = stack_from_stack(&render_stack, MEGABYTE, string_const("RendererScratch"));
    RECT client_rect;
    GetClientRect(hwnd, &client_rect);
    VkPlatformWindow vk_window = { .hwnd = hwnd, .hinstance = hInstance };
    vk_init(vk, string_const(GAME_NAME), vk_window, iv2_new(client_rect.right, client_rect.bottom), &scratch_stack);

    // Main loop
    MSG msg = {};
    while(GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
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

#define ALSA_VERIFY(alsa_function) { \
    i32 alsa_error; \
    if((alsa_error = alsa_function) < 0) { \
        log_exit("ALSA error: %s", snd_strerror(alsa_error)); \
    } \
} 

typedef struct {
    bool                close_requested;
    Log                 log;

    Display*            display;
    Window              window;
    snd_pcm_t*          alsa_pcm;
    u32                 alsa_latency_samples;
    Platform            platform;

    DynamicLibrary      game;
    GameInitFunction*   game_init;
    GameUpdateFunction* game_update;
    GameAudioCallback*  game_audio_callback;
} Context;

void update_game_library(Context* context) {
    if(dynamic_library_update(&context->game)) {
        log_print(LOG_HOT_RELOAD, "Loaded game library " STRING_FMT, STRING_ARG(context->game.path));
        context->game_init           = dynamic_library_load_function(context->game, string_const("game_init"));
        context->game_update         = dynamic_library_load_function(context->game, string_const("game_update"));
        context->game_audio_callback = dynamic_library_load_function(context->game, string_const("game_audio_callback"));
    }
}

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
        default: return PLATFORM_KEY_NONE;
    }
    return PLATFORM_KEY_NONE;
}

i32 main(i32 argc, char** argv) {
    // Allocate memory
    void* mem = malloc(ROOT_MEMORY_SIZE);
    Stack root_stack = stack_from_memory(mem, ROOT_MEMORY_SIZE, string_const("Root"));
    Context* context = (Context*)stack_alloc(&root_stack, sizeof(Context));
    memset(context, 0, sizeof(Context));
    Stack game_stack           = stack_from_stack(&root_stack, GAME_STACK_SIZE, string_const("Game"));
    Stack render_stack         = stack_from_stack(&root_stack, RENDER_STACK_SIZE, string_const("Renderer"));
    Stack platform_frame_stack = stack_from_stack(&root_stack, PLATFORM_FRAME_STACK_SIZE, string_const("PlatformFrame"));

    // Initialize logging
    log_init(&context->log, LOG_TARGETS, string_const(LOG_FILE_PATH));
    log_bind(&context->log);
    context->platform.log = &context->log;

    // Init Xlib window
    context->display = XOpenDisplay("");
    assert(context->display != NULL);

    i32 screen = DefaultScreen(context->display);
    Window root_window = RootWindow(context->display, screen);
    XSetWindowAttributes set_window_attributes = {};
    set_window_attributes.background_pixmap = None;
    set_window_attributes.border_pixel = 0;
    set_window_attributes.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask | KeyReleaseMask | PointerMotionMask | ButtonPressMask | ButtonReleaseMask;

    u32 window_width = 1280;
    u32 window_height = 720;
    context->window = XCreateWindow(context->display, root_window, 0, 0, window_width, window_height, 0, DefaultDepth(context->display, screen), InputOutput, DefaultVisual(context->display, screen), CWBorderPixel | CWEventMask, &set_window_attributes);
    if(context->window == 0) { panic(); }
    XStoreName(context->display, context->window, GAME_NAME);
    XMapWindow(context->display, context->window);
    context->platform.window_size = iv2_new(window_width, window_height);

    // Ask the window manager to tell us when the window is closed, rather than killing the connection
    Atom wm_delete_window = XInternAtom(context->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(context->display, context->window, &wm_delete_window, 1);

    // Initialize ALSA audio
    u32 sample_rate = AUDIO_SAMPLE_RATE;
    snd_pcm_hw_params_t* hw_params;
    ALSA_VERIFY(snd_pcm_open(&context->alsa_pcm, "default", SND_PCM_STREAM_PLAYBACK, 0));
    snd_pcm_hw_params_alloca(&hw_params);
    ALSA_VERIFY(snd_pcm_hw_params_any(context->alsa_pcm, hw_params));
    ALSA_VERIFY(snd_pcm_hw_params_set_access(context->alsa_pcm, hw_params, SND_PCM_ACCESS_RW_NONINTERLEAVED));
    //ALSA_VERIFY(snd_pcm_hw_params_set_format(context->alsa_pcm, hw_params, SND_PCM_FORMAT_S16_LE));
    ALSA_VERIFY(snd_pcm_hw_params_set_format(context->alsa_pcm, hw_params, SND_PCM_FORMAT_FLOAT_LE));
    ALSA_VERIFY(snd_pcm_hw_params_set_rate_near(context->alsa_pcm, hw_params, &sample_rate, 0));
    ALSA_VERIFY(snd_pcm_hw_params_set_channels(context->alsa_pcm, hw_params, 1));
    ALSA_VERIFY(snd_pcm_hw_params(context->alsa_pcm, hw_params));
    ALSA_VERIFY(snd_pcm_prepare(context->alsa_pcm));
    context->alsa_latency_samples = sample_rate / 12;

    // Initialize renderer and game
    VkContext* vk = (VkContext*)stack_alloc_zero(&render_stack, sizeof(VkContext));
    Stack render_scratch_stack = stack_from_stack(&render_stack, MEGABYTE, string_const("RendererScratch"));
    VkPlatformWindow vk_window = { .display = context->display, .window = context->window };
    vk_init(vk, string_const(GAME_NAME), vk_window, context->platform.window_size, &render_scratch_stack);

    dynamic_library_init(&context->game, string_const(GAME_LIB_NAME));
    update_game_library(context);
    RenderSetup* render_setup = (RenderSetup*)stack_alloc_zero(&render_stack, sizeof(RenderSetup));
    context->game_init(game_stack.memory, asset_pack_data, render_setup, &context->platform);
    vk_load_assets(vk, render_setup);

    // Loop
    while(context->close_requested == false) {
        // Poll Xlib events
        while(XPending(context->display)) {
            XEvent event;
            XNextEvent(context->display, &event);
            switch(event.type) {
                case Expose:
                    break;
                case ClientMessage: {
                    if((Atom)event.xclient.data.l[0] == wm_delete_window) {
                        context->close_requested = true;
                    }
                } break;
                case ConfigureNotify: {
                    // ConfigureNotify also fires on moves, so only flag real size changes
                    XWindowAttributes window_attributes;
                    XGetWindowAttributes(context->display, context->window, &window_attributes);
                    iv2 window_size = iv2_new(window_attributes.width, window_attributes.height);
                    if(!iv2_eq(window_size, context->platform.window_size)) {
                        context->platform.window_size = window_size;
                        context->platform.window_size_updated_this_frame = true;
                    }
                } break;
                case KeyPress: {
                    u32 keysym = XLookupKeysym(&(event.xkey), 0);
                    PlatformKey key = platform_key_from_xlib_keysym(keysym);
                    platform_push_event(&context->platform, (PlatformEvent){ .type = PLATFORM_EVENT_KEYDOWN, .key = key });
                } break;
                case KeyRelease: {
                    // X11 natively repeats key events when the key is held down. We could turn
                    // that off, but it turns it off globally for the user's X11 session, which
                    // is unacceptable of course. Here we are ignoring them manually.
                    bool is_repeat_key = false;
                    if (XPending(context->display)) {
                        XEvent next_event;
                        XPeekEvent(context->display, &next_event);
                        if (next_event.type == KeyPress && next_event.xkey.time == event.xkey.time 
                        && next_event.xkey.keycode == event.xkey.keycode) {
                            XNextEvent(context->display, &next_event);
                            is_repeat_key = true;
                        }
                    }
                    if(!is_repeat_key) {
                        u64 keysym = XLookupKeysym(&(event.xkey), 0);
                        PlatformKey key = platform_key_from_xlib_keysym(keysym);
                        platform_push_event(&context->platform, (PlatformEvent){ .type = PLATFORM_EVENT_KEYUP, .key = key });
                    }
                } break;
                default: break;
            }
        }

        // Update game, which writes the frame straight into GPU memory
        RenderFrame render_frame;
        vk_frame_begin(vk, context->platform.window_size, context->platform.window_size_updated_this_frame, &render_frame);
        context->game_update(game_stack.memory, &render_frame, &context->platform);

        // Update ALSA sound
        snd_pcm_sframes_t available;
        snd_pcm_sframes_t delay;
        ALSA_VERIFY(snd_pcm_avail_delay(context->alsa_pcm, &available, &delay));
        // TODO: Make sure we have enough frames available.
        i32 sample_count = context->alsa_latency_samples - delay;
        if(sample_count > 0) {
            f32* sample_buffer = (f32*)stack_alloc(&platform_frame_stack, sample_count * sizeof(f32));
            context->game_audio_callback(game_stack.memory, sample_buffer, sample_count);
            i32 frames_written = snd_pcm_writen(context->alsa_pcm, (void**)&sample_buffer, sample_count);
            assert(frames_written == sample_count);
        }

        // Render
        vk_frame_end(vk, &render_frame);

        // Prepare for next frame
        stack_clear(&platform_frame_stack);
        update_game_library(context);
        context->platform.window_size_updated_this_frame = false;
        context->platform.events_len = 0;
    }
    return 0;
}
#endif

// WEB
#if PLATFORM == PLATFORM_WEB

#endif
