#!/usr/bin/env python3
from math import sqrt
from typing import Callable, Dict, Iterable, List

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.animation import FuncAnimation
from matplotlib.artist import Artist
from matplotlib.axes import Axes
from matplotlib.backend_bases import KeyEvent
from matplotlib.figure import Figure
from matplotlib.lines import Line2D
from matplotlib.scale import ScaleBase, register_scale
from matplotlib.transforms import Transform

import params
from fft import SpectrumFn
from params import n_to_name


ChangeNoteFn = Callable[[int], None]

KEYS: Dict[str, int] = {
    'left': -1,
    'right': 1,
    'down': -12,
    'up': 12,
}


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


class Plot:
    def __init__(
        self,
        get_spectrum: SpectrumFn,
        change_note: ChangeNoteFn,
    ):
        self.get_spectrum = get_spectrum
        self.change_note = change_note
        self.run: Callable[[], None] = plt.show

        ticks = [10, 25, 50, 100, 200, 600]
        ticks = [
            *(-x for x in ticks[::-1]),
            0, *ticks,
        ]

        fig: Figure
        ax: Axes
        self.fig, ax = plt.subplots()
        self.ax = ax

        self.plots: List[Line2D] = [
            ax.plot([], [], label=str(harm))[0]
            for harm in range(1, params.n_harmonics + 1)
        ]

        ax.grid()
        ax.legend(title='Harmonic')

        ax.set_xlabel('Deviation, cents')
        ax.set_xlim(-600, 600)
        ax.set_xscale('tune-scale')
        ax.set_xticks(ticks, minor=False)
        ax.tick_params(axis='x', which='both', labelrotation=45)

        ax.set_ylabel('Spectral power')
        ax.set_ylim(0, params.y_max)

        self.fig.canvas.mpl_connect('key_press_event', self.on_key)

        self.animation = FuncAnimation(
            self.fig, self.animate,
            interval=1_000 // params.framerate,
            blit=True,
        )

    def animate(self, frame: int) -> Iterable[Artist]:
        freqs, powers = self.get_spectrum()

        for freq_axis, power_data, plot in zip(freqs, powers, self.plots):
            plot.set_data(freq_axis, power_data)

        return self.plots

    def on_key(self, event: KeyEvent):
        delta = KEYS.get(event.key)
        if delta is not None:
            self.change_note(delta)

    def set_note(self, note: int):
        name = n_to_name(note)
        self.ax.set_title(f'Harmonic spectrum at {name}')
        self.fig.canvas.draw()
