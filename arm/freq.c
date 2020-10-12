#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <gsl/gsl_blas.h>
#include <gsl/gsl_block.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_vector.h>

#include "capture.h"
#include "freq.h"


#define METER 1
#define DUMP_ONE 1


static void meter(const sample_t *restrict samples, int n_samples)
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
        "max=%-6d ave=%-6.1f pow=%-6.1f\r",
        max,
        sum / (float)n_samples,
        asum / (float)n_samples
    );
    fflush(stdout);
}


static void dump_one(
    const sample_t *restrict samples, 
    const float *restrict output,
    int n_samples
)
{
    FILE *f = fopen("dump.csv", "w");
    assert(f);
    fputs("Index,V,f\n", f);
    for (int i = 0; i < n_samples; i++)
        fprintf(f, "%d,%d,%f\n", i, samples[i], output[i]);
    assert(!fclose(f));
    exit(0);
}


static void autocorrelate_l1(
    const sample_t *restrict samples, 
    float *restrict input,
    float *restrict output,
    int N
)
{
    for (int i = 0; i < N; i++)
        input[i] = (float)samples[i];

    gsl_block_float input_block = {
        .data = input,
        .size = N
    };

    gsl_vector_float x1 = {
        .data = input,
        .size = N,
        .stride = 1,
        .block = &input_block,
        .owner = false
    }, x2 = x1;

    for (int i = 0; i < N; i++)
    {
        int err = gsl_blas_sdot(&x1, &x2, output + i);
        if (err)
        {
            fprintf(
                stderr, "GSL failure: %d = %s\n", err, gsl_strerror(err)
            );
            exit(-1);
        }

        x1.size--;
        x2.size--;
        x2.data++;
    }
}


void consume(const sample_t *samples, int n_samples)
{
    float *input = calloc(n_samples, sizeof(float)),
         *output = calloc(n_samples, sizeof(float));
    assert(input);
    assert(output);
    autocorrelate_l1(samples, input, output, n_samples);

#if METER
    meter(samples, n_samples);
#endif
#if DUMP_ONE
    dump_one(samples, output, n_samples);
#endif

    free(buffer);
}

