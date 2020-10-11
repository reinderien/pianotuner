#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "capture.h"
#include "freq.h"


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

