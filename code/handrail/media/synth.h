#ifndef handrail_synth_h_INCLUDED
#define handrail_synth_h_INCLUDED

#ifndef SYNTH_WAVE_COUNT
#define SYNTH_WAVE_COUNT 3
#endif

#ifndef SYNTH_NOISE_COUNT
#define SYNTH_NOISE_COUNT 1
#endif

#ifndef SYNTH_NOISE_PHASE_COOLDOWN
#define SYNTH_NOISE_PHASE_COOLDOWN 8
#endif

i32 triangle_lut[32] = { 
    0,
    0 + 16,
    32,
    32 + 16,
    64,
    64 + 16,
    96,
    96 + 16,
    128,
    128 + 16,
    160,
    160 + 16,
    192,
    192 + 16,
    224,
    224 + 16,
    255,
    255 - 16,
    224,
    224 - 16,
    192,
    192 - 16,
    160,
    160 - 16,
    128,
    128 - 16,
    96,
    96 - 16,
    64,
    64 - 16,
    32,
    16
};

typedef struct {
    f64 amp;
    f64 freq;
    f64 phase;
    f64 amp_actual;
    f64 freq_actual;
} SynthWaveChannel;

typedef struct {
    f64 amp;
    f64 phase_amp;
    i8  phase_timer;
} SynthNoiseChannel;

typedef struct {
    f64               attenuation;
    f64               shelf;
    SynthWaveChannel  wave_channels[SYNTH_WAVE_COUNT];
    SynthNoiseChannel noise_channels[SYNTH_NOISE_COUNT];
} Synth;

void synth_init(Synth* synth);
void synth_callback(Synth* synth, f32* samples, i32 samples_len, i32 sample_rate);

#ifdef CSM_IMPLEMENTATION

void synth_init(Synth* synth) {
    memset(synth, 0, sizeof(Synth));
    synth->attenuation = 0.5f;
    synth->shelf = 0.9f;
}

void synth_callback(Synth* synth, f32* samples, i32 samples_len, i32 sample_rate) {
    SynthWaveChannel* wave;
    SynthNoiseChannel* noise;

    for(i32 i = 0; i < SYNTH_WAVE_COUNT; i++) {
        wave = &synth->wave_channels[i];
        if(wave->amp < 0.0f) {
            wave->amp = 0.0f;
        }
        if(wave->freq < 0.0f) {
            wave->freq = 0.0f;
        }
    }
    for(i32 i = 0; i < SYNTH_NOISE_COUNT; i++) {
        noise = &synth->noise_channels[i];
        if(noise->amp < 0.0f) {
            noise->amp = 0.0f;
        }
        //noise->amp = ((i32)(noise->amp * 256.0)) / 256.0;
    }
    
    for(i32 i = 0; i < samples_len; i++) {
		samples[i] = 0.0f;
        for(i32 j = 0; j < SYNTH_WAVE_COUNT; j++) {
            wave = &synth->wave_channels[j];
            if(wave->freq > 10.0f) {
                wave->freq_actual = f32_lerp(wave->freq_actual, wave->freq, 0.002f);
            }
            wave->amp_actual = f32_lerp(wave->amp_actual,  wave->amp,  0.002f);
            wave->phase += (2.0f * M_PI * wave->freq_actual) / sample_rate;

            // sin
            //samples[i] += wave->amp_actual * sinf(wave->phase);

            // square
            samples[i] += (sin(wave->phase) >= 0.0) ? wave->amp_actual : -wave->amp_actual;

            // triangle
            //i32 lut_i = (i32)fmod(wave->phase * M_PI, 32.0);                       
            //i32 lut_sample = triangle_lut[(i32)fmod(wave->phase * 2.5, 32.0)] - 128;
            //f32 triangle_sample = (f32)lut_sample / 128.0;
            //samples[i] += wave->amp_actual * triangle_sample;
        }
        for(i32 j = 0; j < SYNTH_NOISE_COUNT; j++) {
            noise = &synth->noise_channels[j];
            if(noise->phase_timer < 0) {
    			noise->phase_timer = SYNTH_NOISE_PHASE_COOLDOWN;
    			noise->phase_amp = random_f32_signed() * noise->amp;
            }
            noise->phase_timer--;
			samples[i] += noise->phase_amp;
        }
		samples[i] *= synth->attenuation;
		// RELEASE: remove this assert
		assert(samples[i] > -synth->shelf && samples[i] < synth->shelf);
		samples[i] = f32_clamp(samples[i], -synth->shelf, synth->shelf);
    }
}

#endif
#endif
