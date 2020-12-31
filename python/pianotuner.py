#!/usr/bin/env python3
import math
import matplotlib
import numpy as np
import pyaudio
import scipy.interpolate
from contextlib import contextmanager
from pprint import pprint
from typing import Callable, Dict, Literal, Optional, Tuple, Union

import pyfftw
from matplotlib import pyplot as plt
from pyfftw.pyfftw import FFTW

RATE = 44_100

DeviceDict = Dict[str, Union[str, int, float]]


def get_fund_index(spectrum: np.ndarray) -> int:
    mags = np.abs(spectrum)
    try:
        first_i = next(i for i, v in enumerate(mags) if v >= 100)
    except StopIteration:
        first_i = np.argmax(mags)
        print(f'Nothing above |100|; using index {first_i}')
        return first_i

    # Look for the largest component between here and the approximate midpoint to the
    # next harmonic
    best_i = first_i + np.argmax(mags[first_i: int(first_i * 1.5)])
    print(f'Best in [{first_i}, {first_i*1.5}] is {best_i}')
    return best_i


def init_plot(t_window: int) -> Tuple[
    Callable[[], None],  # plot_loop
    Callable[[np.ndarray], None],  # plot
]:
    print('Initializing plot...')
    fig, (ax1, ax2) = plt.subplots(ncols=2, sharey=True)

    ax1.set_title('Broad Spectrum')
    ax1.set_xlabel('Frequency (Hz)')
    ax1.set_ylabel('Spectral Magnitude')
    broad_line, = ax1.loglog([], [])

    ax2.set_title('Tuning Spectrum')
    ax2.set_xlabel('Tune (cents)')
    tune_line, = ax2.semilogy([], [])

    for ax in (ax1, ax2):
        ax.grid(which='major', axis='both', linestyle='-')
        ax.grid(which='minor', axis='both')

    ax1.set_xticks(minor=False, ticks=[55 * pow(2, o) for o in range(-2, 8)])
    ax1.set_xticks(minor=True, ticks=[55 * pow(2, o + i / 12) for o in range(-2, 8) for i in range(1, 12)])
    ax1.get_xaxis().set_major_formatter(matplotlib.ticker.ScalarFormatter())
    ax1.set_xlim(xmin=55 * pow(2, -2), xmax=55 * pow(2, 8))
    ax1.set_ylim(ymin=1e-3, ymax=1e4)

    ax2.set_xlim(xmin=-100, xmax=100)

    def plot_loop():
        plt.show(block=True)

    def plot(spectrum: np.ndarray):
        broad_x = [i / t_window for i in range(len(spectrum))]
        broad_y = np.abs(spectrum)
        broad_x_new = [55 * pow(2, o + i / 48) for o in range(-2, 8) for i in range(1, 48)]
        broad_y_new = scipy.interpolate.interp1d(broad_x, broad_y)(broad_x_new)
        broad_line.set_xdata(broad_x_new)
        broad_line.set_ydata(broad_y_new)

        fund_i = get_fund_index(spectrum)
        fund_f = fund_i / t_window
        fund_n = f_to_n(fund_f)
        fund_n_goal = round(fund_n)
        print(f'fund i={fund_i} f={fund_f:.1f} n={fund_n:.1f} -> {fund_n_goal}')
        lower_n, higher_n = fund_n_goal - 1, fund_n_goal + 1
        lower_f, higher_f = n_to_f(lower_n), n_to_f(higher_n)
        lower_i, higher_i = int(math.floor(lower_f * t_window)), int(math.ceil(higher_f * t_window))
        tune_x = [100*(f_to_n(i / t_window) - fund_n_goal)
                  for i in range(lower_i, higher_i)]
        tune_y = broad_y[lower_i: higher_i]
        print(
            f'n {lower_n}-{higher_n} '
            f'f {lower_f:.1f}-{higher_f:.1f} '
            f'i {lower_i}-{higher_i} '
            f'tune-x {tune_x[0]:.1f}-{tune_x[-1]:.1f}'
        )
        tune_x_new = list(range(-100, 101))
        tune_y_new = scipy.interpolate.interp1d(tune_x, tune_y, bounds_error=False)(tune_x_new)
        tune_line.set_xdata(tune_x_new)
        tune_line.set_ydata(tune_y_new)
        ax2.set_xlabel(
            f'Tune (cents) from {n_to_name(fund_n_goal)}'
            f'={n_to_f(fund_n_goal)}Hz'
        )

        print('Drawing...')
        fig.canvas.draw()
        fig.canvas.flush_events()
        print('Done drawing.')

    return plot_loop, plot


