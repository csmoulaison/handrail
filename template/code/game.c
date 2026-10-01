// The game. The build compiles this into a shared library (bin/__NAME__.so or
// .dll) that handrail's platform layer (handrail/code/main.c) loads, and
// reloads whenever it's rebuilt: run the dynamic build target while the game
// is running to see changes live.
//
// The library exports the three functions declared in
// handrail/code/handrail/game.h:
// - game_init:           once at startup. Sets up game state and the renderer.
// - game_update:         once per frame. Reads input and draws the frame.
// - game_audio_callback: whenever the platform wants more audio.

#include "config.h"

#define HANDRAIL_IMPLEMENTATION
#include "handrail/core.h"

// Asset handles and regions, written by prebuild.c
#include "generated/asset_handles.c"
#include "draw.c"

// All game state. It lives in game_memory, which the platform owns, so it
// survives hot reloads. Globals and statics in this file don't.
typedef struct {
    // sizeof(Game) when the state was initialized. Must stay first, so it can
    // be read after a hot reload changes the layout of everything after it.
    u64  game_size;
    bool tinted; // Toggled with space
} Game;

GAME_INIT(game_init) {
    Game* game = (Game*)game_memory;
    char* pack = (char*)asset_memory;
    log_bind(platform->log);
    log_set_layer_name(LOG_GAME, string_const("game"));

    // Initialize game state
    game->game_size = sizeof(Game);
    game->tinted = false;

    // Tell the renderer what to upload and how to draw (handrail/code/handrail/render.h).
    // Buffer regions: the vector font data. Shaders reach them through push
    // constants, in this order (code/shaders/draw.glsl).
    render_setup->buffer_regions[0] = (RenderBufferRegion){ &pack[FONT_VECTOR_GLYPHS_REGION_OFFSET], FONT_VECTOR_GLYPHS_REGION_SIZE };
    render_setup->buffer_regions[1] = (RenderBufferRegion){ &pack[FONT_VECTOR_BANDS_REGION_OFFSET], FONT_VECTOR_BANDS_REGION_SIZE };
    render_setup->buffer_regions[2] = (RenderBufferRegion){ &pack[FONT_VECTOR_CURVES_REGION_OFFSET], FONT_VECTOR_CURVES_REGION_SIZE };
    render_setup->buffer_regions_len = 3;

    // Textures: every TEXTURE asset, so a TEXTURE handle indexes the shaders' textures array
    for(i32 i = 0; i < TEXTURE_COUNT; i++) {
        render_setup->textures[i] = texture_asset(pack, i);
    }
    render_setup->textures_len = TEXTURE_COUNT;
    render_setup->texture_pixels = (u8*)&pack[TEXTURE_PIXELS_REGION_OFFSET];

    // Samplers: one, so pixel art stays crisp
    render_setup->samplers[0] = (RenderSamplerDesc){ RENDER_FILTER_NEAREST, RENDER_ADDRESS_CLAMP };
    render_setup->samplers_len = 1;

    // Passes: one, alpha blended, with no depth, since everything is flat and
    // drawn in order
    render_setup->passes[0] = (RenderPassDesc){
        .vertex_shader   = shader_asset(pack, SHADER_DRAW_VERT),
        .fragment_shader = shader_asset(pack, SHADER_DRAW_FRAG),
        .blend = RENDER_BLEND_ALPHA, .cull = RENDER_CULL_NONE, .depth_test = false, .depth_write = false };
    render_setup->passes_len = 1;

    log_print(LOG_GAME, "Game initialized");
}

GAME_UPDATE(game_update) {
    Game* game = (Game*)game_memory;
    char* pack = (char*)asset_memory;
    log_bind(platform->log);

    // A hot reload that changes the Game struct leaves the old layout in memory
    if(game->game_size != sizeof(Game)) {
        log_print(LOG_WARN, "Game state changed size in a hot reload. State may be corrupt; restart the game");
        game->game_size = sizeof(Game);
    }

    // Input: this frame's events (handrail/code/handrail/platform.h). For
    // rebindable actions over keys and gamepads, see handrail/code/handrail/input.h.
    for(i32 i = 0; i < platform->events_len; i++) {
        PlatformEvent* event = &platform->events[i];
        if(event->type == PLATFORM_EVENT_KEYDOWN && event->key == PLATFORM_KEY_SPACE) {
            game->tinted = !game->tinted;
        }
    }

    // Draw
    iv2 screen = platform->window_size;
    f32 time = (f32)((f64)platform->time_ns / 1e9);
    Draw draw;
    draw_init(&draw, render_frame, platform->frame_stack, screen, v4_new(0.1f, 0.1f, 0.15f, 1.0f));

    // A checkerboard quad, bobbing up and down near the center of the screen
    v2 quad_size = v2_new(128.0f, 128.0f);
    v2 quad_position = v2_new(screen.x * 0.5f - quad_size.x * 0.5f, screen.y * 0.5f + 20.0f * sinf(time * 2.0f));
    v4 quad_color = game->tinted ? v4_new(1.0f, 0.6f, 0.3f, 1.0f) : v4_new(1.0f, 1.0f, 1.0f, 1.0f);
    draw_quad(&draw, quad_position, quad_size, TEXTURE_CHECKER, quad_color);

    // Text below it
    FontVectorData* font = font_vector_asset(pack, FONT_VECTOR_BODY);
    v4 white = v4_new(1.0f, 1.0f, 1.0f, 1.0f);
    String title = string_const("Hello, __NAME__");
    f32 title_size = 48.0f;
    v2 title_extent = font_vector_measure(font, title, title_size);
    draw_text(&draw, font, title, v2_new(screen.x * 0.5f - title_extent.x * 0.5f, screen.y * 0.5f - 80.0f), title_size, white);
    draw_text(&draw, font, string_const("Press space to tint the quad"), v2_new(24.0f, 24.0f), 24.0f, white);
}

GAME_AUDIO_CALLBACK(game_audio_callback) {
    // Silence. Write frames_len frames of AUDIO_CHANNEL_COUNT interleaved
    // samples here to make sound; see handrail/code/handrail/media/pcm.h and synth.h.
    memset(samples, 0, frames_len * AUDIO_CHANNEL_COUNT * sizeof(f32));
}
