from typing import Callable, Tuple

import numpy as np
import pyfftw
import re
import time
from multiprocessing import cpu_count
from pathlib import Path
from pyfftw.pyfftw import FFTW

import audio
import params
from params import f_to_fft, fft_to_f, f_to_n, LOG_2

AxisPair = Tuple[
    np.ndarray,  # freq (horizontal) axis
    np.ndarray,  # power (vertical) axis
]
SpectrumFn = Callable[[], AxisPair]


class FFTError(Exception):
    pass


def init_fft(read_audio: audio.ReadFn) -> SpectrumFn:
    start = time.monotonic()
    fft_in = pyfftw.zeros_aligned(shape=params.n_fft_in, dtype=np.float32)
    fft_out = pyfftw.empty_aligned(shape=params.n_fft_out, dtype=np.complex64)
    n_cpus = cpu_count()
    types = ('double', 'float', 'ldouble')
    wisdom_fns = [Path(f'.fftw_wisdom_{t}') for t in types]
    flags = ('FFTW_MEASURE',)
    has_wisdom = False

    if all(p.exists() for p in wisdom_fns):
        print('Loading FFT wisdom...', end=' ')
        wisdoms = [p.read_bytes() for p in wisdom_fns]
        statuses = pyfftw.import_wisdom(wisdoms)
        for status, path in zip(statuses, wisdom_fns):
            if not status:
                print(f'Invalid wisdom in {path}')
                break
        else:
            flags += ('FFTW_WISDOM_ONLY',)
            has_wisdom = True

    if not has_wisdom:
        print(f'Planning FFT wisdom on {n_cpus} cpus...', end=' ')

    fft = pyfftw.FFTW(
        fft_in, fft_out,
        direction='FFTW_FORWARD',
        flags=flags,
        threads=n_cpus,
        planning_timelimit=60,
    )
    wisdoms = pyfftw.export_wisdom()
    for wisdom, path in zip(wisdoms, wisdom_fns):
        path.write_bytes(wisdom)

    n_codelets = sum(
        len(re.findall(r'codelet', wisdom.decode()))
        for wisdom in wisdoms
    )
    end = time.monotonic()
    print(f'{n_codelets} codelets in {end-start:.1f}s')

    return make_spectrum_fn(fft, fft_in, fft_out, read_audio)


def make_spectrum_fn(
    fft: FFTW,
    fft_in: np.ndarray,
    fft_out: np.ndarray,
    read_audio: audio.ReadFn,
) -> SpectrumFn:
    f_tune_exact = 440
    f_left, f_right = f_tune_exact/np.sqrt(2), f_tune_exact*np.sqrt(2)
    i_left, i_right = f_to_fft(f_left), f_to_fft(f_right)
    fft_indices = np.arange(i_left, i_right + 1)
    freqs = fft_to_f(fft_indices)
    cents = 1_200 * np.log(freqs / f_tune_exact) / LOG_2

    def fn() -> AxisPair:
        # Read up to n_fft_in samples; usually it will be much smaller
        samples = read_audio(params.n_fft_in)

        n = len(samples)
        if n:
            # Shift the existing data left within the same array
            fft_in[:-n] = fft_in[n:]
            # Copy new data into the end of the array
            fft_in[-n:] = samples

            fft()

        fund = fft_out[i_left: i_right+1]
        return cents, np.abs(fund)

    return fn
