#include <stdio.h>
#include <stdlib.h>
#include "capture.h"


void cleanup()
{
    capture_deinit();
}


int main(int argc, const char **argv)
{
    capture_init();
    
    if (atexit(cleanup))
    {
        perror("Failed to register deinit");
        exit(-1);
    }
        

    return 0;
}

