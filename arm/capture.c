#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <asoundlib.h>

#include "capture.h"
#include "freq.h"


#define EXCRUCIATING_DETAIL 0

#define AGC false

// Has no effect?
#define VOLUME 0.75


struct CaptureContextTag
{
    snd_output_t *output;
	snd_pcm_t *pcm;
	
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
    static const char *names[] =
    {
        "GENERIC",
        "MULTI",
        "MODEM",
        "DIGITIZER"
    };
    assert(class <= SND_PCM_CLASS_LAST);
    return names[class];
}


static const char *snd_pcm_subclass_name(snd_pcm_subclass_t subclass)
{
    static const char *names[] =
    {
        "GENERIC MIX",
        "MULTI MIX"
    };
    assert(subclass <= SND_PCM_SUBCLASS_LAST);
    return names[subclass];
}


static const char *snd_ctl_type_name(snd_ctl_type_t type)
{
    static const char *names[] =
    {
        "HW",
        "SHM",
        "INET",
        "EXT"
    };
    assert(type <= SND_CTL_TYPE_EXT);
    return names[type];
}


static const char *snd_ctl_power_state_name(unsigned state)
{
    switch (state)
    {
        case SND_CTL_POWER_D0:
            return "D0";
        case SND_CTL_POWER_D1:
            return "D1";
        case SND_CTL_POWER_D2:
            return "D2";
        case SND_CTL_POWER_D3hot:
            return "D3hot";
        case SND_CTL_POWER_D3cold:
            return "D3cold";
        default:
            assert(false);
    }
}


static void open_ctl(
    CaptureContext *restrict ctx, 
    snd_ctl_t **restrict ctl,
    snd_pcm_info_t *restrict pcm_info
)
{
    const int mode =
          SND_PCM_NO_AUTO_RESAMPLE
        | SND_PCM_NO_AUTO_CHANNELS
        | SND_PCM_NO_AUTO_FORMAT
        | SND_PCM_NO_SOFTVOL
    ;
    check_snd(snd_pcm_open(
        &ctx->pcm,
        "default",
        SND_PCM_STREAM_CAPTURE,
        mode
    ));
    
	check_snd(snd_pcm_info(ctx->pcm, pcm_info));

    char name[16];
    assert(snprintf(
        name, 16,
        "hw:%u",
        snd_pcm_info_get_card(pcm_info)
    ));
    check_snd(snd_ctl_open(
        ctl, name, SND_CTL_READONLY
    ));
}


static void init_pcm(CaptureContext *restrict ctx)
{
	snd_pcm_hw_params_t *hwparams;
    snd_pcm_hw_params_alloca(&hwparams);
    assert(hwparams);

    check_snd(snd_pcm_hw_params_any(ctx->pcm, hwparams));

    check_snd(snd_pcm_hw_params_set_access(
        ctx->pcm,
        hwparams,
        // Interleaved has no effect since we have only one channel,
        // but it's the default - so fine
        SND_PCM_ACCESS_MMAP_INTERLEAVED
    ));

    check_snd(snd_pcm_hw_params_set_format(
        ctx->pcm,
        hwparams,
        // We use S16_LE, the only format supported by the hardware.
        // BLAS requires floating-point, so SND_PCM_FORMAT_FLOAT_LE
        // would be nice, but it reads all-zero, so just do it ourselves.
        SND_PCM_FORMAT_S16_LE
    ));

    check_snd(snd_pcm_hw_params_set_channels(ctx->pcm, hwparams, 1));

    check_snd(snd_pcm_hw_params_set_rate_resample(
        ctx->pcm, hwparams, false
    ));

    // Set minimum rate based on Nyquist frequency of max note
    int direction = 0;
    ctx->rate = 2*FMIN;
    check_snd(snd_pcm_hw_params_set_rate_min(
        ctx->pcm, hwparams, &ctx->rate, &direction
    ));
    check_snd(snd_pcm_hw_params_set_rate_first(
        ctx->pcm, hwparams, &ctx->rate, &direction
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
        ctx->pcm, hwparams, ctx->period, direction
    ));

    check_snd(snd_pcm_hw_params_set_buffer_size(
        ctx->pcm, hwparams, 2*ctx->period
    ));

    // Time out after 25% overrun
    ctx->timeout_us = (unsigned)(ctx->period * 1.25 / ctx->rate * 1e6),
    ctx->timeout_ms = ctx->timeout_us / 1000;

    check_snd(snd_pcm_hw_params(ctx->pcm, hwparams));
}


