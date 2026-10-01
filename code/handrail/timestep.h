#ifndef handrail_timestep_h_INCLUDED
#define handrail_timestep_h_INCLUDED

// A fixed-timestep clock. Each frame, timestep_advance adds the platform time
// since last frame and says how many whole steps to simulate, and
// timestep_alpha says how far to blend from the step before the latest to the
// latest. The step body, and what's blended, are the game's:
//
//     i32 steps = timestep_advance(&game->timestep, platform->time_ns, STEP_NS, MAX_STEPS);
//     for(i32 step = 0; step < steps; step++) {
//         game->previous = game->current;
//         // ... advance game->current by STEP_NS
//     }
//     f64 alpha = timestep_alpha(&game->timestep, STEP_NS);

// Time carried between frames. Lives in game state. Zeroed is a clock that
// hasn't seen a frame.
typedef struct {
    u64 time_ns;        // Platform time last frame, 0 before the first
    u64 accumulator_ns; // Time not yet simulated, under one step after each frame
} Timestep;

// Add the time since last frame and return how many steps of step_ns to run
// now, consuming them. Past max_steps, the backlog is dropped (and logged)
// rather than fast-forwarded, as after a breakpoint or a hitch.
i32 timestep_advance(Timestep* timestep, u64 time_ns, u64 step_ns, i32 max_steps);
// How far time has run past the latest step, in [0, 1): the weight to blend
// the latest step over the one before it.
f64 timestep_alpha(Timestep* timestep, u64 step_ns);

#endif

#if defined(HANDRAIL_IMPLEMENTATION_PASS) && !defined(handrail_timestep_h_IMPLEMENTED)
#define handrail_timestep_h_IMPLEMENTED

i32 timestep_advance(Timestep* timestep, u64 time_ns, u64 step_ns, i32 max_steps) {
    assert(step_ns > 0 && max_steps > 0);

    // The first frame has nothing to measure from
    u64 delta_ns = timestep->time_ns != 0 ? time_ns - timestep->time_ns : 0;
    timestep->time_ns = time_ns;
    timestep->accumulator_ns += delta_ns;

    u64 steps = timestep->accumulator_ns / step_ns;
    if(steps > (u64)max_steps) {
        log_print(LOG_INFO, "Fixed timestep %" PRIu64 " ms behind, dropping the backlog",
                  (timestep->accumulator_ns - max_steps * step_ns) / 1000000);
        steps = max_steps;
    }
    timestep->accumulator_ns %= step_ns;
    return (i32)steps;
}

f64 timestep_alpha(Timestep* timestep, u64 step_ns) {
    return (f64)timestep->accumulator_ns / (f64)step_ns;
}

#endif
