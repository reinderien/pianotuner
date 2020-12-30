#include <math.h>

#include "util.h"

// mod1, always rounding down. The way literally everybody wants this function
// to actually behave, to such an extent that this is actually just how it's
// implemented in python.
float mod1rd(float x)
{
    // A valid pointer is needed for modff to save the integral part
    // of its input to. Passing NULL will cause it to segfault.
    float mod1, int_part;
    mod1 = modff(x, &int_part);
    if (x < 0)
    {
        if (mod1 == 0)
            return 0;
        else
            return 1 + mod1;
    }
    else
        return mod1;
}

float clip(float x)
{
    return fmaxf(fminf(x, 1), 0);
}
