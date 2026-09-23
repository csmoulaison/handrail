#define CSM_IMPLEMENTATION
#define HANDRAIL_INCLUDE_VULKAN
#define BUFFER_DEBUG true
#define BUFFER_VERBOSE false
#include "handrail/core.h"

//#include "generated/asset_data.c"
#include "generated/asset_handles.c"

// render.c is provided by the specific game
//#include "render.c"

// Memory sizes
#ifndef PLATFORM_STACK_SIZE
#define PLATFORM_STACK_SIZE (MEGABYTE * 4)
#endif
#ifndef GAME_STACK_SIZE
#define GAME_STACK_SIZE (MEGABYTE * 4)
#endif
#ifndef RENDER_STACK_SIZE
#define RENDER_STACK_SIZE (MEGABYTE * 4)
#endif
#ifndef RENDER_FRAME_STACK_SIZE
#define RENDER_FRAME_STACK_SIZE (MEGABYTE * 4)
#endif
#ifndef PLATFORM_FRAME_STACK_SIZE
#define PLATFORM_FRAME_STACK_SIZE (MEGABYTE * 4)
#endif

#define ROOT_MEMORY_SIZE (sizeof(Context) + PLATFORM_STACK_SIZE + GAME_STACK_SIZE + RENDER_STACK_SIZE + RENDER_FRAME_STACK_SIZE + PLATFORM_FRAME_STACK_SIZE)

// Audio settings
#ifndef AUDIO_SAMPLE_RATE
#define AUDIO_SAMPLE_RATE 48000
#endif
#ifndef AUDIO_BITS_PER_SAMPLE
#define AUDIO_BITS_PER_SAMPLE 32
#endif
#ifndef AUDIO_CHANNEL_COUNT
#define AUDIO_CHANNEL_COUNT 1
#endif

#define AUDIO_BYTES_PER_SAMPLE (AUDIO_BITS_PER_SAMPLE / 8)

// WINDOWS
#if PLATFORM == PLATFORM_WINDOWS

#include <objbase.h>
#include <uuids.h>
#include <avrt.h>
#include <audioclient.h>
#include <mmdeviceapi.h>

DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xbcde0395, 0xe52f, 0x467c, 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e);
DEFINE_GUID(IID_IMMDeviceEnumerator,  0xa95664d2, 0x9614, 0x4f35, 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6);
DEFINE_GUID(IID_IAudioClient,         0x1cb9ad4c, 0xdbfa, 0x4c32, 0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2);
DEFINE_GUID(IID_IAudioClient3,        0x7ed4ee07, 0x8e67, 0x4cd4, 0x8c, 0x1a, 0x2b, 0x7a, 0x59, 0x87, 0xad, 0x42);
DEFINE_GUID(IID_IAudioRenderClient,   0xf294acfc, 0x3146, 0x4483, 0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2);

#define HR_VERIFY(statement) { HRESULT hr_res = 0; if((hr_res = (statement)) != 0) { printf("ERROR!!\n"); print_hresult_string(hr_res); exit(1); } }

typedef struct {
    WAVEFORMATEX* format;    
    // Circular buffer for audio samples
    void*         buffer;
    // Buffer size, must be a power of 2
    u32           size;
    // Buffer capacity in samples 
    u64           sample_count;
    // Count of samples were played back since previous LockBuffer call
    u64           play_samples;
    // Samples used from buffer
    u32           used;
    // Offset to read from buffer
    LONG          read_offset;
    // Offset up to what buffer is currently being used
    LONG          lock_offset;
    // Offset up to what buffer is filled
    LONG          write_offset;
    // False before first use of BufferLock
    bool          first_lock;
    // Output buffer size in bytes
    u32           output_size;

    IAudioClient* client;
    HANDLE        event;
    HANDLE        thread;
    LONG          stop;
    SRWLOCK       lock;
    u8*           buffer_1;
    u8*           buffer_2;
} Wasapi;

typedef struct {
    Stack  root_stack;
    Stack  platform_stack;
    Stack  game_stack;
    Stack  render_stack;
    Stack  render_frame_stack;
    Stack  platform_frame_stack;
    Wasapi audio;
    bool   quit_requested;
} Context;

