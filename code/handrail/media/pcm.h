#ifndef handrail_pcm_h_INCLUDED
#define handrail_pcm_h_INCLUDED

// When these are defined, every clip must match them and PcmClip doesn't store
// them. Undefine any of them to allow clips of different formats in one project.
#ifndef PCM_CHANNEL_COUNT
#define PCM_CHANNEL_COUNT 2
#endif
#ifndef PCM_BITS_PER_SAMPLE
#define PCM_BITS_PER_SAMPLE 16
#endif
#ifndef PCM_SAMPLE_RATE
#define PCM_SAMPLE_RATE 48000
#endif

#pragma pack(push, 1)
typedef struct {
    char riff_id[4];  // "RIFF"
    u32  size;        // Size of file - 8 bytes
    char wave_id[4];  // "WAVE"
} WavRiffHeader;

typedef struct {
    char id[4];
    u32  size;
} WavChunkHeader;

typedef struct {
    u16 audio_format;  // 1 for PCM
    u16 channel_count;
    u32 sample_rate;
    u32 byte_rate;     // sample_rate * channel_count * bytes_per_sample
    u16 sample_alignment;
    u16 bits_per_sample;
} WavFormat;
#pragma pack(pop)

// Interleaved samples, frames_len frames of channel_count samples each.
typedef struct {
#ifndef PCM_CHANNEL_COUNT
    u8  channel_count;
#endif
#ifndef PCM_BITS_PER_SAMPLE
    u8  bits_per_sample;
#endif
#ifndef PCM_SAMPLE_RATE
    u32 sample_rate;
#endif
    u64 buffer_size;
    u64 frames_len;
    u8  sample_buffer[];
} PcmClip;

// Load an uncompressed PCM .wav file.
PcmClip* pcm_clip_from_wav(String path, Stack* stack);
u64      pcm_clip_size_from_buffer_size(u64 buffer_size);
u64      pcm_clip_size(PcmClip* pcm);
// Load a .wav file and push it as a PCM asset. Returns its handle.
u64      pcm_push_asset(AssetBuilder* builder, String tag, String path, Stack* stack);

#endif

#if defined(HANDRAIL_IMPLEMENTATION_PASS) && !defined(handrail_pcm_h_IMPLEMENTED)
#define handrail_pcm_h_IMPLEMENTED

PcmClip* pcm_clip_from_wav(String path, Stack* stack) {
    File file = file_open(path, FILE_OPEN_READ);
    WavRiffHeader riff = {};
    assert(file_read(&file, &riff, sizeof(WavRiffHeader)) == 1);
    assert(strncmp(riff.riff_id, "RIFF", 4) == 0);
    assert(strncmp(riff.wave_id, "WAVE", 4) == 0);

    // Walk the chunks, skipping any we don't use (LIST, fact, etc.) until the data chunk
    WavFormat format = {};
    bool found_format = false;
    WavChunkHeader chunk = {};
    while(true) {
        assert(file_read(&file, &chunk, sizeof(WavChunkHeader)) == 1);
        if(strncmp(chunk.id, "data", 4) == 0) {
            break;
        }
        if(strncmp(chunk.id, "fmt ", 4) == 0) {
            assert(chunk.size >= sizeof(WavFormat));
            assert(file_read(&file, &format, sizeof(WavFormat)) == 1);
            found_format = true;
            file_seek(&file, chunk.size - sizeof(WavFormat));
        } else {
            file_seek(&file, chunk.size);
        }
        // Chunks are padded to an even size
        if(chunk.size % 2 == 1) {
            file_seek(&file, 1);
        }
    }
    assert(found_format);
    assert(format.audio_format == 1);

    log_print(LOG_ASSET, "Loaded " STRING_FMT ": %u channels, %u Hz, %u bits, %u bytes",
        STRING_ARG(path), format.channel_count, format.sample_rate, format.bits_per_sample, chunk.size);

    PcmClip* clip = (PcmClip*)stack_alloc(stack, pcm_clip_size_from_buffer_size(chunk.size));
    clip->buffer_size = chunk.size;
    clip->frames_len = chunk.size / (format.channel_count * (format.bits_per_sample / 8));

#ifdef PCM_CHANNEL_COUNT
    assert(format.channel_count == PCM_CHANNEL_COUNT);
#else
    clip->channel_count = format.channel_count;
#endif

#ifdef PCM_BITS_PER_SAMPLE
    assert(format.bits_per_sample == PCM_BITS_PER_SAMPLE);
#else
    clip->bits_per_sample = format.bits_per_sample;
#endif

#ifdef PCM_SAMPLE_RATE
    assert(format.sample_rate == PCM_SAMPLE_RATE);
#else
    clip->sample_rate = format.sample_rate;
#endif

    assert(file_read(&file, clip->sample_buffer, chunk.size) == 1);
    file_close(&file);
    return clip;
}

u64 pcm_clip_size_from_buffer_size(u64 buffer_size) {
    return buffer_size + sizeof(PcmClip);
}

u64 pcm_clip_size(PcmClip* pcm) {
    return pcm_clip_size_from_buffer_size(pcm->buffer_size);
}

u64 pcm_push_asset(AssetBuilder* builder, String tag, String path, Stack* stack) {
    PcmClip* clip = pcm_clip_from_wav(path, stack);
    u64 handle = asset_builder_next_handle_of_type(builder, string_const("PCM"));
    asset_builder_push_asset(builder, tag, string_const("PCM"), string_const("PcmClip"), clip, pcm_clip_size(clip), NULL);
    return handle;
}

#endif
