#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "util.h"
#include "capture.h"
#include "freq.h"
#include "gauge.h"


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



int main(int argc, const char **argv)
{
    if (atexit(cleanup))
    {
        perror("Failed to register deinit");
        exit(-1);
    }
    signal(SIGINT, handle_sigint);

    capture = capture_init();
    FreqContext freq = {
        .period = capture_period(capture),
        .rate = capture_rate(capture)
    };
    gauge = gauge_init();

    //gauge_demo(gauge);
    while (true) {
        capture_capture_period(capture, consume, &freq);
        float db = clip((log10f(freq.energy) - 2)/5);
        float octave = 0.5, semitone = 0.5, deviation = 0.5;
        if (freq.freq > 0) {
            octave = log2f(freq.freq/C0);
            semitone = mod1rd(octave);
            deviation = mod1rd(12*semitone + 0.5);

            octave = clip(octave/8);
        }
        printf("%f %f %f %f\n", db, octave, semitone, deviation);
        gauge_message(gauge, db, octave, semitone, deviation);
    }
}
