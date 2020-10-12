#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <cblas.h>

#include "capture.h"
#include "freq.h"

#define METER 1
#define DUMP_ONE 1


static void meter(
    const sample_t *restrict samples, 
    int n_samples,
    double calc_time
)
{
    sample_t max = 0;
    int sum = 0, asum = 0;
    for (int i = 0; i < n_samples; i++)
    {
        sample_t x = samples[i],
                px = abs(x);
        max = px > max ? px : max;
        sum += x;
        asum += px;
    }

    printf(
        "max=%-6d ave=%-6.1f pow=%-6.1f t=%.3e\r",
        max,
        sum / (float)n_samples,
        asum / (float)n_samples,
        calc_time
    );
    fflush(stdout);
}


static void dump_one(
    const sample_t *restrict samples, 
    const float *restrict output,
    int N
)
{
    FILE *f = fopen("dump.csv", "w");
    assert(f);
    
    fputs("Index,V,freq,AC\n", f);
    
    for (int i = 0; i < N; i++)
    {
        fprintf(f, "%d,%d,", i, samples[i]);
        if (i)
            fprintf(f, "%f", 44100./i);
        fprintf(f, ",%f\n", output[i]);
    }
    
    assert(!fclose(f));
    exit(0);
}


static void autocorrelate(
    const float *restrict input,
    float *restrict output,
    int N
)
{
    for (int i = 0; i < N; i++)
    {
        output[i] = cblas_sdot(
            N-i,     // N
            input,   // X
            1,       // incX
            input+i, // Y
            1        // incY
        );
    }
}


void consume(const sample_t *samples, int n_samples)
{
    float *input = calloc(n_samples, sizeof(float)),
         *output = calloc(n_samples, sizeof(float));
    assert(input);
    assert(output);
    
    for (int i = 0; i < n_samples; i++)
        input[i] = (float)samples[i];
        
#if METER
    struct timespec t1, t2;
    assert(!clock_gettime(CLOCK_MONOTONIC_RAW, &t1));
#endif

    autocorrelate(input, output, n_samples);

#if METER
    assert(!clock_gettime(CLOCK_MONOTONIC_RAW, &t2));
    double dur = t2.tv_sec - t1.tv_sec + 1e-9*(t2.tv_nsec - t1.tv_nsec);
    meter(samples, n_samples, dur);
#endif

#if DUMP_ONE
    dump_one(samples, output, n_samples);
#endif

    free(input);
    free(output);
}

