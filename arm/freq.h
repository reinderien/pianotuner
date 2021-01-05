#pragma once

#include <stdint.h>

// 2**(1/12)
#define SEMI 1.0594630943592953

#define A440_OCTAVES 4
#define FMIN (440. / (1 << A440_OCTAVES))
#define N_NOTES 88
#define FMAX (FMIN * (1 << (N_NOTES/12)) *SEMI*SEMI*SEMI)
// The basis of octave number determination. Equal to 13.75*2**(3/12) exactly.
#define C0 16.351597831287414

/*
Example:
fmin = 27.5 Hz    tmax = 36.363 ms
fsamp = 44.1 kHz  tsamp = 22.676 us
samples_min = fsamp / fmin = 1604
*/

void autocorrelate(float *f, unsigned nf, float *ac, unsigned nac);
float freq(float *ac, unsigned nac, unsigned rate);
