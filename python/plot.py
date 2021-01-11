#!/usr/bin/env python3
from math import sqrt
from typing import Iterable, Callable, Tuple

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.animation import FuncAnimation
from matplotlib.artist import Artist
from matplotlib.axes import Axes
from matplotlib.figure import Figure
from matplotlib.lines import Line2D
from matplotlib.scale import ScaleBase, register_scale
from matplotlib.transforms import Transform

import params
from fft import SpectrumFn


class TuneScale(ScaleBase):
    """
    Zoom in on 0 using a piecewise antisymmetric hyperbolic non-affine
    transform, preserving equivalence at -600, 0 and 600

    y = 1/(b - ax) - c
    """

    name = 'tune-scale'

    # Stretch parameter, higher means more zoom
    a = 4e-5
    b = 300*a + sqrt(9e4*a**2 + a)

    def __init__(self, axis, **kwargs):
        super().__init__(axis, **kwargs)

    def set_default_locators_and_formatters(self, axis):
        pass

    def get_transform(self) -> Transform:
        return self.Inverse()

    class Forward(Transform):
        input_dims = 1
        output_dims = 1
        is_separable = True

        def __init__(self):
            super().__init__(shorthand_name='forward-tune-transform')

        def transform_non_affine(self, x: np.ndarray) -> np.ndarray:
            a, b = TuneScale.a, TuneScale.b
            y = np.sign(x) * (1/(b - a*np.abs(x)) - 1/b)
            return y

        def inverted(self) -> Transform:
            return TuneScale.Inverse()

    class Inverse(Transform):
        input_dims = 1
        output_dims = 1
        is_separable = True

        def __init__(self):
            super().__init__(shorthand_name='inverse-tune-transform')

        def transform_non_affine(self, y: np.ndarray) -> np.ndarray:
            a, b = TuneScale.a, TuneScale.b
            x = np.sign(y)*(b - 1/(np.abs(y) + 1/b))/a
            return x

        def inverted(self) -> Transform:
            return TuneScale.Forward()


register_scale(TuneScale)


def animate(frame: int, plot: Line2D, get_spectrum: SpectrumFn) -> Iterable[Artist]:
    freqs, powers = get_spectrum()
    plot.set_data(freqs, powers)
    return plot,


def init_plot(get_spectrum: SpectrumFn) -> Tuple[
    Callable[[], None],  # plot loop
    FuncAnimation,
]:
    ticks = [10, 25, 50, 100, 250, 600]
    ticks = [
        *(-x for x in ticks[::-1]),
        0, *ticks,
    ]

    fig: Figure
    ax: Axes
    fig, ax = plt.subplots()

    ax.set_title('Harmonic spectrogram')

    ax.set_ylabel('Spectral power')
    ax.set_ylim(0, 500)

    ax.set_xlabel('Deviation, cents')
    ax.set_xlim(-600, 600)
    ax.set_xscale('tune-scale')
    ax.set_xticks(ticks, minor=False)
    ax.tick_params(axis='x', which='both', labelrotation=45)

    ax.grid()

    plot, = ax.plot([], [], )

    animation = FuncAnimation(
        fig, animate,
        fargs=(plot, get_spectrum),
        interval=1_000 // params.framerate,
        blit=True,
    )

    return plt.show, animation