void print_hresult_string(HRESULT res) {
    LPTSTR str = (LPSTR)malloc(4096);
    wsprintf(str, "Error code %u", res);
    DWORD size = FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        res,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        str,
        4096,
        NULL);
    if(size == 0) {
        printf("Couldn't get HRESULT string\n");
    }
    printf("HRESULT %s\n", str);
    return;
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Context* context = NULL;
    if (uMsg == WM_CREATE) {
        CREATESTRUCT* create = (CREATESTRUCT*)lParam;
        context = (Context*)create->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)context);
    } else {
        context = (Context*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    switch(uMsg) {
        case WM_CLOSE: {
            DestroyWindow(hwnd);
            return 0;
        } break;
        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        } break;
        default: break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

static DWORD CALLBACK wasapi_audio_thread(LPVOID arg) {
    Wasapi* audio = (Wasapi*)arg;

    DWORD task = 0;
    HANDLE handle = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task);
    assert(handle != NULL);

    IAudioRenderClient* playback = NULL;
    HR_VERIFY(IAudioClient_GetService(audio->client, &IID_IAudioRenderClient, (LPVOID*)&playback));

    u32 buffer_samples = 0;
    HR_VERIFY(IAudioClient_GetBufferSize(audio->client, &buffer_samples));
    HR_VERIFY(IAudioClient_Start(audio->client));

    u32 buffer_mask = audio->size - 1;

    while(WaitForSingleObject(audio->event, INFINITE) == WAIT_OBJECT_0) {
        if(InterlockedExchange(&audio->stop, FALSE)) {
            break;
        }

        u32 pad_samples = 0;
        HR_VERIFY(IAudioClient_GetCurrentPadding(audio->client, &pad_samples));

        u8* output = NULL;
        u32 max_output_samples = buffer_samples - pad_samples;
        HR_VERIFY(IAudioRenderClient_GetBuffer(playback, max_output_samples, &output));

        // available: samples/bytes available to read from buffer,
        // write: samples/bytes to write to buffer
        AcquireSRWLockExclusive(&audio->lock);
        u32 available_size = audio->write_offset - audio->read_offset;
        u32 available_samples = available_size / AUDIO_BYTES_PER_SAMPLE;
        u32 write_samples = min(available_samples, max_output_samples);
        u32 write_size = write_samples * AUDIO_BYTES_PER_SAMPLE;
        audio->lock_offset = audio->read_offset + write_size;
        DWORD flags = 0;
        if(write_samples == 0) {
            // Write silence if no write samples
            write_samples = max_output_samples;
            flags = AUDCLNT_BUFFERFLAGS_SILENT;
        }
        audio->used += write_samples;
        ReleaseSRWLockExclusive(&audio->lock);

        memcpy(output, audio->buffer_1 + (audio->read_offset & buffer_mask), write_size);
        InterlockedAdd(&audio->read_offset, write_size);
        HR_VERIFY(IAudioRenderClient_ReleaseBuffer(playback, write_samples, flags));
    }

    HR_VERIFY(IAudioClient_Stop(audio->client));
    IAudioRenderClient_Release(playback);
    AvRevertMmThreadCharacteristics(handle);
    return 0;
}

void win_init(Context* context, HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow) {
    // Console logging
    assert(AllocConsole());
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
    freopen_s(&f, "CONIN$",  "r", stdin);
    
    // Allocate memory
    void* mem = VirtualAlloc(NULL, ROOT_MEMORY_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    assert(mem);
    context->root_stack           = stack_from_memory(mem, ROOT_MEMORY_SIZE, string_const("Root"));
    context->platform_stack       = stack_from_stack(&context->root_stack, PLATFORM_STACK_SIZE, string_const("Platform"));
    context->game_stack           = stack_from_stack(&context->root_stack, GAME_STACK_SIZE, string_const("Game"));
    context->render_stack         = stack_from_stack(&context->root_stack, RENDER_STACK_SIZE, string_const("Render"));
    context->render_frame_stack   = stack_from_stack(&context->root_stack, RENDER_FRAME_STACK_SIZE, string_const("RenderFrame"));
    context->platform_frame_stack = stack_from_stack(&context->root_stack, PLATFORM_FRAME_STACK_SIZE, string_const("PlatformFrame"));

    // Create window
    WNDCLASS window_class = {};
    window_class.lpfnWndProc   = window_proc;
    window_class.hInstance     = hInstance;
    window_class.lpszClassName = GAME_NAME;
    RegisterClass(&window_class);

    HWND hwnd = CreateWindowExA(
        0, GAME_NAME, GAME_NAME,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, hInstance, context);
    assert(hwnd != NULL);
    //ShowWindow(hwnd, nCmdShow);

    // NOW: Vulkan, Audio, Input, Game DLL, Update
    vk_init(GAME_NAME, &context->platform_frame_stack);

    // WASAPI audio
    Wasapi* audio = &context->audio;
    // TODO: second arg here, look into.
    HR_VERIFY(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED));

    // Enumerate playback devices and choose default
    IMMDeviceEnumerator* enumerator = NULL;
    HR_VERIFY(CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, (LPVOID*)&enumerator));
    IMMDevice* device = NULL;
    HR_VERIFY(IMMDeviceEnumerator_GetDefaultAudioEndpoint(enumerator, eRender, eConsole, &device));
    IMMDeviceEnumerator_Release(enumerator);

    // Create audio client
    HR_VERIFY(IMMDevice_Activate(device, &IID_IAudioClient, CLSCTX_ALL, NULL, (LPVOID*)&audio->client));
    IMMDevice_Release(device);
  
    WAVEFORMATEXTENSIBLE format_ext = {};
    format_ext.Format.wFormatTag           = WAVE_FORMAT_EXTENSIBLE,
    format_ext.Format.nChannels            = (WORD)AUDIO_CHANNEL_COUNT;
    format_ext.Format.nSamplesPerSec       = (WORD)AUDIO_SAMPLE_RATE;
    format_ext.Format.nAvgBytesPerSec      = (DWORD)(AUDIO_SAMPLE_RATE * AUDIO_CHANNEL_COUNT * AUDIO_BYTES_PER_SAMPLE);
    format_ext.Format.nBlockAlign          = (WORD)(AUDIO_CHANNEL_COUNT * AUDIO_BYTES_PER_SAMPLE);
    format_ext.Format.wBitsPerSample       = (WORD)AUDIO_BITS_PER_SAMPLE;
    format_ext.Format.cbSize               = sizeof(format_ext) - sizeof(format_ext.Format);
    format_ext.Samples.wValidBitsPerSample = (WORD)AUDIO_BITS_PER_SAMPLE;
    format_ext.dwChannelMask               = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    format_ext.SubFormat                   = MEDIASUBTYPE_IEEE_FLOAT;
    audio->format = stack_alloc(&context->platform_stack, sizeof(format_ext));
    memcpy(audio->format, &format_ext, sizeof(format_ext));

    // Try to initialize client3 with newer functionality
    bool client_initialized = false;
    IAudioClient3* client3 = NULL;
    if(SUCCEEDED(IAudioClient_QueryInterface(audio->client, &IID_IAudioClient3, (LPVOID*)&client3))) {
        u32 period_default = 0;
        u32 period_fundamental = 0;
        u32 period_min = 0;
        u32 period_max = 0;
        if(SUCCEEDED(IAudioClient3_GetSharedModeEnginePeriod(client3, audio->format, &period_default, &period_fundamental, &period_min, &period_max))) {
            if(SUCCEEDED(IAudioClient3_InitializeSharedAudioStream(client3, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, period_min, audio->format, NULL))) {
                client_initialized = true;                
            }
        }
        IAudioClient3_Release(client3);
    }
    // Or not
    if(!client_initialized) {
        REFERENCE_TIME duration = {};
        HR_VERIFY(IAudioClient_GetDevicePeriod(audio->client, &duration, NULL));
        DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
        HR_VERIFY(IAudioClient_Initialize(audio->client, AUDCLNT_SHAREMODE_SHARED, flags, duration, 0, audio->format, NULL));
    }

    u32 buffer_samples = 0;
    HR_VERIFY(IAudioClient_GetBufferSize(audio->client, &buffer_samples));
    audio->output_size = buffer_samples * audio->format->nBlockAlign;
  
    // Event handle to wait on
    audio->event = CreateEventW(NULL, false, false, NULL);
    HR_VERIFY(IAudioClient_SetEventHandle(audio->client, audio->event));

    // Create a magic ring buffer with VirtualAlloc memory mapping trix.
    audio->size = u32_round_up_to_power_of_2(max(KILOBYTE * 64, audio->format->nAvgBytesPerSec));
    char* placeholder1 = VirtualAlloc2(NULL, NULL, 2 * audio->size, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, NULL, 0);
    char* placeholder2 = placeholder1 + audio->size;
    assert(placeholder1 != NULL);
    if(VirtualFree(placeholder1, audio->size, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER) == 0) {
        HRESULT err = GetLastError();
        panic();
    }
    HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, audio->size, NULL);
    assert(section != NULL);
    void* view1 = MapViewOfFile3(section, NULL, placeholder1, 0, audio->size, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0);
    void* view2 = MapViewOfFile3(section, NULL, placeholder2, 0, audio->size, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0);
    assert(view1 != NULL && view2 != NULL);

    audio->buffer       = NULL;    
    audio->sample_count = 0;
    audio->play_samples = 0;
    audio->buffer_1     = view1;
    audio->buffer_2     = view2;
    audio->used         = 0;
    audio->first_lock   = true;
    audio->read_offset  = 0;
    audio->lock_offset  = 0;
    audio->write_offset = 0;
    InterlockedExchange(&audio->stop, false);
    InitializeSRWLock(&audio->lock);
    // NOW: make audio its own struct.
    audio->thread = CreateThread(NULL, 0, &wasapi_audio_thread, audio, 0, NULL);

    // Actual memory freed only when unmapped
    VirtualFree(placeholder1, 0, MEM_RELEASE);
    VirtualFree(placeholder2, 0, MEM_RELEASE);
    CloseHandle(section);
}

