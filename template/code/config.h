// Game configuration. Every translation unit includes this before
// handrail/core.h (game.c, prebuild.c, and handrail's main.c), so they all
// agree on it. Engine headers wrap their limits in #ifndef, so any of them can
// be overridden here too.

// Game log layers. Name them in game_init with log_set_layer_name, and print
// to them with log_print(LOG_GAME, ...). See handrail/code/handrail/log.h.
#define LOG_GAME LOG_GAME_LAYER(0)

// Which layers print, and where to: stdout, and LOG_FILE_PATH
#define LOG_MASK      (LOG_ENGINE_DEFAULT | LOG_GAME)
#define LOG_TARGETS   (LOG_TARGET_STDOUT | LOG_TARGET_FILE)
#define LOG_FILE_PATH "log.txt"

// Which profile timers run. Game timers are PROFILE_GAME_TIMER(n), like the
// log layers. See handrail/code/handrail/profile.h.
#define PROFILE_MASK (PROFILE_ENGINE_ALL | PROFILE_GAME_ALL)
