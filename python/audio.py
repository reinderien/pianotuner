import numpy as np
import pyaudio
from contextlib import contextmanager
import params
import typing

if typing.TYPE_CHECKING:
    SingleArray = np.ndarray[typing.Any, np.dtype[np.float32]]

    class DeviceDict(typing.TypedDict):
        index: int
        structVersion: int
        name: str
        hostApi: int
        maxInputChannels: int
        maxOutputChannels: int
        defaultLowInputLatency: float
        defaultLowOutputLatency: float
        defaultHighInputLatency: float
        defaultHighOutputLatency: float
        defaultSampleRate: float

    class ReadFn(typing.Protocol):
        def __call__(self, n: int) -> SingleArray:
            ...


@contextmanager
def init_audio() -> 'typing.Iterator[ReadFn]':
    audio = pyaudio.PyAudio()
    stream: pyaudio.Stream

    def read(n: int) -> 'SingleArray':
        n_avail: int = stream.get_read_available()
        n_read = min(n, n_avail)
        samples: bytes = stream.read(n_read)
        return np.frombuffer(samples, dtype=np.float32)

    try:
        print('Selecting default audio device...', end=' ')
        device = typing.cast('DeviceDict', audio.get_default_input_device_info())
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
