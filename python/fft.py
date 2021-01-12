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
from params import f_to_fft, fft_to_f, n_to_f, LOG_2, SQ2


AxisPair = Tuple[
    List[np.ndarray],  # freq (horizontal) axes
    List[np.ndarray],  # power (vertical) axes
]
SpectrumFn = Callable[[], AxisPair]


h_indices = np.arange(1, params.n_harmonics + 1)

# Frequency coefficients to find harmonic section bounds
coefficients = np.full(
    params.n_harmonics + 1,
    params.n_fft_out / params.f_upper,
)
coefficients[0] /= SQ2
coefficients[1:] *= np.sqrt(h_indices*(h_indices + 1))


class FFTError(Exception):
    pass


class FFT:
    def __init__(self, read_audio: audio.ReadFn):
        self.read_audio = read_audio
        self.fft_in = pyfftw.zeros_aligned(shape=params.n_fft_in, dtype=np.float32)
        self.fft_out = pyfftw.empty_aligned(shape=params.n_fft_out, dtype=np.complex64)

        self.fft: FFTW
        self.plan_fft()

        self.cents: List[np.ndarray]
        self.harmonics: List[np.ndarray]

    def plan_fft(self):
        start = time.monotonic()
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

        def make_fft() -> FFTW:
            flags = ('FFTW_MEASURE',)

            if has_wisdom:
                flags += ('FFTW_WISDOM_ONLY',)
            else:
                print(f'Planning FFT wisdom on {n_cpus} cpus...', end=' ')

            return pyfftw.FFTW(
                self.fft_in, self.fft_out,
                direction='FFTW_FORWARD',
                flags=flags,
                threads=n_cpus,
                planning_timelimit=60,
            )

        try:
            self.fft: FFTW = make_fft()
        except RuntimeError as e:
            if 'No FFTW wisdom is known for this plan' in e.args[0]:
                print(str(e))
                has_wisdom = False
                self.fft: FFTW = make_fft()
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
        print(f'{n_codelets} codelets in {end - start:.1f}s')

    def set_note(self, note: int):
        f_tune_exact = n_to_f(note)

        bounds_flat = np.empty(params.n_harmonics + 1, dtype=np.uint32)
        np.rint(f_tune_exact * coefficients, casting='unsafe', out=bounds_flat)
        bounds = np.vstack((bounds_flat[:-1], bounds_flat[1:])).T
        sizes = (bounds[:, 1] - bounds[:, 0])[..., np.newaxis]
        longest = np.max(sizes)

        cents = np.linspace(bounds[:, 0], bounds[:, 0] + longest - 1, longest).T
        cents *= (params.f_upper / f_tune_exact / params.n_fft_out / h_indices)[..., np.newaxis]
        cents = 1_200 / LOG_2 * np.log(cents)

        # This can't really be vectorized because these will be jagged.
        self.cents = [
            cent[:size[0]]
            for cent, size in zip(cents, sizes)
        ]

        self.harmonics = [
            self.fft_out[left: right]
            for left, right in bounds
        ]

    def get_spectrum(self) -> AxisPair:
        # Read up to n_fft_in samples; usually it will be much smaller
        samples = self.read_audio(params.n_fft_in)

        n = len(samples)
        if n:
            # Shift the existing data left within the same array
            self.fft_in[:-n] = self.fft_in[n:]
            # Copy new data into the end of the array
            self.fft_in[-n:] = samples
            self.fft()

        harms = []
        for harm in self.harmonics:
            harm = np.abs(harm)
            yfmax = np.max(harm)
            if yfmax > params.y_max:
                harm *= params.y_max / yfmax
            harms.append(harm)

        return self.cents, harms
