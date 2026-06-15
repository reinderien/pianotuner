#!/usr/bin/env python3

import audio
import params
import plot
import spectrum


def main() -> None:
    params.dump()

    note = params.n_a440

    def change_note(delta: int) -> None:
        nonlocal note
        new_note = note + delta
        if new_note < params.n_notes:
            note = new_note
        elif delta > 1:
            note = params.n_notes - 1
        else:
            return

        spect.set_note(note)
        plotter.set_note(note)

    with audio.init_audio() as read_audio:
        spect = spectrum.Spectrum(read_audio)
        plotter = plot.Plot(spect.get_spectrum, change_note)
        change_note(+0)
        plotter.run()


if __name__ == '__main__':
    main()
