import typing

import numpy as np

import audio
import params


if typing.TYPE_CHECKING:
    AxisPair = tuple[
        audio.SingleArray,  # freq (horizontal) axes
        audio.SingleArray,  # power (vertical) axes
    ]


class SpectralError(Exception):
    pass


class Spectrum:
    def __init__(self, read_audio: 'audio.ReadFn') -> None:
        self.read_audio = read_audio
        self.audio_in = np.zeros(shape=params.n_window_samples, dtype=np.float32)
        self.spec_out = np.empty(shape=0, dtype=np.float32)
        self.cents: audio.SingleArray = self.spec_out.copy()

    def set_note(self, note: int) -> None:
        f_tune_exact = params.n_to_f(note)

        bounds_flat = np.empty(params.n_harmonics + 1, dtype=np.uint32)
        np.rint(f_tune_exact * coefficients, casting='unsafe', out=bounds_flat)
        bounds = np.vstack((bounds_flat[:-1], bounds_flat[1:])).T
        sizes = (bounds[:, 1] - bounds[:, 0])[..., np.newaxis]
        longest = np.max(sizes)

        cents = np.linspace(bounds[:, 0], bounds[:, 0] + longest - 1, longest).T
        cents *= (params.f_upper / f_tune_exact / params.n_fft_out / h_indices)[..., np.newaxis]
        cents = 1_200 / params.LOG_2 * np.log(cents)

        # This can't really be vectorized because these will be jagged.
        self.cents = [
            cent[:size[0]]
            for cent, size in zip(cents, sizes)
        ]

        self.harmonics = [
            self.fft_out[left: right]
            for left, right in bounds
        ]

    def zerocross(self) -> 'audio.SingleArray':
        return np.zeros(20)
        '''
        
        harm = np.abs(harm)
        yfmax = np.max(harm)
        if yfmax > params.y_max:
            harm *= params.y_max / yfmax
        '''

    def get_spectrum(self) -> 'AxisPair':
        # Read up to n_window_samples; usually it will be much smaller
        samples = self.read_audio(params.n_window_samples)

        n = len(samples)
        if n:
            # Shift the existing data left within the same array
            self.audio_in[:-n] = self.audio_in[n:]
            # Copy new data into the end of the array
            self.audio_in[-n:] = samples
            self.spec_out = self.zerocross()

        return self.cents, self.spec_out