i32 WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow) {
    // Init 
    Context context = {};
    win_init(&context, hInstance, hPrevInstance, lpCmdLine, nCmdShow);

    // TMP: sine wave t
    f64 sin_t = 0.0;

    // Loop
    while(!context.quit_requested) {
        // Event handling
        MSG msg = {};
        while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if(msg.message ==  WM_QUIT) {
                context.quit_requested = true;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if(context.quit_requested) {
            break;
        }

        // WASAPI audio
        // =====================================================================
        // Lock the buffer
        Wasapi* audio = &context.audio;

        AcquireSRWLockExclusive(&audio->lock);
        u32 used_size = audio->lock_offset - audio->read_offset;
        if(used_size < audio->output_size) {
            u32 available_size = audio->write_offset - audio->read_offset;
            used_size = min(audio->output_size, available_size);
            audio->lock_offset = audio->read_offset + used_size;
        }

        u32 write_size = audio->size - used_size;
        audio->write_offset = audio->lock_offset;
        audio->play_samples = audio->used;
        if(audio->first_lock) {
            audio->play_samples = 0;
            audio->first_lock = false;
        }
        audio->used = 0;
        ReleaseSRWLockExclusive(&audio->lock);

        audio->buffer = audio->buffer_1 + (audio->lock_offset & (audio->size - 1));
        audio->sample_count = write_size / AUDIO_BYTES_PER_SAMPLE;
        
        // Write 100ms or available space, whichever is smaller. Tune this
        // number to the max update time, otherwise audio will have
        // discontinuities
        u64 sample_count = min(AUDIO_SAMPLE_RATE / 10, audio->sample_count);
        if(sample_count > 0) {
            // Allocate temporary sample buffer or use the Wasapi one directly?
            //f32* sample_buffer = (f32*)stack_alloc(&platform_frame_stack, sample_count);
            f32* sample_buffer = (f32*)audio->buffer;

            // TODO: game layer callback
            //context->game_audio_callback(game_stack.memory, audio->sample_buffer, sample_count);

            // TMP: fill with sine wave with magic frequency number 
            f64 delta = (400.0 * (M_PI * 2)) / (double)AUDIO_SAMPLE_RATE;
            for(i32 i = 0; i < sample_count; i++) {
                sample_buffer[i * 2] = sin(sin_t) * 0.5;
                //audio->sample_buffer[i * 2 + 1] = sin(sin_t) * 0.1;
                sin_t += delta;
            }
        }

        // Unlock the buffer
        InterlockedAdd(&audio->write_offset, sample_count * AUDIO_BYTES_PER_SAMPLE);
    }
    
	return 0;
}

#endif

// LINUX
#if PLATFORM == PLATFORM_LINUX

#include <GL/glx.h>
typedef GLXContext(*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

#include <alsa/asoundlib.h>
#include <alloca.h>

#define ALSA_VERIFY(alsa_function) { \
	i32 alsa_error; \
	if((alsa_error = alsa_function) < 0) { \
		fprintf(stderr, "ALSA error: %s\n", snd_strerror(alsa_error)); \
		exit(1); \
	} \
} 

typedef struct {
    bool                close_requested;

    Display*            display;
    Window              window;
    snd_pcm_t*          alsa_pcm;
    u32                 alsaLatencySamples;
    Platform            platform;

    DynamicLibrary      game;
    GameInitFunction*   game_init;
    GameUpdateFunction* game_update;
    GameAudioCallback*  game_audio_callback;
} Context;

void update_game_library(Context* context) {
    if(dynamic_lib_update(&context->game)) {
        context->game_init           = dynamic_lib_load_function(context->game, string_const("game_init"));
        context->game_update         = dynamic_lib_load_function(context->game, string_const("game_update"));
        context->game_audio_callback = dynamic_lib_load_function(context->game, string_const("game_audio_callback"));
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
    Stack platform_stack       = stack_from_stack(&root_stack, PLATFORM_STACK_SIZE, string_const("Platform"));
    Stack game_stack           = stack_from_stack(&root_stack, GAME_STACK_SIZE, string_const("Game"));
    Stack render_stack         = stack_from_stack(&root_stack, RENDER_STACK_SIZE, string_const("Render"));
    Stack render_frame_stack   = stack_from_stack(&root_stack, RENDER_FRAME_STACK_SIZE, string_const("RenderFrame"));
    Stack platform_frame_stack = stack_from_stack(&root_stack, PLATFORM_FRAME_STACK_SIZE, string_const("PlatformFrame"));

    // Init Xlib/GLX window
	context->display = XOpenDisplay("");
	assert(context->display != NULL);

	// Will factor out GLX stuff for other API integrations.
	i32 glx_version_major;
	i32 glx_version_minor;
	assert(glXQueryVersion(context->display, &glx_version_major, &glx_version_minor) != 0 && !(glx_version_major == 1 && glx_version_minor < 3) && glx_version_major >= 1);

	// Find the best framebuffer configuration from those available. Our only
	// quantitative criteria at the moment is sample count.
	i32 desired_framebuffer_attributes[] = {
		GLX_X_RENDERABLE, True,
		GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
		GLX_RENDER_TYPE, GLX_RGBA_BIT,
		GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
		GLX_RED_SIZE, 8,
		GLX_GREEN_SIZE, 8,
		GLX_BLUE_SIZE, 8,
		GLX_ALPHA_SIZE, 8,
		GLX_DEPTH_SIZE, 24,
		GLX_STENCIL_SIZE, 8,
		GLX_DOUBLEBUFFER, True,
		GLX_SAMPLE_BUFFERS, 1,
		GLX_SAMPLES, 4,
		None
	};
	i32 framebuffer_configs_len;
	GLXFBConfig* framebuffer_configs = glXChooseFBConfig(context->display, DefaultScreen(context->display), desired_framebuffer_attributes, &framebuffer_configs_len);
	assert(framebuffer_configs != NULL);
	i32 best_framebuffer_config = -1;
	i32 best_sample_count = -1;
	for(i32 fc = 0; fc < framebuffer_configs_len; fc++) {
		XVisualInfo* tmp_visual_info = glXGetVisualFromFBConfig(context->display, framebuffer_configs[fc]);
		if(tmp_visual_info != NULL) {
			i32 sample_buffers;
			glXGetFBConfigAttrib(context->display, framebuffer_configs[fc], GLX_SAMPLE_BUFFERS, &sample_buffers);
			i32 samples;
			glXGetFBConfigAttrib(context->display, framebuffer_configs[fc], GLX_SAMPLES, &samples);
			if(best_framebuffer_config == -1 || (sample_buffers && samples > best_sample_count)) {
				best_framebuffer_config = fc;
				best_sample_count = samples;
			}
		}
		XFree(tmp_visual_info);
	}
	GLXFBConfig glx_framebuffer_config = framebuffer_configs[best_framebuffer_config];
	XVisualInfo* glx_visual_info = glXGetVisualFromFBConfig(context->display, glx_framebuffer_config);
	XFree(framebuffer_configs);

	// Set up root context-> This is somewhat mixed up with GLX stuff still, though
	// it should survive the generic case with some things factored into variables.
	Window root_window = RootWindow(context->display, glx_visual_info->screen);
	XSetWindowAttributes set_window_attributes = {};
	set_window_attributes.colormap = XCreateColormap(context->display, root_window, glx_visual_info->visual, AllocNone);
	set_window_attributes.background_pixmap = None;
	set_window_attributes.border_pixel = 0;
	set_window_attributes.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask | KeyReleaseMask | PointerMotionMask | ButtonPressMask | ButtonReleaseMask;

	// Create our actual Xlib context->
	u32 window_width = 1;
	u32 window_height = 1;
	context->window = XCreateWindow(context->display, root_window, 0, 0, window_width, window_height, 0, glx_visual_info->depth, InputOutput, glx_visual_info->visual, CWBorderPixel | CWColormap | CWEventMask, &set_window_attributes);
	if(context->window == 0) { panic(); }
	XFree(glx_visual_info);
	XStoreName(context->display, context->window, GAME_NAME);
	XMapWindow(context->display, context->window);

	// Validate existence of required GL extensions
	glXCreateContextAttribsARBProc glXCreateContextAttribsARB;
	char* gl_extensions = (char*)glXQueryExtensionsString(context->display, DefaultScreen(context->display));
	glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");
	const char* extension = "GLX_ARB_create_context";
	char* start;
	char* where;
	char* terminator;
	// Extension names shouldn't have spaces
	where = strchr((char*)extension, ' ');
	assert(!where && *extension != '\0');
	bool found_extension = true;
	for (start = gl_extensions;;) {
		where = strstr(start, extension);
		if (!where)
			break;

		terminator = where + strlen(extension);
		if (where == start || *(where - 1) == ' ') {
			if (*terminator == ' ' || *terminator == '\0')
				found_extension = true;
			start = terminator;
		}
	}
	assert(found_extension == true);

	// Create GLX context and window
	i32 glx_attributes[] = {
		GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
		GLX_CONTEXT_MINOR_VERSION_ARB, 6,
		None
	};
	GLXContext glx = glXCreateContextAttribsARB(context->display, glx_framebuffer_config, 0, 1, glx_attributes);
	if(glXIsDirect(context->display, glx) == false) { panic(); }
	glXMakeCurrent(context->display, context->window, glx);

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
    render_init(render_stack.memory, asset_pack_data);
    dynamic_lib_init(&context->game, string_const(GAME_LIB_NAME));
    update_game_library(context);
    context->game_init(game_stack.memory, asset_pack_data);

    // Loop
    while(context->close_requested == false) {
        // Poll Xlib events
    	while(XPending(context->display)) {
        	XEvent event;
        	XNextEvent(context->display, &event);
        	switch(event.type) {
        		case Expose:
        			break;
        		case ConfigureNotify: {
                	XWindowAttributes window_attributes;
                	XGetWindowAttributes(context->display, context->window, &window_attributes);
                	context->platform.window_size.x = window_attributes.width;
                	context->platform.window_size.y = window_attributes.height;
        			context->platform.window_size_updated_this_frame = true;
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

        // Update game
        context->game_update(game_stack.memory, render_frame_stack.memory, &context->platform);

        // Update ALSA sound
    	snd_pcm_sframes_t available;
    	snd_pcm_sframes_t delay;
    	ALSA_VERIFY(snd_pcm_avail_delay(context->alsa_pcm, &available, &delay));
    	// TODO: Make sure we have enough frames available.
    	i32 sample_count = context->alsa_latency_samples - delay;
        if(sample_count > 0) {
    		f32* sample_buffer = (f32*)stack_alloc(&platform_frame_stack, sample_count);
            context->game_audio_callback(game_stack.memory, sample_buffer, sample_count);
            i32 frames_written = snd_pcm_writen(context->alsa_pcm, (void**)&sample_buffer, sample_count);
            assert(frames_written == sample_count);
        }

        // Update renderer
        render_update(render_stack.memory, render_frame_stack.memory, asset_pack_data, &context->platform);
    	glXSwapBuffers(context->display, context->window);

        // Prepare for next frame
        stack_clear(&render_frame_stack);
        stack_clear(&platform_frame_stack);
        update_game_library(context);
        context->platform.window_size_updated_this_frame = false;
    }
    return 0;
}
#endif

// WEB
#if PLATFORM == PLATFORM_WEB

#endif
