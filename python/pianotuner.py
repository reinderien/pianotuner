#!/usr/bin/env python3

from audio import init_audio
from fft import init_fft
from plot import init_plot
import params


def main():
    params.dump()

    with init_audio() as read_audio:
        get_spectrum = init_fft(read_audio)
        plot_loop, animation = init_plot(get_spectrum)
        plot_loop()


if __name__ == '__main__':
    main()
