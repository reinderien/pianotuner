#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <asoundlib.h>

#include "capture.h"


#define SUBDEV_NO 0
#define NAME_SIZE 16

// 2**(1/12)
#define SEMI 1.0594630943592953

#define A440_OCTAVES 4
#define FMIN (440. / (1 << A440_OCTAVES))
#define N_NOTES 88
#define FMAX (FMIN * (1 << (N_NOTES/12)) *SEMI*SEMI*SEMI)

/*
Example:
fmin = 27.5 Hz    tmax = 36.363 ms
fsamp = 44.1 kHz  tsamp = 22.676 us
samples_min = fsamp / fmin = 1604
*/


struct CaptureContextTag
{
    int card_no, dev_no;
    char card_name[NAME_SIZE];

    snd_output_t *output;
    snd_ctl_t *card_ctl;
    snd_ctl_card_info_t *card_info;
	snd_pcm_info_t *pcm_info;
	snd_pcm_t *pcm;
	snd_pcm_hw_params_t *hwparams;
	
    unsigned rate, period, timeout_ms, timeout_us;
    bool restart;

    snd_pcm_state_t prev_state;
};


static void warn_snd(int err)
{
    if (err < 0)
    {
        fprintf(
            stderr,
            "ALSA failure %d: %s\n",
            err,
            snd_strerror(err)
        );
    }
}


static void check_snd(int err)
{
    warn_snd(err);
    if (err < 0)
        exit(1);
}


static const char *snd_pcm_class_name(snd_pcm_class_t class)
{
    switch (class)
    {
        case SND_PCM_CLASS_GENERIC:
            return "GENERIC";
        case SND_PCM_CLASS_MULTI:
            return "MULTI";
        case SND_PCM_CLASS_MODEM:
            return "MODEM";
        case SND_PCM_CLASS_DIGITIZER:
            return "DIGITIZER";
        default:
            assert(false);
    }
}


static const char *snd_pcm_subclass_name(snd_pcm_subclass_t subclass)
{
    switch (subclass)
    {
        case SND_PCM_SUBCLASS_GENERIC_MIX:
            return "GENERIC MIX";
        case SND_PCM_SUBCLASS_MULTI_MIX:
            return "MULTI MIX";
        default:
            assert(false);
    }
}


static void enumerate(CaptureContext *ctx)
{
    puts("Enumerating devices...");

    ctx->pcm_info = malloc(snd_pcm_info_sizeof());
    ctx->card_info = malloc(snd_ctl_card_info_sizeof());
    assert(ctx->pcm_info);
    assert(ctx->card_info);

    // Iterate through all cards
    for (ctx->card_no = -1;;)
    {
        check_snd(snd_card_next(&ctx->card_no));
        if (ctx->card_no < 0)
            break;

        assert(snprintf(
            ctx->card_name,
            NAME_SIZE,
            "hw:%d",
            ctx->card_no
        ) > 0);
        check_snd(snd_ctl_open(
            &ctx->card_ctl,
            ctx->card_name,
            SND_CTL_READONLY
        ));

        // Iterate through PCM devices on this card
        for (ctx->dev_no = -1;;)
        {
            check_snd(snd_ctl_pcm_next_device(ctx->card_ctl, &ctx->dev_no));
            if (ctx->dev_no < 0)
                break;

            printf("hw:%d,%d", ctx->card_no, ctx->dev_no);

			// Do not iterate through subdevices; just use the first
			snd_pcm_info_set_device(ctx->pcm_info, ctx->dev_no);
			snd_pcm_info_set_subdevice(ctx->pcm_info, SUBDEV_NO);
			snd_pcm_info_set_stream(ctx->pcm_info, SND_PCM_STREAM_CAPTURE);
			
			int err = snd_ctl_pcm_info(ctx->card_ctl, ctx->pcm_info);
            switch (err)
            {
                case 0:
                    // Use the first device that is capture-capable
                    printf(",%d: use\n", SUBDEV_NO);
                    return;
                case -ENOENT:
                    // This PCM doesn't have capture
                    puts(",*: skip");
                    break;
                default:
                    // Different failure - treat it as fatal
                    check_snd(err);
            }
        }

        check_snd(snd_ctl_close(ctx->card_ctl));
    }

    fputs("There are no capture devices\n", stderr);
    exit(-1);
}


