#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>


typedef struct ContextTag
{
    int capture_freq;
} Context;


static void check_sdl(int err)
{
    if (err < 0) {
        fprintf(stderr, "Error %d: %s", err, SDL_GetError());
        exit(-1);
    }
}


static void callback(
    void *user_data,
    uint8_t *bytes,
    int n_bytes
) {
    const int32_t *const samples = (int32_t*)bytes;
    Context *ctx = user_data;
    
}


static void describe(const SDL_AudioSpec *have)
{
    printf(
        "Negotiated format:\n"
        "freq     %d\n"
        "channels %d\n"
        "silence  %d\n"
        "samples  %d\n"
        "size     %d\n"
        "bitsize  %d\n"
        "signed   %d\n"
        "format   %s\n"
        "endian   %s\n",
        have->freq,
        have->channels,
        have->silence,
        have->samples,
        have->size,
        SDL_AUDIO_BITSIZE(have->format),
        !SDL_AUDIO_ISUNSIGNED(have->format),
        SDL_AUDIO_ISINT(have->format) ? "int" : "float",
        SDL_AUDIO_ISBIGENDIAN(have->format) ? "big" : "little"
    ); 
}


void capture_init(void)
{
    Context *ctx = malloc(sizeof(Context));
    if (!ctx)
    {
        perror("Failed to allocate context");
        exit(1);
    }

    check_sdl(SDL_Init(SDL_INIT_AUDIO));

    SDL_AudioSpec have;
    const SDL_AudioSpec want = {
        .freq = 65536,
        .format = AUDIO_S32SYS,
        .channels = 1,
        .samples = 1024,
        .callback = callback,
        .userdata = ctx
    };
    
    const bool is_capture = true;
    const char *default_dev = NULL;
    SDL_AudioDeviceID dev_id = SDL_OpenAudioDevice(
        default_dev,
        is_capture,
        &want,
        &have,
        SDL_AUDIO_ALLOW_FREQUENCY_CHANGE
    );
    if (dev_id == 0)
    {
        fputs(SDL_GetError(), stderr);
        exit(-1);
    }
    
    ctx->capture_freq = have.freq;
    
    printf(
        "Capture initialized on default device %d: %s\n",
        dev_id,
        SDL_GetCurrentAudioDriver()
    );
    describe(&have);
}


void capture_deinit(void)
{
    SDL_Quit();
    puts("Capture de-initialized");
}

