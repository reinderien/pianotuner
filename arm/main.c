#include <stdio.h>
#include <stdlib.h>

#include "capture.h"


static CaptureContext *capture;


void cleanup()
{
    capture_deinit(capture);
}


int main(int argc, const char **argv)
{
    capture = capture_init();
    
    if (atexit(cleanup))
    {
        perror("Failed to register deinit");
        exit(-1);
    }
        

    return 0;
}

