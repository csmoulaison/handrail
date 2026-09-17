#ifndef handrail_random_h_INCLUDED
#define handrail_random_h_INCLUDED

#include <time.h>

static u32 fast_random_seed;

void random_init();
void random_seed_init(i64 seed);
i32 random_i32(i32 max);
f32 random_f32();
f32 random_f32_signed();

static void fast_random_init();
static u32 fast_random_u32();
static f32 fast_random_f32();
static f32 fast_random_f32_signed();

#ifdef CSM_IMPLEMENTATION

inline void fast_random_init() {
    fast_random_seed = time(NULL);
}

inline u32 fast_random_u32() {
	uint32_t x = fast_random_seed;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	fast_random_seed = x;
	return x;

    //fast_random_seed = (214013 * fast_random_seed + 2531011);
    //return (fast_random_seed >> 16) & 0x7FFF;
}

inline f32 fast_random_f32() {
	return (f32)fast_random_u32() / (f32)RAND_MAX;
}

inline f32 fast_random_f32_signed() {
	return fast_random_f32() * 2.0f - 1.0f;
}

void random_init() {
	srand(time(NULL));
}

void random_seed_init(i64 seed) {
    srand(seed);
}

i32 random_i32(i32 max) {
    if(max == 0) return 0;
	return rand() / (RAND_MAX / max);
}

f32 random_f32() {
	return (f32)rand() / (f32)RAND_MAX;
}

f32 random_f32_signed() {
	return random_f32() * 2.0f - 1.0f;
}

#endif
#endif
