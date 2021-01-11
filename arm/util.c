#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
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


// Limit x to the range [0, 1]. If x is below 0, 0 is outputted. If x is above
// 1, then 1 is outputted. Otherwise, x is outputted.
float clip(float x)
{
    return fmaxf(fminf(x, 1), 0);
}


// Save the memory block [mem, mem + size) to a file named fn.
void save(void *mem, size_t size, const char *fn)
{
    unsigned const SS = 512;

    int f = creat(fn, 0666);
    assert(f != -1);
    assert(ftruncate(f, size) == 0);

    unsigned nsectors = size/SS;
    size_t remainder = size%SS;
    for (unsigned s = 0; s < nsectors; s++)
        assert(write(f, mem + SS*s, SS) == SS);
    if (remainder > 0)
        assert(write(f, mem + SS*nsectors, remainder) == remainder);

    assert(close(f) == 0);
}
