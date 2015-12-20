#!/usr/bin/env python3


import math, matplotlib, numpy, pprint, pyaudio, pyfftw, scipy.interpolate, time
from matplotlib import pyplot as plt


def get_best_device(audio):
    standard_rates = (192000, 96000, 88200, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 9600, 8000)
    default_device = audio.get_default_input_device_info()
    best_rate, best_device = 0, default_device
    for device_index in range(audio.get_device_count()):
        device = audio.get_device_info_by_index(device_index)
        if device['maxInputChannels'] < 1:
            continue
        device.update({'supported_rates': [],
                       'is_default': device['index'] == default_device['index']})
        for rate in standard_rates:
            try:
                if audio.is_format_supported(rate=rate, input_channels=1, input_format=pyaudio.paFloat32,
                                             input_device=device['index']):
                    device['supported_rates'].append(rate)
                    if rate > best_rate or (rate == best_rate and device['is_default']):
                        best_rate, best_device = rate, device
            except ValueError:
                pass

    print('Best device:\n%s' % pprint.pformat(best_device))
    print('Best rate: %d Hz' % best_rate)
    return best_device, best_rate


def plot(spectrum, t_window):
    dx = [i / t_window for i in range(len(spectrum))]
    dy = numpy.abs(spectrum)
    dxnew = [55 * pow(2, o + i / 48) for o in range(-2, 8) for i in range(1, 48)]
    dynew = scipy.interpolate.interp1d(dx, dy)(dxnew)

    fig, ax = plt.subplots(1, 1)
    ax.set_title('Magnitude Spectrum')
    ax.loglog(dxnew, dynew)
    ax.grid(which='major', axis='both', linestyle='-')
    ax.grid(which='minor', axis='both')
    ax.set_autoscaley_on(True)
    ax.set_xticks(minor=False, ticks=[55 * pow(2, o) for o in range(-2, 8)])
    ax.set_xticks(minor=True, ticks=[55 * pow(2, o + i / 12) for o in range(-2, 8) for i in range(1, 12)])
    ax.get_xaxis().set_major_formatter(matplotlib.ticker.ScalarFormatter())
    ax.set_xlim(xmin=55 * pow(2, -2), xmax=55 * pow(2, 8))

    plt.show()


def get_note_name(f):
    # https://en.wikipedia.org/wiki/Piano#/media/File:Piano_Frequencies.svg
    names = ('C', 'C#', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B')
    n = math.floor(12 * math.log(f / 55) / math.log(2) + 0.5) + 21
    name = names[n % 12] + str(int(n / 12))
    return n, name


def get_note(spectrum, t_window):
    try:
        i, v = next((i, v) for i, v in enumerate(numpy.abs(spectrum)) if v >= 100)
    except StopIteration:
        return None
    f = i / t_window
    n, name = get_note_name(f)
    return f, n, name, v


def audio_cb(fftw, t_window):
    def inner(in_data, frame_count, time_info, status_flags):
        in_data = pyfftw.n_byte_align(n=16, array=numpy.fromstring(in_data, dtype='float32'))
        print('%d samples in [%f %f]; status %s' %
              (frame_count, min(in_data), max(in_data), status_flags))

        fft_res = fftw(input_array=in_data)
        note = get_note(fft_res, t_window)
        if note:
            f, n, name, v = note
            print('Fundamental |%.1f| at %.1f Hz, n=%d "%s"' % (v, f, n, name))
            flag = pyaudio.paComplete
        else:
            flag = pyaudio.paContinue

        out_data = None
        return out_data, flag
    return inner


def capture():
    print('Initializing audio...')
    audio = pyaudio.PyAudio()
    try:
        print('Selecting audio device...')
        best_device, best_rate = get_best_device(audio)
        t_window = 1
        n_frames = int(t_window * best_rate)

        print('Initializing FFT...')
        fft_in = pyfftw.n_byte_align_empty(n=16, shape=n_frames, dtype='float32')
        fft_out = pyfftw.n_byte_align_empty(n=16, shape=n_frames // 2 + 1, dtype='complex64')
        fftw = pyfftw.FFTW(fft_in, fft_out,
                           direction='FFTW_FORWARD',
                           flags=('FFTW_MEASURE', 'FFTW_DESTROY_INPUT'),
                           threads=4, planning_timelimit=2)
        print('Opening capture stream...')
        stream = audio.open(rate=best_rate, channels=1, format=pyaudio.paFloat32,
                            input=True, input_device_index=best_device['index'],
                            frames_per_buffer=n_frames, stream_callback=audio_cb(fftw, t_window))
        try:
            while stream.is_active() and not stream.is_stopped():
                time.sleep(1)
        finally:
            print('Closing audio...')
            stream.stop_stream()
            stream.close()
    finally:
        audio.terminate()

    print('Plotting...')
    plot(fft_out, t_window)


capture()
print('Complete.')