static void init_pcm(CaptureContext *ctx)
{
    assert(snprintf(
        ctx->card_name,
        NAME_SIZE,
        "hw:%d,%d,%d",
        ctx->card_no,
        ctx->dev_no,
        SUBDEV_NO
    ) > 0);

    const int mode = 0;
    check_snd(snd_pcm_open(
        &ctx->pcm,
        ctx->card_name,
        SND_PCM_STREAM_CAPTURE,
        mode
    ));

    ctx->hwparams = malloc(snd_pcm_hw_params_sizeof());
    assert(ctx->hwparams);

    check_snd(snd_pcm_hw_params_any(ctx->pcm, ctx->hwparams));

    check_snd(snd_pcm_hw_params_set_access(
        ctx->pcm,
        ctx->hwparams,
        // Interleaved has no effect since we have only one channel,
        // but it's the default - so fine
        SND_PCM_ACCESS_MMAP_INTERLEAVED
    ));

    check_snd(snd_pcm_hw_params_set_format(
        ctx->pcm,
        ctx->hwparams,
        // This is the only one supported by the hardware anyway
        SND_PCM_FORMAT_S16_LE
    ));

    check_snd(snd_pcm_hw_params_set_channels(
        ctx->pcm,
        ctx->hwparams,
        1
    ));

    check_snd(snd_pcm_hw_params_set_rate_resample(
        ctx->pcm, ctx->hwparams, false
    ));

    // Set minimum rate based on Nyquist frequency of max note
    int direction = 0;
    ctx->rate = 2*FMIN;
    check_snd(snd_pcm_hw_params_set_rate_min(
        ctx->pcm,
        ctx->hwparams,
        &ctx->rate,
        &direction
    ));
    check_snd(snd_pcm_hw_params_set_rate_first(
        ctx->pcm,
        ctx->hwparams,
        &ctx->rate,
        &direction
    ));
    if (direction != 0)
    {
        fprintf(
            stderr,
            "Inexact sampling frequency %u; direction %d\n",
            ctx->rate,
            direction
        );
        exit(-1);
    }

    const float min_period = ctx->rate / FMIN;
    for (ctx->period = 1; ctx->period < min_period; ctx->period <<= 1);
    direction = 0;
    check_snd(snd_pcm_hw_params_set_period_size(
        ctx->pcm,
        ctx->hwparams,
        ctx->period,
        direction
    ));

    check_snd(snd_pcm_hw_params_set_buffer_size(
        ctx->pcm,
        ctx->hwparams,
        2*ctx->period
    ));

    // Time out after 25% overrun
    ctx->timeout_us = (unsigned)(ctx->period * 1.25 / ctx->rate * 1e6),
    ctx->timeout_ms = ctx->timeout_us / 1000;

    check_snd(snd_pcm_hw_params(ctx->pcm, ctx->hwparams));
}


static void describe(const CaptureContext *ctx)
{
    check_snd(snd_ctl_card_info(ctx->card_ctl, ctx->card_info));

    printf(
        "\n"
        "Card -------------\n"
        "  name       : %s\n"
        "  id         : %s\n"
        "  components : %s\n"
        "  driver     : %s\n"
        "  short name : %s\n"
        "  long name  : %s\n"
        "  mixer      : %s\n\n",
        ctx->card_name,
        snd_ctl_card_info_get_id(ctx->card_info),
        snd_ctl_card_info_get_components(ctx->card_info),
        snd_ctl_card_info_get_driver(ctx->card_info),
        snd_ctl_card_info_get_name(ctx->card_info),
        snd_ctl_card_info_get_longname(ctx->card_info),
        snd_ctl_card_info_get_mixername(ctx->card_info)
    );

    printf(
        "PCM --------------\n"
        "  card        : %d\n"
        "  device      : %d\n"
        "  subdev index/avail/total : %d/%d/%d\n"
        "  name        : %s\n"
        "  id          : %s\n"
        "  subdev name : %s\n"
        "  class       : %s\n"
        "  subclass    : %s\n\n",
        snd_pcm_info_get_card(ctx->pcm_info),
        snd_pcm_info_get_device(ctx->pcm_info),
        snd_pcm_info_get_subdevice(ctx->pcm_info),
        snd_pcm_info_get_subdevices_avail(ctx->pcm_info),
        snd_pcm_info_get_subdevices_count(ctx->pcm_info),
        snd_pcm_info_get_name(ctx->pcm_info),
        snd_pcm_info_get_id(ctx->pcm_info),
        snd_pcm_info_get_subdevice_name(ctx->pcm_info),
        snd_pcm_class_name(
            snd_pcm_info_get_class(ctx->pcm_info)
        ),
        snd_pcm_subclass_name(
            snd_pcm_info_get_subclass(ctx->pcm_info)
        )
    );

    puts("Parameters -------");
    check_snd(snd_pcm_dump(ctx->pcm, ctx->output));

    printf(
        "\n"
        "Constraints ------\n"
         "  period : %u > %d\n"
         "  rate   : %u > %d\n"
         "  fmin   : %.1f < %.1f\n"
         "  fmax   : %d > %d\n\n",
         ctx->period, (int)(ctx->rate / FMIN),
         ctx->rate, (int)(2*FMAX),
         ctx->rate / (float)ctx->period, FMIN,
         ctx->rate/2, (int)FMAX
    );
}


CaptureContext *capture_init(void)
{
    CaptureContext *ctx = malloc(sizeof(CaptureContext));
    assert(ctx);

    ctx->restart = true;
    ctx->prev_state = -1;  // The first state will always be "new"

    const bool close = false;
    check_snd(snd_output_stdio_attach(
        &ctx->output,
        stdout,
        close
    ));

    enumerate(ctx);
    init_pcm(ctx);
    describe(ctx);

    return ctx;
}


