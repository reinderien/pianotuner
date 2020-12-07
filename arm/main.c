#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <time.h>

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
    gauge = gauge_init();

    const struct timespec rqtp_1ms = {.tv_nsec = 1000000};

    while (true)
    {
        gauge_message(gauge, 0.2, 0.4, 0.6, 0.8);
        nanosleep(&rqtp_1ms, NULL);

        // capture_period(capture, consume);
    }
}

