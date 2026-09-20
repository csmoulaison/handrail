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

#define ROOT_MEMORY_SIZE (sizeof(Context) + GAME_STACK_SIZE + RENDER_STACK_SIZE + RENDER_FRAME_STACK_SIZE + PLATFORM_FRAME_STACK_SIZE)

// Audio settings
#ifndef AUDIO_SAMPLE_RATE
#define AUDIO_SAMPLE_RATE 44100
#endif

// WINDOWS
#if PLATFORM == PLATFORM_WINDOWS

typedef struct {
    Stack root_stack;
    Stack game_stack;
    Stack render_stack;
    Stack render_frame_stack;
    Stack platform_frame_stack;
} Context;

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
        default: break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow) {
    // Console logging
    assert(AllocConsole());
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
    freopen_s(&f, "CONIN$", "r", stdin);
    
    // Allocate memory
    Context context = {};
    void* mem = VirtualAlloc(NULL, ROOT_MEMORY_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    assert(mem);
    context.root_stack           = stack_from_memory(mem, ROOT_MEMORY_SIZE, string_const("Root"));
    context.game_stack           = stack_from_stack(&context.root_stack, GAME_STACK_SIZE, string_const("Game"));
    context.render_stack         = stack_from_stack(&context.root_stack, RENDER_STACK_SIZE, string_const("Renderer"));
    context.render_frame_stack   = stack_from_stack(&context.root_stack, RENDER_FRAME_STACK_SIZE, string_const("RenderFrame"));
    context.platform_frame_stack = stack_from_stack(&context.root_stack, PLATFORM_FRAME_STACK_SIZE, string_const("PlatformFrame"));

    // Create window
    WNDCLASS window_class = {};
    window_class.lpfnWndProc   = window_proc;
    window_class.hInstance     = hInstance;
    window_class.lpszClassName = GAME_NAME;
    RegisterClass(&window_class);

    HWND hwnd = CreateWindowEx(
        0, GAME_NAME, GAME_NAME,
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, hInstance, &context);
    assert(hwnd);
    ShowWindow(hwnd, nCmdShow);

    // NOW: Vulkan, Audio, Input, Game DLL, Update
    vk_init(GAME_NAME, &context.platform_frame_stack);
     
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
    Stack game_stack           = stack_from_stack(&root_stack, GAME_STACK_SIZE, string_const("Game"));
    Stack render_stack         = stack_from_stack(&root_stack, RENDER_STACK_SIZE, string_const("Renderer"));
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
