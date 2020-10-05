#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <asoundlib.h>

#include "capture.h"


#define SUBDEV_NO 0
#define NAME_SIZE 16


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
	
    unsigned rate;
};


static void warn_snd(int err)
{
    if (err)
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
    if (err)
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
    check_snd(snd_pcm_open(
        &ctx->pcm,
        ctx->card_name,
        SND_PCM_STREAM_CAPTURE,
        0 // mode
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

    // Use maximum sampling rate; works out to 48,000
    int direction;
    check_snd(snd_pcm_hw_params_set_rate_last(
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

    check_snd(snd_pcm_hw_params(ctx->pcm, ctx->hwparams));
    check_snd(snd_pcm_prepare(ctx->pcm));
}


static void describe(const CaptureContext *ctx)
{
    check_snd(snd_ctl_card_info(ctx->card_ctl, ctx->card_info));

    puts("\n"
         "Card -------------");
    printf("  name       : %s\n", ctx->card_name);
    printf("  id         : %s\n",
        snd_ctl_card_info_get_id(ctx->card_info));
    printf("  components : %s\n",
        snd_ctl_card_info_get_components(ctx->card_info));
    printf("  driver     : %s\n",
        snd_ctl_card_info_get_driver(ctx->card_info));
    printf("  short name : %s\n",
        snd_ctl_card_info_get_name(ctx->card_info));
    printf("  long name  : %s\n",
        snd_ctl_card_info_get_longname(ctx->card_info));
    printf("  mixer      : %s\n\n",
        snd_ctl_card_info_get_mixername(ctx->card_info));

    puts("PCM --------------");
    printf("  card        : %d\n",
        snd_pcm_info_get_card(ctx->pcm_info));
    printf("  device      : %d\n",
        snd_pcm_info_get_device(ctx->pcm_info));
    printf("  subdev index/avail/total : %d/%d/%d\n",
        snd_pcm_info_get_subdevice(ctx->pcm_info),
        snd_pcm_info_get_subdevices_avail(ctx->pcm_info),
        snd_pcm_info_get_subdevices_count(ctx->pcm_info)
    );
    printf("  name        : %s\n",
        snd_pcm_info_get_name(ctx->pcm_info));
    printf("  id          : %s\n",
        snd_pcm_info_get_id(ctx->pcm_info));
    printf("  subdev name : %s\n",
        snd_pcm_info_get_subdevice_name(ctx->pcm_info));
    printf("  class       : %s\n",
        snd_pcm_class_name(
            snd_pcm_info_get_class(ctx->pcm_info)
        ));
    printf("  subclass    : %s\n\n",
        snd_pcm_subclass_name(
            snd_pcm_info_get_subclass(ctx->pcm_info)
        ));

    puts("Parameters -------");
    check_snd(snd_pcm_dump(ctx->pcm, ctx->output));
}


CaptureContext *capture_init(void)
{
    CaptureContext *ctx = malloc(sizeof(CaptureContext));
    assert(ctx);

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
}

