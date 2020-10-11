#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "freq.h"


#define METER 1
#define DUMP_ONE 1


void consume(const int16_t *samples, int n_samples)
{
#if METER
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
#endif

#if DUMP_ONE
    FILE *f = fopen("dump.csv", "w");
    assert(f);
    fputs("Index,V\n", f);
    for (int i = 0; i < n_samples; i++)
        fprintf(f, "%d,%d\n", i, samples[i]);
    assert(!fclose(f));
    exit(0);
#endif
}


