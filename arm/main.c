#include <stdio.h>
#include <stdlib.h>

#include "capture.h"


static CaptureContext *capture;


static void cleanup()
{
    capture_deinit(capture);
}


static void consume(const int16_t *samples, int n_samples)
{
    fflush(stdout);
}


int main(int argc, const char **argv)
{
    capture = capture_init();

    if (atexit(cleanup))
    {
        perror("Failed to register deinit");
        exit(-1);
    }

    for (int i = 0; i < 50; i++)
    {
        capture_period(capture, consume);
    }
    putchar('\n');

    return 0;
}