void capture_deinit(CaptureContext *ctx)
{
    warn_snd(snd_pcm_close(ctx->pcm));
    warn_snd(snd_ctl_close(ctx->card_ctl));
    snd_config_update_free_global();

    free(ctx->card_info);
    free(ctx->pcm_info);
    free(ctx->hwparams);
    free(ctx);

    puts("Capture deinitialized");
}


static bool resume(CaptureContext *ctx)
{
    fputs("Attempting to resume...\n", stderr);

    int err;
    while (true)
    {
        err = snd_pcm_resume(ctx->pcm);
        if (err != -EAGAIN)
            break;
        sleep(1);
    }

    if (err == 0)
        return true;
    warn_snd(err);
    return false;
}


static bool recover(CaptureContext *ctx, int err)
{
    const char *name = snd_strerror(err);
    fprintf(stderr, "Attempting recovery from error %s...\n", name);

    switch (err)
    {
        case -EPIPE:  // Overrun
            err = snd_pcm_prepare(ctx->pcm);
            warn_snd(err);
            return err == 0;

        case -ESTRPIPE:  // Suspended
            return resume(ctx);

        default:
            fprintf(
                stderr,
                "Don't know how to recover from %s\n",
                name
            );
            return false;
    }
}


static bool recover_err(CaptureContext *ctx, int err)
{
    fprintf(
        stderr,
        "Trying recovery from error %d - %s\n",
        err, snd_strerror(err)
    );
    return recover(ctx, err);
}


static bool recover_state(CaptureContext *ctx, snd_pcm_state_t state)
{
    const char *name = snd_pcm_state_name(state);

    if (ctx->prev_state != state)
    {
        ctx->prev_state = state;
        printf("Entered state %s\n", name);
    }

    switch (state)
    {
        case SND_PCM_STATE_OPEN:
        case SND_PCM_STATE_SETUP:
        case SND_PCM_STATE_PREPARED:
        case SND_PCM_STATE_RUNNING:
            return true;
        default:
            break;
    }

    bool result;

    fprintf(stderr, "Attempting recovery from state %s\n", name);

    switch (state)
    {
        case SND_PCM_STATE_XRUN:
            ctx->restart = true;
            result = recover(ctx, -EPIPE);
            break;

        case SND_PCM_STATE_SUSPENDED:
            result = recover(ctx, -ESTRPIPE);
            break;

        default:
            fprintf(
                stderr,
                "Don't know how to recover from state %s\n",
                name
            );
            return true;  // Non-fatal
    }

    fprintf(
        stderr,
        "Recovery from state %s %s\n",
        name,
        result ? "succeeded" : "failed"
    );
    return result;
}


static snd_pcm_sframes_t capture_wait(CaptureContext *ctx)
{
    snd_pcm_state_t state = snd_pcm_state(ctx->pcm);
    if (!recover_state(ctx, state))
    {
        usleep(ctx->timeout_us);
        return 0;
    }

    snd_pcm_sframes_t avail = snd_pcm_avail_update(ctx->pcm);
    if (avail < 0)
    {
        ctx->restart = true;
        if (!recover_err(ctx, avail))
            usleep(ctx->timeout_us);
        return 0;
    }

    if (avail >= ctx->period)
        return avail;

    if (ctx->restart)
    {
        fputs("Restarting...\n", stderr);
        int err = snd_pcm_start(ctx->pcm);
        if (err == 0)
            ctx->restart = false;
        else
        {
            warn_snd(err);
            usleep(ctx->timeout_us);
        }
    }
    else
    {
        int err = snd_pcm_wait(ctx->pcm, ctx->timeout_ms);
        if (err < 0)
        {
            ctx->restart = true;
            if (!recover_err(ctx, err))
                usleep(ctx->timeout_us);
        }
    }
    return 0;
}


void capture_period(
    CaptureContext *ctx,
    void (*consume)(
        const int16_t *samples,
        int n_samples
    )
) {
    snd_pcm_sframes_t avail;
    do
        avail = capture_wait(ctx);
    while (avail == 0);

    const snd_pcm_channel_area_t *areas;
    snd_pcm_uframes_t offset,
        frames = ctx->period;
    int err = (snd_pcm_mmap_begin(
        ctx->pcm,
        &areas,
        &offset,
        &frames
    ));
    if (err < 0)
    {
        warn_snd(err);
        return;
    }

    assert(areas->first % 8 == 0);  // Assume start offset bit-alignment
    assert(areas->step == 16);      // Assume fully-contiguous samples

    snd_pcm_uframes_t used;
    if (frames >= ctx->period)
    {
        used = ctx->period;

        const int16_t *samples = (int16_t*)(
            (uint8_t*)areas->addr
            + (areas->first / 8)
        ) + offset;

        consume(samples, ctx->period);
    }
    else
    {
        fprintf(
            stderr,
            "Short read %ld < %u\n",
            frames,
            ctx->period
        );
        used = 0;
    }

    snd_pcm_sframes_t transferred = snd_pcm_mmap_commit(
        ctx->pcm,
        offset,
        used
    );
    warn_snd(transferred);
}

