import numpy as np
import pyaudio
from contextlib import contextmanager
from typing import Dict, Union, Callable
import params

DeviceDict = Dict[str, Union[str, int, float]]
ReadFn = Callable[[int], np.ndarray]


@contextmanager
def init_audio():
    audio = pyaudio.PyAudio()
    stream: pyaudio.Stream

    def read(n: int) -> np.ndarray:
        n_avail: int = stream.get_read_available()
        n_read = min(n, n_avail)
        samples: bytes = stream.read(n_read)
        return np.frombuffer(samples, dtype=np.float32)

    try:
        print('Selecting default audio device...', end=' ')
        device: DeviceDict = audio.get_default_input_device_info()
        print(f"#{device['index']}", end='')
        name = device.get('name')
        if name is not None:
            print(f': {name}')
        else:
            print()

        stream = audio.open(
            input_device_index=device['index'],
            input=True,
            rate=params.f_samp,
            channels=1,
            format=pyaudio.paFloat32,
            frames_per_buffer=2*params.n_frame_samples,
        )

        try:
            yield read
        finally:
            print('Closing audio...')
            stream.stop_stream()
            stream.close()
    finally:
        audio.terminate()
