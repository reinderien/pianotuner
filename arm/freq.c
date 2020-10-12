#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <gsl/gsl_block.h>
#include <gsl/gsl_vector.h>

#include "capture.h"
#include "freq.h"


#define METER 1
#define DUMP_ONE 0


static void meter(const sample_t *restrict samples, int n_samples)
{
    sample_t max = -FLT_MAX, 
             sum = 0, asum = 0;
    for (int i = 0; i < n_samples; i++)
    {
        sample_t x = samples[i],
             px = fabs(x);
        max = px > max ? px : max;
        sum += x;
        asum += px;
    }

    printf(
        "max=%-6.1f ave=%-6.1f pow=%-6.1f\r",
        (float)max,
        sum / (float)n_samples,
        asum / (float)n_samples
    );
    fflush(stdout);
}


static void dump_one(const sample_t *restrict samples, int n_samples)
{
    FILE *f = fopen("dump.csv", "w");
    assert(f);
    fputs("Index,V\n", f);
    for (int i = 0; i < n_samples; i++)
        fprintf(f, "%d,%f\n", i, samples[i]);
    assert(!fclose(f));
    exit(0);
}


/*
static void autocorrelate(const float *samples, int n_samples)
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
*/

void consume(const sample_t *samples, int n_samples)
{
    // autocorrelate(samples, n_samples);

#if METER
    meter(samples, n_samples);
#endif
#if DUMP_ONE
    dump_one(samples, n_samples);
#endif
}

