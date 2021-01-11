#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cblas.h>
#include <string.h>

#include "util.h"
#include "capture.h"
#include "freq.h"
#include "gauge.h"
#include "vrb.h"


#define ACLEN 2048
#define POWER_THRESHOLD 64


static CaptureContext *capture = NULL;
static GaugeContext *gauge = NULL;



static void cleanup()
{
    putchar('\n'); // after the \r from consume()

    if (capture)
        capture_deinit(&capture);

    if (gauge)
        gauge_deinit(&gauge);
}


static void handle_sigint(int signal)
{
    exit(0); // will call cleanup anyway
}


static void consume(CaptureContext *cc, const sample_t *restrict samples, void *p)
{
    VRB *b = p;
    float *restrict present = b->present;
    unsigned N = capture_period(cc);
    for (unsigned i = 0; i < N; i++)
        present[i] = samples[i];
    vrb_advance(b, N*sizeof(float));
}
// Returns the power of the last capture, which will always be needed.
static float read_audio(CaptureContext *cc, VRB *b)
{
    capture_do_capture(cc, consume, b);

    unsigned N = capture_period(cc);
    float *last_cap = vrb_past(b, N*sizeof(float));
    return cblas_sdot(
        N,         // len
        last_cap,  // x
        1,         // incX
        last_cap,  // y
        1          // incY
    )/N;
}

static float power_to_db(float power)
{
    return clip((log10f(power) - 2)/5);
}

int main(int argc, const char **argv)
{
    if (atexit(cleanup))
    {
        perror("Failed to register deinit");
        exit(-1);
    }
    signal(SIGINT, handle_sigint);

    capture = capture_init();
    unsigned period = capture_period(capture);
    gauge = gauge_init();

    //gauge_demo(gauge);

    unsigned hist_len = ACLEN + period;
    VRB *hist = vrb_create(hist_len*sizeof(float));
    float ac[ACLEN];

    while (true)
    {
        float power;
continue_outer_while:
        power = read_audio(capture, hist);
        printf("%f %f\n", power, power_to_db(power));
        gauge_message(gauge, power_to_db(power), 0, 0, 0);
        if (power > POWER_THRESHOLD)
        {
            // There is a note. Load the history with the note before
            // attempting an autocorrelation.
            for (unsigned i = 0; i < (hist_len - 1)/period + 1; i++)
            {
                power = read_audio(capture, hist);
                printf("%f %f\n", power, power_to_db(power));
                gauge_message(gauge, power_to_db(power), 0, 0, 0);
                if (power < POWER_THRESHOLD)
                    goto continue_outer_while;
            }

            memset(ac, 0, sizeof(ac));
            while (true)
            {
                autocorrelate(vrb_past(hist, hist_len*sizeof(float)), hist_len,
                              ac, ACLEN);
                float f = freq(ac, ACLEN, capture_rate(capture));
                float octave = 0, semitone = 0, deviation = 0;
                if (f > 0)
                {
                    octave = log2f(f/C0);
                    semitone = mod1rd(octave + 1./24);
                    deviation = mod1rd(12*semitone);

                    octave = clip(octave/8);
                }
                printf(
                    "%f %f    %f %f %f %f\n",
                    power,
                    f,
                    power_to_db(power),
                    octave,
                    semitone,
                    deviation
                );
                gauge_message(
                    gauge,
                    power_to_db(power),
                    octave,
                    semitone,
                    deviation
                );

                power = read_audio(capture, hist);
                if (power < POWER_THRESHOLD)
                {
                    printf("%f %f\n", power, power_to_db(power));
                    gauge_message(gauge, power_to_db(power), 0, 0, 0);
                    goto continue_outer_while;
                }
            }
            //save(ac, sizeof(ac), "data/ac.bin");
        }
    }


    exit(1);
    return 255;
}
