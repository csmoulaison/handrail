#ifndef handrail_game_h_INCLUDED
#define handrail_game_h_INCLUDED

#define GAME_INIT(name) void name(void* game_memory, void* asset_memory, RenderSetup* render_setup, Platform* platform)
typedef GAME_INIT(GameInitFunction);
GAME_INIT(game_init_stub) {}

#define GAME_UPDATE(name) void name(void* game_memory, RenderFrame* render_frame, Platform* platform)
typedef GAME_UPDATE(GameUpdateFunction);
GAME_UPDATE(game_update_stub) {}

#define GAME_AUDIO_CALLBACK(name) void name(void* game_memory, f32* samples, i32 samples_len)
typedef GAME_AUDIO_CALLBACK(GameAudioCallback);
GAME_AUDIO_CALLBACK(game_audio_callback_stub) {}

#endif
