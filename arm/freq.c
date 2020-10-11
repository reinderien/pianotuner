#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <gsl/block/gsl_block.h>
#include <gsl/vector/gsl_vector.h>

#include "freq.h"


#define METER 1
#define DUMP_ONE 1


static void meter(const int16_t *restrict samples, int n_samples)
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
        "max=%-6d ave=%-6.1f pow=%-6.1f\r",
        max,
        sum / (float)n_samples,
        asum / (float)n_samples
    );
    fflush(stdout);
}


static void dump_one(const int16_t *restrict samples, int n_samples)
{
    FILE *f = fopen("dump.csv", "w");
    assert(f);
    fputs("Index,V\n", f);
    for (int i = 0; i < n_samples; i++)
        fprintf(f, "%d,%d\n", i, samples[i]);
    assert(!fclose(f));
    exit(0);
}



static void autocorrelate(const int16_t *samples, int n_samples)
{
    gsl_block_short input_block = {
        .data = samples,
        .size = n_samples
    };

    gsl_vector_short v1 = {
        .data = samples,
        .size = n_samples,
        .stride = 1,
        .block = &input_block,
        .owner = false
    },
    v2 = v1;
}


void consume(const int16_t *samples, int n_samples)
{
    autocorrelate(samples, n_samples);

#if METER
    meter(samples, n_samples);
#endif
#if DUMP_ONE
    dump_one(samples, n_samples);
#endif
}

