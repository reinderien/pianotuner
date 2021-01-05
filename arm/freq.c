#include <assert.h>
#include <float.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <cblas.h>

#include "freq.h"

#define PEAK_THRESHOLD 0x5p-3

// Calculate the autocorrelation, ac(t), of f(t). Mathematically,
//
// ac(t) = 1/D_int * integral from u = D_ac to D_f f(u)*f(u - t), where
// ac's domain is [0, D_ac],
// f's  domain is [0, D_f ], and
// D_int = D_f - D_ac
//
// ac and f are both sampled at even time intervals. It doesn't matter what
// those intervals are because the 1/D_int out in front turns the integral into an
// average. nac samples of ac are calculated and nf samples of f are used. The
// autocorrelation is added to *ac, allowing the user to accumulate
// autocorrelation data over time. Each dot product could have nf - nac + 1
// terms, but for alignment reasons, it is easier to only use nf - nac. That
// way, if f is 5 pages long and ac is 4 pages long, the dot products are
// exactly 1 page long. This also means that f[0] is not actually used.
// ex: nac = 8, nf = 12:
//f: 012.4...8...
//   .   .   0===
//   .   .  1===-
//   .   . 2-==--
//   .   .3--=---
//   .   4-------
//   .  5--- ----
//   . 6---  ----
//   .7---   ----
//           ^f0
void autocorrelate(float *f, unsigned nf, float *ac, unsigned nac)
{
    assert(nf > nac);
    unsigned ndp = nf - nac;
    float *f0 = f + nac;
    for (unsigned i = 0; i < nac; i++)
    {
        ac[i] += cblas_sdot(
            ndp,     // len
            f0,      // x
            1,       // incX
            f0 - i,  // y
            1        // incY
        )/ndp;
    }
}


// Calculate the x coordinate of the peak of a parabola passing through points
// (-1, a), (0, b), and (1, c).
static float parafit(float a, float b, float c)
{
    return (a - c) / (a + c - 2*b) / 2;
}


float freq(float *ac, unsigned nac, unsigned rate)
{
    unsigned i = 1;
    // Ignore everything before the first zero crossing.
    while (true)
    {
        if (i >= nac)
            return -1;
        if (ac[i] < 0)
            break;
        i++;
    }
    // Now, find the first peak above threshold.
    float thr = PEAK_THRESHOLD*ac[0];
    while (true)
    {
        if (i >= nac)
            return -1;
        if (ac[i] > thr)
            break;
        i++;
    }
    unsigned k = i;
    while (true)
    {
        if (k >= nac)
            return -1;
        if (ac[k] < thr)
            break;
        k++;
    }
    k--;
    unsigned j = (i + k)/2;
    unsigned dj = j - i;
    // Hope that the AC is mostly just a parabola between i and k.
    float jfit = (float)j + (float)dj * parafit(ac[j - dj], ac[j], ac[j + dj]);
    return (float)rate / jfit;
}
