#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <asoundlib.h>

#include "capture.h"


#define name_size 16


struct CaptureContextTag
{
    int card_no, dev_no;
    char card_name[name_size];
    
    snd_ctl_t *card_ctl;
    snd_ctl_card_info_t *card_info;
	snd_pcm_info_t *pcm_info;
	snd_pcm_t *pcm;
	
    int capture_freq;
};


static void check_snd(int err)
{
    if (err)
    {
        fprintf(
            stderr,
            "ALSA failure %d: %s\n",
            err,
            snd_strerror(err)
        );
        exit(1);
    }
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
    // Iterate through all cards
    for (ctx->card_no = -1;;)
    {
        check_snd(snd_card_next(&ctx->card_no));
        if (ctx->card_no < 0)
            break;
        
        assert(snprintf(
            ctx->card_name, 
            name_size, 
            "hw:%d", 
            ctx->card_no
        ) > 0);
        check_snd(snd_ctl_open(
            &ctx->card_ctl, 
            ctx->card_name, 
            SND_CTL_READONLY
        ));
        
        // Iterate through PCMs on this card
        for (ctx->dev_no = -1;;) 
        {
            check_snd(snd_ctl_pcm_next_device(ctx->card_ctl, &ctx->dev_no));
            if (ctx->dev_no < 0)
                break;
            
			snd_pcm_info_set_device(ctx->pcm_info, ctx->dev_no);
			snd_pcm_info_set_subdevice(ctx->pcm_info, 0);
			snd_pcm_info_set_stream(ctx->pcm_info, SND_PCM_STREAM_CAPTURE);
			
			int err = snd_ctl_pcm_info(ctx->card_ctl, ctx->pcm_info);
            switch (err)
            {
                case 0:
                    // Use the first device that is capture-capable
                    return;
                case -ENOENT:
                    // This PCM doesn't have capture
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


static void describe(const CaptureContext *ctx)
{
    check_snd(snd_ctl_card_info(ctx->card_ctl, ctx->card_info));
    
    puts("Card -------------");
    printf("Name:       %s\n", ctx->card_name);
    printf("ID:         %s\n", 
        snd_ctl_card_info_get_id(ctx->card_info));
    printf("Components: %s\n",
        snd_ctl_card_info_get_components(ctx->card_info));
    printf("Driver:     %s\n", 
        snd_ctl_card_info_get_driver(ctx->card_info));
    printf("Short name: %s\n", 
        snd_ctl_card_info_get_name(ctx->card_info));
    printf("Long name:  %s\n", 
        snd_ctl_card_info_get_longname(ctx->card_info));
    printf("Mixer:      %s\n\n", 
        snd_ctl_card_info_get_mixername(ctx->card_info));
    
    puts("PCM --------------");
    printf("Card:       %d\n",
        snd_pcm_info_get_card(ctx->pcm_info));
    printf("Device:     %d\n",
        snd_pcm_info_get_device(ctx->pcm_info));
    printf("Subdevice:  %d\n",
        snd_pcm_info_get_subdevice(ctx->pcm_info));
    printf("Subdevices: %d\n",
        snd_pcm_info_get_subdevices_count(ctx->pcm_info));
    printf("Stream:     %s\n",
        snd_pcm_stream_name(
            snd_pcm_info_get_stream(ctx->pcm_info)
        ));
    printf("Name:       %s\n",
        snd_pcm_info_get_name(ctx->pcm_info));
    printf("ID:         %s\n",
        snd_pcm_info_get_id(ctx->pcm_info));
    printf("Subdev name:%s\n",
        snd_pcm_info_get_subdevice_name(ctx->pcm_info));
    printf("Class:      %s\n",
        snd_pcm_class_name(
            snd_pcm_info_get_class(ctx->pcm_info)
        ));
    printf("Subclass:   %s\n\n",
        snd_pcm_subclass_name(
            snd_pcm_info_get_subclass(ctx->pcm_info)
        ));
}


CaptureContext *capture_init(void)
{
    CaptureContext *ctx = malloc(sizeof(CaptureContext));
    assert(ctx);
   
    puts("Enumerating devices...\n");
    
    snd_pcm_info_alloca(&ctx->pcm_info);
    assert(ctx->pcm_info);
    snd_ctl_card_info_alloca(&ctx->card_info);
    assert(ctx->card_info);
    
    enumerate(ctx);
    describe(ctx);

    check_snd(snd_pcm_open(
        &ctx->pcm, 
        ctx->card_name, 
        SND_PCM_STREAM_CAPTURE,
        0 // mode
    ));                                                  
    
    return ctx;
}


void capture_deinit(CaptureContext *ctx)
{
    // Causes a segfault
    // snd_ctl_card_info_free(ctx->card_info);
    // snd_pcm_info_free(ctx->pcm_info);
    
    check_snd(snd_pcm_close(ctx->pcm));
    check_snd(snd_ctl_close(ctx->card_ctl));
    snd_config_update_free_global();
    free(ctx);
}

