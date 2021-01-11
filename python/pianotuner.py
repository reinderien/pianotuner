#!/usr/bin/env python3

from audio import init_audio
from fft import FFT
from plot import Plot
import params


def main():
    params.dump()

    note = params.n_a440

    def change_note(delta: int):
        nonlocal note
        note += delta
        fft.set_note(note)
        plot.set_note(note)

    with init_audio() as read_audio:
        fft = FFT(read_audio)
        plot = Plot(fft.get_spectrum, change_note)
        change_note(0)
        plot.run()


if __name__ == '__main__':
    main()