static void describe(
    const CaptureContext *restrict ctx,
    snd_ctl_t *restrict ctl,
    const snd_pcm_info_t *restrict pcm_info
)
{
    snd_ctl_card_info_t *card_info;
    snd_ctl_card_info_alloca(&card_info);
    assert(card_info);
    check_snd(snd_ctl_card_info(ctl, card_info));

    unsigned pow_state;
    check_snd(snd_ctl_get_power_state(ctl, &pow_state));
    printf(
        "Control ------------------------------------------------------------\n"
        "  name        : %s\n"
        "  type        : %s\n"
        "  power state : %s\n"
        "\n",
        snd_ctl_name(ctl),
        snd_ctl_type_name(snd_ctl_type(ctl)),
        snd_ctl_power_state_name(pow_state)
    );

    printf(
        "Card ---------------------------------------------------------------\n"
        "  id         : %s\n"
        "  components : %s\n"
        "  driver     : %s\n"
        "  short name : %s\n"
        "  long name  : %s\n"
        "  mixer      : %s\n"
        "\n",
        snd_ctl_card_info_get_id(card_info),
        snd_ctl_card_info_get_components(card_info),
        snd_ctl_card_info_get_driver(card_info),
        snd_ctl_card_info_get_name(card_info),
        snd_ctl_card_info_get_longname(card_info),
        snd_ctl_card_info_get_mixername(card_info)
    );

    printf(
        "PCM ----------------------------------------------------------------\n"
        "  card        : %d\n"
        "  device      : %d\n"
        "  subdev index/avail/total : %d/%d/%d\n"
        "  name        : %s\n"
        "  id          : %s\n"
        "  subdev name : %s\n"
        "  class       : %s\n"
        "  subclass    : %s\n"
        "\n",
        snd_pcm_info_get_card(pcm_info),
        snd_pcm_info_get_device(pcm_info),
        snd_pcm_info_get_subdevice(pcm_info),
        snd_pcm_info_get_subdevices_avail(pcm_info),
        snd_pcm_info_get_subdevices_count(pcm_info),
        snd_pcm_info_get_name(pcm_info),
        snd_pcm_info_get_id(pcm_info),
        snd_pcm_info_get_subdevice_name(pcm_info),
        snd_pcm_class_name(
            snd_pcm_info_get_class(pcm_info)
        ),
        snd_pcm_subclass_name(
            snd_pcm_info_get_subclass(pcm_info)
        )
    );
}


