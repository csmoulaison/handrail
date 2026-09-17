#ifndef handrail_fiedler_h_INCLUDED
#define handrail_fiedler_h_INCLUDED

/* Example usage:
	  
	FiedlerTime fiedler = {};
	fiedler_init_time(&fiedler);
	while(game_running) {
		fiedler_accumulate_time(&fiedler);
		while(fiedler.accumulator >= frame_length) {
			game_update(game, frame_length);
			fiedler.accumulator -= frame_length;
		}

	}

*/

#include <time.h>

#ifndef FIEDLER_MINIMUM_FRAME_TIME
#define FIEDLER_MINIMUM_FRAME_TIME 0.25f
#endif

typedef struct {
    f64 time;
    f64 accumulator;
} FiedlerTime;

f64  time_seconds();
void fiedler_init_time(FiedlerTime* fiedler);
void fiedler_accumulate_time(FiedlerTime* fiedler);

#ifdef CSM_IMPLEMENTATION

f64 time_seconds() {
#if PLATFORM == PLATFORM_LINUX
	struct timespec current;
	assert(clock_gettime(CLOCK_REALTIME, &current) == 0);
	return (f64)current.tv_sec + (f32)current.tv_nsec / 1000000000.0f;
#elif PLATFORM == PLATFORM_WINDOWS
	// NOW: Implement windows
	return 0.0;
#elif PLATFORM == PLATFORM_WEB
	// TODO: Implement web
#endif
}

void fiedler_init_time(FiedlerTime* fiedler) {
	fiedler->time = time_seconds();
	fiedler->accumulator = 0.0f;
}

void fiedler_accumulate_time(FiedlerTime* fiedler) {
	f64 new_time = time_seconds();
	f64 frame_time = new_time - fiedler->time;
	if(frame_time > FIEDLER_MINIMUM_FRAME_TIME) {
		frame_time = FIEDLER_MINIMUM_FRAME_TIME;
	}
	fiedler->time = new_time;
	fiedler->accumulator += frame_time;
}

#endif
#endif