def n_to_f(n: float) -> float:
    return 55 * 2**((n - 21)/12)


def f_to_n(f: float) -> float:
    return 12 * math.log(f / 55)/math.log(2) + 21


# https://en.wikipedia.org/wiki/Piano#/media/File:Piano_Frequencies.svg
NAMES = ('C', 'C#', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B')


def n_to_name(n: float) -> str:
    n = int(math.floor(n + 0.5))
    return NAMES[n % 12] + str(int(n / 12))


def get_note(spectrum: np.ndarray, t_window: float) -> Optional[Tuple[
    float,  # freq
    float,  # note index
    str,    # note name
    float,  # fundamental
]]:
    try:
        i, v = next((i, v) for i, v in enumerate(np.abs(spectrum)) if v >= 100)
    except StopIteration:
        return None
    f = i / t_window
    n = f_to_n(f)
    name = n_to_name(n)
    return f, n, name, v


CallbackResult = Tuple[
    Optional[bytes],
    Literal[pyaudio.paContinue, pyaudio.paComplete],
]
AudioCallback = Callable[
    [
        bytes,  # in_data
        int,    # frame_count
        dict,   # time_info
        int,    # status_flags
    ],
    CallbackResult
]


def audio_cb(
    fftw: FFTW,
    t_window: float,
    plot: Callable[[np.ndarray], None],
) -> AudioCallback:

    def cb(in_data: bytes, frame_count: int, time_info: dict, status_flags: int) -> Tuple[
        Optional[bytes],
        Literal[pyaudio.paContinue, pyaudio.paComplete],
    ]:
        print(
            f'{len(in_data)} elements of {type(in_data[0])}, '
            f'{frame_count} samples, '
            f'status {status_flags}'
        )
        in_data = pyfftw.byte_align(
            array=np.frombuffer(in_data, dtype=np.float32)
        )
        print(f'Aligned to {in_data.shape} {in_data.dtype}, '
              f'in [{min(in_data):.2f} {max(in_data):.2f}]')

        fft_res: np.ndarray = fftw(input_array=in_data)
        note = get_note(fft_res, t_window)
        if note:
            f, n, name, v = note
            print(f'Fundamental |{v:.1f}| at {f:.1f} Hz, n={n:.1f} "{name}"')

        try:
            plot(fft_res)
            flag = pyaudio.paContinue
        except RuntimeError as e:
            print(e)
            flag = pyaudio.paComplete

        out_data = None
        return out_data, flag

    return cb


@contextmanager
def init_audio(n_frames: int, cb: Callable):
    print('Initializing audio...')
    audio = pyaudio.PyAudio()

    try:
        print('Selecting default audio device...', end=' ')
        device = audio.get_default_input_device_info()
        print(f"#{device['index']} @ {RATE}")

        print('Opening capture stream...')
        stream = audio.open(
            input_device_index=device['index'],
            input=True,
            rate=RATE,
            channels=1,
            format=pyaudio.paFloat32,
            frames_per_buffer=n_frames,
            stream_callback=cb,
        )
        print()

        try:
            yield
        finally:
            print('Closing audio...')
            stream.stop_stream()
            stream.close()
    finally:
        audio.terminate()


def plan_window() -> Tuple[
    int,    # samples
    float,  # window duration
]:
    t_window_approx = 1
    n_frames = 2**int(math.log(RATE / t_window_approx) / math.log(2))
    t_window = n_frames / RATE
    return n_frames, t_window


def init_fft(n_frames: int) -> FFTW:
    print('Initializing FFT...')
    fft_in = pyfftw.empty_aligned(shape=n_frames, dtype='float32')
    fft_out = pyfftw.empty_aligned(shape=n_frames // 2 + 1, dtype='complex64')
    return pyfftw.FFTW(
        fft_in, fft_out,
        direction='FFTW_FORWARD',
        flags=('FFTW_MEASURE', 'FFTW_DESTROY_INPUT'),
        threads=4,
        planning_timelimit=2,
    )


def main():
    n_frames, t_window = plan_window()
    fftw = init_fft(n_frames)
    plot_loop, plot = init_plot(t_window)

    with init_audio(n_frames, audio_cb(fftw, t_window, plot)):
        plot_loop()


main()
print('Complete.')
