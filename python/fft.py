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
        self.bounds: List[Tuple[int, int]]

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
            flags = {'FFTW_MEASURE'}

            if not has_wisdom:
                print(f'Planning FFT wisdom on {n_cpus} cpus...', end=' ')
                flags.add('FFTW_WISDOM_ONLY')

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
        # todo - variable.
        f_tune_exact = n_to_f(note)

        # This can't really be vectorized because these will be jagged.
        self.cents = []
        self.bounds = []
        for h in range(1, params.n_harmonics + 1):
            f_left, f_right = h * f_tune_exact/SQ2, h * f_tune_exact*SQ2
            i_left, i_right = f_to_fft(f_left), f_to_fft(f_right)
            self.bounds.append((i_left, i_right))

            fft_indices = np.arange(i_left, i_right + 1)
            freqs = fft_to_f(fft_indices)
            self.cents.append(1_200 * np.log(freqs / f_tune_exact/h) / LOG_2)

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
        for left, right in self.bounds:
            harm = np.abs(self.fft_out[left: right+1])
            yfmax = np.max(harm)
            if yfmax > params.y_max:
                harm *= params.y_max / yfmax
            harms.append(harm)

        return self.cents, harms
