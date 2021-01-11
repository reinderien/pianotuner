from typing import Callable, Tuple, List

import numpy as np
import pyfftw
import re
import time
from multiprocessing import cpu_count
from pathlib import Path
from pyfftw.pyfftw import FFTW

import audio
import params
from params import f_to_fft, fft_to_f, LOG_2


N_HARMS = 5
YMAX = 50


AxisPair = Tuple[
    List[np.ndarray],  # freq (horizontal) axes
    List[np.ndarray],  # power (vertical) axes
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
            has_wisdom = True

    def make_fft():
        flags = {'FFTW_MEASURE'}

        if not has_wisdom:
            print(f'Planning FFT wisdom on {n_cpus} cpus...', end=' ')
            flags.add('FFTW_WISDOM_ONLY')

        return pyfftw.FFTW(
            fft_in, fft_out,
            direction='FFTW_FORWARD',
            flags=flags,
            threads=n_cpus,
            planning_timelimit=60,
        )
    try:
        fft = make_fft()
    except RuntimeError as e:
        if 'No FFTW wisdom is known for this plan' in e.args[0]:
            print(str(e))
            has_wisdom = False
            fft = make_fft()
        else:
            raise

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


def make_axes() -> Tuple[
    List[np.ndarray],       # List of cent axes for each harmonic
    List[Tuple[int, int]],  # Boundary indices for each harmonic
]:
    # todo - variable.
    f_tune_exact = 440
    SQ2 = np.sqrt(2)

    # This can't really be vectorized because these will be jagged.
    cents = []
    bounds = []
    for h in range(1, N_HARMS + 1):
        f_left, f_right = h * f_tune_exact/SQ2, h * f_tune_exact*SQ2
        i_left, i_right = f_to_fft(f_left), f_to_fft(f_right)
        bounds.append((i_left, i_right))

        fft_indices = np.arange(i_left, i_right + 1)
        freqs = fft_to_f(fft_indices)
        cents.append(1_200 * np.log(freqs / f_tune_exact/h) / LOG_2)

    return cents, bounds


def make_spectrum_fn(
    fft: FFTW,
    fft_in: np.ndarray,
    fft_out: np.ndarray,
    read_audio: audio.ReadFn,
) -> SpectrumFn:
    cents, bounds = make_axes()

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

        harms = []
        for left, right in bounds:
            harm = np.abs(fft_out[left: right+1])
            yfmax = np.max(harm)
            if yfmax > YMAX:
                harm *= YMAX/yfmax
            harms.append(harm)

        return cents, harms

    return fn
