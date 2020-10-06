#!/usr/bin/env python3

from math import *


def mu(note):
    return 2**(note/12)


n_notes = 88
f_a4 = 440                      # cycles/sec
a4_index = 4*12                 # notes
f_samp = 48_000                 # samples/sec
f_min = f_a4 * mu(-a4_index)    # cycles/sec
f_max = f_min * mu(n_notes - 1) # cycles/sec
f_upper = f_samp/2              # cycles/sec
f_min_tune = f_min * mu(-1)     # cycles/sec
f_max_tune = f_max * mu(1)      # cycles/sec
t_min = 1 / f_min_tune          # secs/cycle
samp_min = t_min*f_samp         # samples/cycle
samp_act = 2**ceil(log(samp_min)/log(2))  # samples/cycle
t_act = samp_act / f_samp       # secs/cycle
f_lower = 1/t_act               # cycles/sec
lower_index = 12 * log(f_lower/f_min) / log(2)

print(f't_min = {t_min*1e3:.1f}ms')
print(f't_act = {t_act*1e3:.1f}ms')
print(f'Min samples = {samp_min:.0f}')
print(f'Act samples = {samp_act}')
print(f'Min detectable index = {lower_index:.1f}')
print(f'Min detectable freq = {f_lower:.3f} (also spectral resolution)')
print(f'f_mintune = {f_min_tune:.3f}')
print(f'f_min = {f_min:.3f}')
print(f'f_max = {f_max:.1f}')
print(f'f_maxtune = {f_max_tune:.1f}')
print(f'Max detectable freq = {f_upper}')
print(f'Max detectable harmonic = {f_upper/f_max_tune:.1f}')

