import numpy as np

# https://en.wikipedia.org/wiki/Piano_key_frequencies
NAMES = ('C', 'C♯', 'D', 'E♭', 'E', 'F', 'F♯', 'G', 'A♭', 'A', 'B♭', 'B')

LOG_2: float = np.log(2)
SQ2: float = np.sqrt(2)


def prev_pow_2(x: float) -> int:
    exp = int(np.log(x) / LOG_2)
    power: int = 2**exp
    return power


def next_pow_2(x: float) -> int:
    exp = int(np.ceil(np.log(x) / LOG_2))
    power: int = 2**exp
    return power


n_notes = 88         # (semitones) number of piano notes
n_a440 = 12*4        # (semitones) offset from A0 in semitones
f_a0 = f_min = 27.5  # (cycles/sec) frequency of A0
f_samp = 48_000      # (samples/sec) sampling frequency
t_window_min = 1.    # (seconds) minimum capture window duration
framerate_min = 30.  # (frames/sec) minimum animation framerate
y_max = 50           # post-FFT audio y-units

f_upper = 0.5*f_samp  # (cycles/sec) maximum detectable frequency
samp_min = t_window_min*f_samp  # (samples/cycle) minimum samples per window
n_window_samples = next_pow_2(samp_min)  # (samples/cycle) samples per window >= samp_min
t_window = n_window_samples/f_samp     # (secs/cycle) capture window duration >= t_window_min
f_lower = 1./t_window                  # (cycles/sec) minimum detectable frequency
frame_samples_max = f_samp / framerate_min       # (samples/frame) max samples per animation frame
n_frame_samples = prev_pow_2(frame_samples_max)  # (samples/frame) samples per animation frame >= frame_samples_max
framerate = f_samp / n_frame_samples   # (frames/sec) animation framerate


def n_to_f(note: int) -> float:
    exp: float = np.power(2, note/12)
    return f_a0 * exp


def f_to_n(freq: float) -> float:
    rel: float = np.log(freq/f_a0)
    return 12 * rel / LOG_2


def n_to_name(n: float) -> str:
    # In application note space, A0 maps to index 0, but in musical note space
    # C is at 0
    n = round(n) + 9
    octave, semi = divmod(n, 12)
    return f'{NAMES[semi]}{octave}'


f_max = n_to_f(n_notes - 1)    # (cycles/sec) maximum piano frequency
f_min_tune = n_to_f(-1)        # (cycles/sec) frequency of one below lowest piano note
f_max_tune = n_to_f(n_notes)   # (cycles/sec) frequency of one above highest piano note
t_min = 1 / f_min_tune         # (secs/cycle)  cycle time for one below lowest piano note
samp_min = t_min*f_samp        # (samples/cycle) recalculate min window sample count
lower_index = f_to_n(f_lower)  # (semitones) lowest detectable note index
n_worst = f_to_n(f_min + f_lower)  # (semitones) worst-case note resolution


def dump(verbose: bool = False) -> None:
    print('Audio parameters:')
    print(f't_window = {t_window:.2f}s')
    print(f'f_samp = {f_samp/1e3:.1f} kHz')
    print(f'framerate = {framerate:.1f} Hz')

    if verbose:
        print(f't_min = {t_min*1e3:.1f}ms')
        print(f'Min samples = {samp_min:.0f}')
        print(f'Act samples = {n_window_samples}')
        print(f'Min detectable index = {lower_index:.1f} semitones')
        print(f'Min detectable freq = {f_lower:.3f} Hz '
              f'(also spectral resolution)')
        print(f'Worst-case fundamental note resolution: {n_worst:.2f}')
        print(f'f_mintune = {f_min_tune:.3f} Hz')
        print(f'f_min = {f_min:.3f} Hz')
        print(f'f_max = {f_max/1e3:.3f} kHz')
        print(f'f_maxtune = {f_max_tune/1e3:.3f} kHz')
        print(f'Max detectable freq = {f_upper/1e3} kHz')
        print(f'Max detectable harmonic = {f_upper/f_max_tune:.1f}')

    print()
