#ifndef handrail_game_h_INCLUDED
#define handrail_game_h_INCLUDED

#define GAME_INIT(name) void name(void* game_memory, void* asset_memory)
typedef GAME_INIT(GameInitFunction);
GAME_INIT(game_init_stub) {}

#define GAME_UPDATE(name) void name(void* game_memory, void* render_frame_memory, Platform* platform)
typedef GAME_UPDATE(GameUpdateFunction);
GAME_UPDATE(game_update_stub) {}

#define GAME_AUDIO_CALLBACK(name) void name(void* game_memory, f32* samples, i32 samples_len)
typedef GAME_AUDIO_CALLBACK(GameAudioCallback);
GAME_AUDIO_CALLBACK(game_audio_callback_stub) {}

#endif
