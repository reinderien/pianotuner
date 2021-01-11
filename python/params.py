#!/usr/bin/env python3

import math
from typing import Union

import numpy as np


# https://en.wikipedia.org/wiki/Piano#/media/File:Piano_Frequencies.svg
NAMES = ('C', 'C♯', 'D', 'E♭', 'E', 'F', 'F♯', 'G', 'A♭', 'A', 'B♭', 'B')

LOG_2 = math.log(2)
SQ2 = math.sqrt(2)


def prev_pow_2(x: float) -> int:
    return 2 ** int(np.log(x) / LOG_2)


def next_pow_2(x: float) -> int:
    return 2 ** int(np.ceil(np.log(x) / LOG_2))


n_notes = 88         # semitones
n_a440 = 12*4        # semitones
f_a0 = f_min = 27.5  # cycles/sec
f_samp = 48_000      # samples/sec
t_window_min = 1     # seconds
framerate_min = 30   # frames/sec
n_harmonics = 5      # octaves
y_max = 50           # post-FFT audio y-units

f_upper = f_samp/2   # cycles/sec
samp_min = t_window_min*f_samp  # samples/cycle
n_window_samples = next_pow_2(samp_min)  # samples/cycle
t_window = n_window_samples / f_samp   # secs/cycle
f_lower = 1/t_window                   # cycles/sec
n_fft_in = n_window_samples            # samples
n_fft_out = n_window_samples // 2 + 1  # samples
frame_samples_max = f_samp / framerate_min       # samples/frame
n_frame_samples = prev_pow_2(frame_samples_max)  # samples/frame
framerate = f_samp / n_frame_samples   # frames/sec


def n_to_f(note: int) -> float:
    return f_a0 * np.power(2, note/12)


def f_to_n(freq: float) -> float:
    return 12 * np.log(freq/f_a0) / LOG_2


def n_to_name(n: float) -> str:
    # In application note space, A0 maps to index 0, but in musical note space
    # C is at 0
    n = int(round(n)) + 9
    return NAMES[n % 12] + str(int(n / 12))


def f_to_fft(f: float) -> int:
    return round(f / f_upper * n_fft_out)


def fft_to_f(i: Union[int, np.ndarray]) -> float:
    return i / n_fft_out * f_upper


f_max = n_to_f(n_notes - 1)    # cycles/sec
f_min_tune = n_to_f(-1)        # cycles/sec
f_max_tune = n_to_f(n_notes)   # cycles/sec
t_min = 1 / f_min_tune         # secs/cycle
samp_min = t_min*f_samp        # samples/cycle
lower_index = f_to_n(f_lower)  # semitones
n_worst = f_to_n(f_min + f_lower)  # semitones


def dump(verbose: bool = False):
    print('Audio parameters:')
    print(f't_window = {t_window:.2f}s')
    print(f'f_samp = {f_samp/1e3:.1f} kHz')
    print(f'framerate = {framerate:.1f} Hz')

    if verbose:
        print(f't_min = {t_min*1e3:.1f}ms')
        print(f'Min samples = {samp_min:.0f}')
        print(f'Act samples = {n_window_samples}')
        print(f'Min detectable index = {lower_index:.1f} semitones')
        print(f'Min detectable freq = {f_lower:.3f} Hz '
              f'(also spectral resolution)')
        print(f'Worst-case fundamental note resolution: {n_worst:.2f}')
        print(f'f_mintune = {f_min_tune:.3f} Hz')
        print(f'f_min = {f_min:.3f} Hz')
        print(f'f_max = {f_max/1e3:.3f} kHz')
        print(f'f_maxtune = {f_max_tune/1e3:.3f} kHz')
        print(f'Max detectable freq = {f_upper/1e3} kHz')
        print(f'Max detectable harmonic = {f_upper/f_max_tune:.1f}')

    print()