static void describe_set_elems(
    const CaptureContext *restrict ctx,
    snd_ctl_t *restrict ctl
)
{
    snd_ctl_elem_list_t *elements;
    snd_ctl_elem_list_alloca(&elements);
    check_snd(snd_ctl_elem_list(ctl, elements));

    int n_elements = snd_ctl_elem_list_get_count(elements);
    check_snd(snd_ctl_elem_list_alloc_space(elements, n_elements));

    // It's goofy that we have to call this a second time, but it's needed
    check_snd(snd_ctl_elem_list(ctl, elements));
    assert(n_elements == snd_ctl_elem_list_get_count(elements));

	snd_ctl_elem_id_t *id;
	snd_ctl_elem_id_alloca(&id);
	assert(id);

    snd_ctl_elem_info_t *elem;
	snd_ctl_elem_info_alloca(&elem);
	assert(elem);

    snd_ctl_elem_value_t *value;
	snd_ctl_elem_value_alloca(&value);
	assert(value);

	bool vol_set = false, agc_set = false;

	puts(
        "Control elements ---------------------------------------------------\n"
    );

    for (int e = 0; e < n_elements; e++)
    {
        snd_ctl_elem_list_get_id(elements, e, id);
        snd_ctl_elem_info_set_id(elem, id);
	    check_snd(snd_ctl_elem_info(ctl, elem));

	    snd_ctl_elem_type_t type = snd_ctl_elem_info_get_type(elem);

#if EXCRUCIATING_DETAIL
	    const char *item_name;
	    unsigned items;
	    if (type == SND_CTL_ELEM_TYPE_ENUMERATED)
	    {
	        item_name = snd_ctl_elem_info_get_item_name(elem);
	        items = snd_ctl_elem_info_get_items(elem);
        }
        else
        {
            item_name = "<non-enumerated>";
            items = 0;
        }
#endif

        char min[32], max[32], step[32];
        long max_int = 0;
        switch (type)
        {
        case SND_CTL_ELEM_TYPE_INTEGER:
            max_int = snd_ctl_elem_info_get_max(elem);
            snprintf(min, 32, "%ld", snd_ctl_elem_info_get_min(elem));
            snprintf(max, 32, "%ld", max_int);
            snprintf(step, 32, "%ld", snd_ctl_elem_info_get_step(elem));
            break;
        case SND_CTL_ELEM_TYPE_INTEGER64:
            snprintf(min, 32, "%lld", snd_ctl_elem_info_get_min64(elem));
            snprintf(max, 32, "%lld", snd_ctl_elem_info_get_max64(elem));
            snprintf(step, 32, "%lld", snd_ctl_elem_info_get_step64(elem));
            break;
        default:
            strncpy(min, "<non-integer>", 32);
            strncpy(max, "<non-integer>", 32);
            strncpy(step, "<non-integer>", 32);
        }

        const char *name = snd_ctl_elem_info_get_name(elem);
        bool readable = snd_ctl_elem_info_is_readable(elem),
             writable = snd_ctl_elem_info_is_writable(elem);
        const int index = snd_ctl_elem_info_get_index(elem);

        printf(
            "  name       : %s\n"
            "  type       : %s\n"
            "  numid      : %u\n"
        #if EXCRUCIATING_DETAIL
            "  count      : %u\n"
            "  device     : %u\n"
            "  subdevice  : %u\n"
            "  dimension  : %d\n"
            "  dimensions : %d\n"
            "  index      : %u\n"
        #endif
            "  interface  : %s\n"
        #if EXCRUCIATING_DETAIL
            "  item name  : %s\n"
            "  items      : %u\n"
        #endif
            "  min        : %s\n"
            "  max        : %s\n"
        #if EXCRUCIATING_DETAIL
            "  step       : %s\n"
            "  owner      : %d\n"
            "  inactive   : %d\n"
            "  locked     : %d\n"
            "  is owner        : %d\n"
            "  is user         : %d\n"
        #endif
            "  is readable     : %d\n"
            "  is writable     : %d\n"
        #if EXCRUCIATING_DETAIL
            "  is volatile     : %d\n"
            "  tlv commandable : %d\n"
            "  tlv readable    : %d\n"
            "  tlv writeable   : %d\n"
        #endif
            ,
            name,
            snd_ctl_elem_type_name(
                snd_ctl_elem_info_get_type(elem)
            ),
            snd_ctl_elem_info_get_numid(elem),
            #if EXCRUCIATING_DETAIL
                snd_ctl_elem_info_get_count(elem),
                snd_ctl_elem_info_get_device(elem),
                snd_ctl_elem_info_get_subdevice(elem),
                snd_ctl_elem_info_get_dimension(elem, e),
                snd_ctl_elem_info_get_dimensions(elem),
                index,
            #endif
            snd_ctl_elem_iface_name(
                snd_ctl_elem_info_get_interface(elem)
            ),
            #if EXCRUCIATING_DETAIL
                item_name,
                items,
            #endif
            min,
            max,
            #if EXCRUCIATING_DETAIL
                step,
                snd_ctl_elem_info_get_owner(elem),
                snd_ctl_elem_info_is_inactive(elem),
                snd_ctl_elem_info_is_locked(elem),
                snd_ctl_elem_info_is_owner(elem),
                snd_ctl_elem_info_is_user(elem),
            #endif
            readable,
            writable
            #if EXCRUCIATING_DETAIL
                ,
                snd_ctl_elem_info_is_volatile(elem),
                snd_ctl_elem_info_is_tlv_commandable(elem),
                snd_ctl_elem_info_is_tlv_readable(elem),
                snd_ctl_elem_info_is_tlv_writable(elem)
            #endif
        );

        snd_ctl_elem_value_set_id(value, id);

        bool is_agc = false, is_vol = false;
        if (!strcmp(name, "Auto Gain Control"))
            is_agc = true;
        else if (!strcmp(name, "Mic Capture Volume"))
            is_vol = true;
        if (is_vol || is_agc)
        {
            assert(writable);

            if (is_vol)
            {
                snd_ctl_elem_value_set_integer(
                    value,
                    index,
                    (long)(VOLUME * max_int)
                );
                vol_set = true;
            }
            else if (is_agc)
            {
                snd_ctl_elem_value_set_boolean(value, index, AGC);
                agc_set = true;
            }

            check_snd(snd_ctl_elem_write(ctl, value));
        }

        if (readable)
        {
            check_snd(snd_ctl_elem_read(ctl, value));

            char val_str[64];

            switch (type)
            {
            case SND_CTL_ELEM_TYPE_INTEGER:
                snprintf(
                    val_str, 64, "%ld",
                    snd_ctl_elem_value_get_integer(value, index)
                );
                break;

            case SND_CTL_ELEM_TYPE_INTEGER64:
                snprintf(
                    val_str, 64, "%lld",
                    snd_ctl_elem_value_get_integer64(value, index)
                );
                break;

            case SND_CTL_ELEM_TYPE_BOOLEAN:
                strncpy(
                    val_str,
                    snd_ctl_elem_value_get_boolean(value, index)
                    ? "TRUE" : "FALSE",
                    64
                );
                break;

            default:
                strncpy(val_str, "Unsupported type", 64);
                break;
            }

            printf("  value      : %s\n", val_str);
        }

        putchar('\n');
    }

    assert(vol_set);
    assert(agc_set);
}


