#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "capture.h"


static CaptureContext *capture;


static void cleanup()
{
    putchar('\n'); // after the \r from consume()
    capture_deinit(capture);
}


static void handle_sigint(int signal)
{
    exit(0); // will call cleanup anyway
}


static void consume(const int16_t *samples, int n_samples)
{
    int max = INT_MIN, sum = 0, asum = 0;
    for (int i = 0; i < n_samples; i++)
    {
        int16_t x = samples[i],
               px = abs(x);
        max = px > max ? px : max;
        sum += x;
        asum += px;
    }

    printf(
        "max=%-6d ave=%-6d pow=%-6d\r",
        max,
        sum / n_samples,
        asum / n_samples
    );
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
    signal(SIGINT, handle_sigint);

    while (true)
        capture_period(capture, consume);
}

