#include <assert.h>
#include <float.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <cblas.h>

#include "capture.h"
#include "freq.h"

#define METER 1
#define DUMP_ONE 0


static void meter(
    const sample_t *restrict samples, 
    int n_samples
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
        "max=%6d ave=%6.1f pow=%6.1f ",
        max,
        sum / (float)n_samples,
        asum / (float)n_samples
    );
}


static void dump_one(
    const float *restrict output,
    int N, int rate
)
{
    FILE *f = fopen("dump.csv", "w");
    assert(f);
    
    fputs("Index,f,a\n", f);
    for (int i = 0; i < N; i++)
    {
        fprintf(
            f, "%d,%f,%f\n",
            i, rate/(float)i, output[i]
        );
    }
    
    assert(!fclose(f));
    exit(0);
}


#define AC() cblas_sdot(   \
    N - i,     /* len  */  \
    input,     /* x    */  \
    1,         /* incX */  \
    input + i, /* y    */  \
    1          /* incY */  \
)


static int peak_start(
    const float *restrict input,
#if DUMP_ONE
    float *restrict output,
#endif
    int N,
    float energy,
    float *restrict amin
)
{
    const float peak_thresh = 0.5;
    *amin = FLT_MAX;

    for (int i = 1; i < N; i++)
    {
        float a = AC()/energy;
    #if DUMP_ONE
        output[i] = a;
    #endif

        float diff = a - *amin;
        if (diff < 0)
            *amin = a;
        else if (diff >= peak_thresh)
            return i + 1;
    }
    
    // No good peak found
    return -1;
}


static int peak_top(
    const float *restrict input,
#if DUMP_ONE
    float *restrict output,
#endif
    int N,
    float energy,
    float amin,
    float *restrict a2max,
    float *restrict a1max,
    float *restrict a0max,
    int istart,
    int *restrict imax
)
{
    float a2,
          a1 = amin,
          a0 = amin;
    *a2max = amin;
    *a1max = amin;
    *a0max = amin;
    *imax = istart;

    int i;
    for (i = istart; i < N && i < *imax *3/2; i++)
    {
        a2 = a1;
        a1 = a0;
        a0 = AC()/energy;
    #if DUMP_ONE
        output[i] = a0;
    #endif

        if (*a1max < a1)
        {
            *a2max = a2;
            *a1max = a1;
            *a0max = a0;
            *imax = i;
        }   
    }
    return i;
}


static float parafit(
    float a, float b, float c
)
{
    return (a - c) / (a + c - 2*b) / 2;
}


static bool autocorrelate(
    const float *restrict input,
    int N,
    int rate,
    float *restrict energy,
    float *restrict freq
)
{
#if DUMP_ONE
    float *output = calloc(N, sizeof(float));
    assert(output);
#endif

    /*
    We cannot simply take the max peak, because there can be false peaks 33%+ 
    the magnitude of the first real peak. So looking across the entire
    autocorrelated spectrum is both wrong and slow.
    */
    
    int i = 0;
    *energy = AC();
#if DUMP_ONE
    output[0] = 1;
#endif
#if METER
    printf("energy=%8.2g ", *energy);
#endif

    /*
    1. Maintain a running minimum;
    2. Compare the normalized current value to the normalized minimum;
    3. If over some fixed value like 25%, declare the beginning of the first 
       peak
    */
    float amin;
    int istart = peak_start(
        input,
    #if DUMP_ONE
        output,
    #endif
        N, *energy, &amin);
    if (istart == -1)
    {
    #if METER
        printf(
            "                                           "
            "                                           ");
    #endif
        return false;
    }
    
    /*
    Look for the max over another 50% time, stopping halfway to the next peak.
    */
    float a2max, a1max, a0max;
    int imax, istop = peak_top(
        input,
    #if DUMP_ONE
        output,
    #endif
        N, *energy, amin, &a2max, &a1max, &a0max, istart, &imax);

    float delta = parafit(a2max, a1max, a0max);
    *freq = rate / (imax + delta);

#if METER
    float pd = a1max - amin;
    printf(
        "amin=%5.2f amax=%5.2f istart=%4d imax=%4d istop=%4d delta=%5.2f pd=%5.2f f=%7.1f ",
         amin,     a1max,      istart,    imax,    istop,    delta,      pd,     *freq
    );
#endif

#if DUMP_ONE
    if (istart < 40)
        dump_one(output, istop, rate);
    free(output);
#endif

    return freq;
}


void consume(const sample_t *samples, int n_samples, int rate)
{
    float *input = calloc(n_samples, sizeof(float));
    assert(input);
    
    for (int i = 0; i < n_samples; i++)
        input[i] = (float)samples[i];
        
#if METER
    meter(samples, n_samples);
    
    struct timespec t1, t2;
    assert(!clock_gettime(CLOCK_MONOTONIC_RAW, &t1));
#endif

    float energy, freq;
    bool success = autocorrelate(input, n_samples, rate, &energy, &freq);

#if METER
    assert(!clock_gettime(CLOCK_MONOTONIC_RAW, &t2));
    double dur = t2.tv_sec - t1.tv_sec + 1e-9*(t2.tv_nsec - t1.tv_nsec);
    printf("t=%6.3e \n", dur);
    fflush(stdout);
#endif

    free(input);
}