static void describe_params(const CaptureContext *restrict ctx)
{
    puts(
        "Parameters ---------------------------------------------------------");
    check_snd(snd_pcm_dump(ctx->pcm, ctx->output));

    printf(
        "\n"
        "Constraints --------------------------------------------------------\n"
        "  period : %u > %d\n"
        "  rate   : %u > %d\n"
        "  fmin   : %.1f < %.1f\n"
        "  fmax   : %d > %d\n"
        "\n",
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

    ctx->restart = false;
    ctx->prev_state = -1;  // The first state will always be "new"

    const bool close = false;
    check_snd(snd_output_stdio_attach(
        &ctx->output,
        stdout,
        close
    ));

    snd_pcm_info_t *pcm_info;
    snd_pcm_info_alloca(&pcm_info);
    assert(pcm_info);

    snd_ctl_t *ctl;
    open_ctl(ctx, &ctl, pcm_info);
    describe(ctx, ctl, pcm_info);
    describe_set_elems(ctx, ctl);
    check_snd(snd_ctl_close(ctl));

    init_pcm(ctx);
    describe_params(ctx);
    check_snd(snd_pcm_start(ctx->pcm));

    return ctx;
}


void capture_deinit(CaptureContext *ctx)
{
    warn_snd(snd_pcm_close(ctx->pcm));
    snd_config_update_free_global();

    free(ctx);

    puts("Capture deinitialized");
}


/*
All of the following is based loosely on the "direct write only" method
shown in alsa-lib's test/pcm.c
*/


static bool resume(const CaptureContext *restrict ctx)
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


static bool recover(const CaptureContext *restrict ctx, int err)
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


static bool recover_err(const CaptureContext *restrict ctx, int err)
{
    fprintf(
        stderr,
        "Trying recovery from error %d - %s\n",
        err, snd_strerror(err)
    );
    return recover(ctx, err);
}


static bool recover_state(
    CaptureContext *restrict ctx,
    snd_pcm_state_t state
)
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


static snd_pcm_sframes_t capture_wait(CaptureContext *restrict ctx)
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
        const sample_t *samples,
        int n_samples,
        int rate
    )
) {
    snd_pcm_sframes_t avail;
    do
        avail = capture_wait(ctx);
    while (avail == 0);

    const snd_pcm_channel_area_t *areas;
    snd_pcm_uframes_t offset, frames = ctx->period;
    int err = (snd_pcm_mmap_begin(ctx->pcm, &areas, &offset, &frames));
    if (err < 0)
    {
        warn_snd(err);
        return;
    }

    // Assume start offset bit-alignment
    assert(areas->first % 8 == 0);  
    // Assume fully-contiguous samples
    assert(areas->step == 8*sizeof(sample_t));

    snd_pcm_uframes_t used;
    if (frames >= ctx->period)
    {
        used = ctx->period;

        const sample_t *samples = (sample_t*)(
            (uint8_t*)areas->addr
            + (areas->first / 8)
        ) + offset;

        consume(samples, ctx->period, ctx->rate);
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
        ctx->pcm, offset, used
    );
    warn_snd(transferred);
}

