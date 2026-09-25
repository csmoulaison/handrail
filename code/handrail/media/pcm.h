#ifndef handrail_wav_h_INCLUDED
#define handrail_wav_h_INCLUDED

// Undef any of these to allow for multiple types in the same project
#ifndef PCM_CHANNEL_COUNT
#define PCM_CHANNEL_COUNT 1
#endif
#ifndef PCM_BITS_PER_SAMPLE
#define PCM_BITS_PER_SAMPLE 16
#endif
#ifndef PCM_SAMPLE_RATE
#define PCM_SAMPLE_RATE 48000
#endif

#pragma pack(push, 1)
typedef struct {
    // "RIFF"
    char riff_header[4];    
    // (Size of file - 8 bytes)
    u32  size;
    // "WAVE"
    char wave_header[4];
    // "fmt "
    char fmt_header[4];
    // 16 for PCM
    u32  format_chunk_size;
    // 1 for PCM
    u16  audio_format;
    u16  channel_count;  
    u32  sample_rate;
    // (sample_rate * num_channels * bytes_per_sample)
    u32  byte_rate;
    u16  sample_alignment;
    u16  bits_per_sample;
    // "data"
    char data_header[4];
    // Size of data section
    u32  data_size;
} WavHeader;
#pragma pack(pop)

typedef struct {
#ifndef PCM_CHANNEL_COUNT
    u8 channel_count;
#endif
#ifndef PCM_BITS_PER_SAMPLE
    u8 bits_per_sample
#endif
#ifndef PCM_SAMPLE_RATE
    u32 sample_rate;
#endif
    u64 buffer_size;
    u64 sample_count;
    u8  sample_buffer[];    
} PcmClip;

PcmClip* pcm_clip_from_wav(String path, Stack* stack);
u64      pcm_clip_size_from_buffer_size(u64 buffer_size);
u64      pcm_clip_size(PcmClip* pcm);

#ifdef CSM_IMPLEMENTATION

PcmClip* pcm_clip_from_wav(String path, Stack* stack) {
    assert(sizeof(WavHeader) == 44);

    WavHeader header = {};
    File file = file_open(path, FILE_OPEN_READ);
    printf("before BEFORE read ftell %u\n", ftell(file.handle));
    file_read(&file, &header, sizeof(WavHeader));
    printf("before read ftell %u\n", ftell(file.handle));
    assert(strncmp(header.riff_header, "RIFF", 4) == 0);
    assert(strncmp(header.wave_header, "WAVE", 4) == 0);
    assert(strncmp(header.fmt_header,  "fmt ", 3) == 0);
    assert(strncmp(header.data_header, "data", 4) == 0);

    printf("pcm chunk size %u\n", header.format_chunk_size);
    printf("pcm format %u\n", header.audio_format);
    printf("pcm channels %u\n", header.channel_count);
    printf("pcm sample rate %u\n", header.sample_rate);
    printf("pcm bits per sample %u\n", header.bits_per_sample);
    printf("pcm data size %u\n", header.data_size);

    PcmClip* clip = (PcmClip*)stack_alloc(stack, pcm_clip_size_from_buffer_size(header.data_size));
    clip->buffer_size = header.data_size;
    clip->sample_count = header.data_size / (header.bits_per_sample / 8);

    #ifdef PCM_CHANNEL_COUNT
    assert(header.channel_count == PCM_CHANNEL_COUNT);
    #else
    clip->channel_count = header.channel_count;
    #endif

    #ifdef PCM_BITS_PER_SAMPLE
    assert(header.bits_per_sample == PCM_BITS_PER_SAMPLE);
    #else
    clip->bits_per_sample = header.bits_per_sample;
    #endif

    #ifdef PCM_SAMPLE_RATE
    assert(header.sample_rate == PCM_SAMPLE_RATE);
    #else
    clip->sample_rate = header.sample_rate;
    #endif

    assert(file_read(&file, clip->sample_buffer, header.data_size) != 0);
    file_close(&file);
    return clip;
}

u64 pcm_clip_size_from_buffer_size(u64 buffer_size) {
    return buffer_size + sizeof(PcmClip);
}

u64 pcm_clip_size(PcmClip* pcm) {
    return pcm_clip_size_from_buffer_size(pcm->buffer_size);
}

#endif
#endif
