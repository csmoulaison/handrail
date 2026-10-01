#ifndef handrail_game_h_INCLUDED
#define handrail_game_h_INCLUDED

// Audio format the platform plays and game_audio_callback writes: 32-bit float
// samples, interleaved across channels.
#ifndef AUDIO_SAMPLE_RATE
#define AUDIO_SAMPLE_RATE 48000
#endif
#ifndef AUDIO_CHANNEL_COUNT
#define AUDIO_CHANNEL_COUNT 2
#endif

#define GAME_INIT(name) void name(void* game_memory, void* asset_memory, RenderSetup* render_setup, Platform* platform)
typedef GAME_INIT(GameInitFunction);
GAME_INIT(game_init_stub) {}

#define GAME_UPDATE(name) void name(void* game_memory, void* asset_memory, RenderFrame* render_frame, Platform* platform)
typedef GAME_UPDATE(GameUpdateFunction);
GAME_UPDATE(game_update_stub) {}

// Fill frames_len frames, frames_len * AUDIO_CHANNEL_COUNT interleaved samples (L, R, L, R, ...).
#define GAME_AUDIO_CALLBACK(name) void name(void* game_memory, void* asset_memory, f32* samples, i32 frames_len)
typedef GAME_AUDIO_CALLBACK(GameAudioCallback);
GAME_AUDIO_CALLBACK(game_audio_callback_stub) {}

#endif
