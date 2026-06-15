import typing

import numpy as np
import scipy

import audio
import params


if typing.TYPE_CHECKING:
    AxisPair = tuple[
        audio.SingleArray,  # freq (horizontal) axes
        audio.SingleArray,  # power (vertical) axes
    ]


class Spectrum:
    def __init__(self, read_audio: 'audio.ReadFn') -> None:
        self.read_audio = read_audio
        self.audio_in = np.zeros(shape=params.n_window_samples, dtype=np.float32)
        self.spec_out: audio.SingleArray = np.empty(shape=0, dtype=np.float32)
        self.freq_out: audio.SingleArray = self.spec_out.copy()
        self.set_note(params.n_a440)

    def set_note(self, note: int) -> None:
        self.f_tune_exact = params.n_to_f(note)
        fcentre = 2*self.f_tune_exact/params.f_samp
        bandwidth_n = 2  # semitones
        factor = 2**(bandwidth_n/12)  # frequency factor, unitless
        flo = fcentre / factor
        fhi = fcentre * factor
        self.filt_b, self.filt_a = scipy.signal.butter(N=3, Wn=(flo, fhi), btype='bandpass')

    def zerocross(self) -> 'AxisPair':
        """
        Test case:
        fsamp = 48000
        one sample = 1/48000 = 21 us
        1/440 Hz = 2.3 ms
        2.3 ms/cycle / 21 us/sample = 109 samples/cycle
        """
        # period = params.f_samp/440
        # self.audio_in[:] = 0.01*np.sin(
        #     np.arange(self.audio_in.size)/period * 2*np.pi
        # )

        # lopass = self.audio_in - self.audio_in.mean()
        lopass = scipy.signal.lfilter(self.filt_b, self.filt_a, self.audio_in)

        signs = np.sign(lopass)
        signs = signs[signs != 0]
        i_zc = np.flatnonzero(np.diff(signs))
        if i_zc.size < 2:
            empty = np.empty(shape=0, dtype=np.float32)
            return empty, empty

        y0 = lopass[i_zc]
        y1 = lopass[i_zc + 1]
        i_zc_refined = y0/(y0 - y1) + i_zc
        freqs = params.f_upper/(np.diff(i_zc_refined))

        cents = 1200/params.LOG_2 * np.log(freqs/self.f_tune_exact)
        mask = (cents > -600) & (cents < 600)
        # print(f'{cents.min():.1f} < {cents.mean():.1f} < {cents.max():.1f}, ', end='')
        cents = cents[mask]
        if cents.size < 1:
            # print()
            empty = np.empty(shape=0, dtype=np.float32)
            return empty, empty

        powers = np.add.reduceat(np.abs(lopass), i_zc)[:-1]
        powers = powers[mask]
        pmax = powers.max()
        # print(f'p={pmax:.3f}')
        if pmax > params.y_max:
            powers *= params.y_max/pmax

        return cents, powers

    def get_spectrum(self) -> 'AxisPair':
        # Read up to n_window_samples; usually it will be much smaller
        samples = self.read_audio(params.n_window_samples)

        n = len(samples)
        if n:
            # Shift the existing data left within the same array
            self.audio_in[:-n] = self.audio_in[n:]
            # Copy new data into the end of the array
            self.audio_in[-n:] = samples
            self.freq_out, self.spec_out = self.zerocross()

        return self.freq_out, self.spec_out
