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
    const float *restrict buffer,
    int n_samples
)
{
    FILE *f = fopen("dump.csv", "w");
    assert(f);
    fputs("Index,V,f\n", f);
    for (int i = 0; i < n_samples; i++)
        fprintf(f, "%d,%d,%f\n", i, samples[i], buffer[i]);
    assert(!fclose(f));
    exit(0);
}


static void autocorrelate(
    const sample_t *restrict samples, 
    float *restrict buffer, 
    int n_samples
)
{
    for (int i = 0; i < n_samples; i++)
        buffer[i] = (float)samples[i];

    gsl_block_float input_block = {
        .data = buffer,
        .size = n_samples
    };

    gsl_vector_float x = {
        .data = buffer,
        .size = n_samples,
        .stride = 1,
        .block = &input_block,
        .owner = false
    };
    
    const gsl_matrix_float A = {
        .size1 = n_samples,
        .size2 = n_samples,
        .tda = 1,  // This is a hack to re-use a single buffer
        .data = buffer,
        .block = &input_block,
        .owner = false
    };
    
    int err = gsl_blas_strmv(
        CblasLower,
        CblasNoTrans,
        CblasNonUnit,
        &A, &x
    );
    if (err)
    {
        fprintf(
            stderr, "GSL failure: %d = %s\n", err, gsl_strerror(err)
        );
    }
        
}


void consume(const sample_t *samples, int n_samples)
{
    float *buffer = calloc(n_samples, sizeof(float));
    assert(buffer);
    autocorrelate(samples, buffer, n_samples);

#if METER
    meter(samples, n_samples);
#endif
#if DUMP_ONE
    dump_one(samples, buffer, n_samples);
#endif

    free(buffer);
}

