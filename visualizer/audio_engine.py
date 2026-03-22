"""
Audio engine for capturing and processing audio data.
Supports microphone input AND system/loopback audio capture.
"""

import numpy as np
import sounddevice as sd
from scipy.fft import rfft, rfftfreq
from typing import Optional, List
import threading
import sys


class AudioEngine:
    """Handles audio capture and FFT processing."""

    def __init__(self, settings):
        self.settings = settings
        self.sample_rate = settings.sample_rate
        self.chunk_size = settings.chunk_size
        self.stream: Optional[sd.InputStream] = None
        self.audio_data = np.zeros(self.chunk_size)
        self.fft_data = np.zeros(self.chunk_size // 2)
        self.smoothed_fft = np.zeros(self.chunk_size // 2)
        self.lock = threading.Lock()
        self._running = False
        self.peak_level = 0.0
        self.rms_level = 0.0
        self._current_device = settings.device_index
        self._device_cache: Optional[List[dict]] = None

    def _audio_callback(self, indata, frames, time_info, status):
        """Callback for audio stream — runs in a separate thread."""
        with self.lock:
            if indata.shape[1] > 1:
                # Mix stereo to mono
                self.audio_data = np.mean(indata, axis=1).copy()
            else:
                self.audio_data = indata[:, 0].copy()

    def start(self, device_index: int = None):
        """Start audio capture."""
        self.stop()
        self.sample_rate = 44100

        if device_index is not None:
            self.settings.device_index = device_index

        device = None if self.settings.device_index == -1 else self.settings.device_index
        self._current_device = self.settings.device_index

        try:
            channels = 1
            self.sample_rate = self.settings.sample_rate

            if device is not None:
                try:
                    dev_info = sd.query_devices(device)
                    native_sr = int(dev_info['default_samplerate'])
                    self.sample_rate = native_sr
                    # Use up to 2 channels for loopback/stereo devices
                    max_ch = dev_info.get('max_input_channels', 1)
                    channels = min(2, max(1, max_ch))
                except Exception:
                    pass

            self.stream = sd.InputStream(
                samplerate=self.sample_rate,
                blocksize=self.chunk_size,
                device=device,
                channels=channels,
                dtype='float32',
                callback=self._audio_callback,
            )
            self.stream.start()
            self._running = True

            device_name = "System Default"
            if device is not None:
                try:
                    device_name = sd.query_devices(device)['name']
                except Exception:
                    device_name = f"Device {device}"

            print(f"Audio started: {device_name} @ {self.sample_rate}Hz, {channels}ch")

        except Exception as e:
            print(f"Error starting audio on device {device}: {e}")
            self._running = False
            if device is not None:
                print("Falling back to default device...")
                self.settings.device_index = -1
                self.start()

    def stop(self):
        """Stop audio capture."""
        self._running = False
        if self.stream:
            try:
                self.stream.stop()
                self.stream.close()
            except Exception:
                pass
            self.stream = None

    def restart_with_device(self, device_index: int):
        """Restart the audio engine with a different input device."""
        print(f"Switching audio device to index: {device_index}")
        self.start(device_index)

    def get_frequency_bands(self, num_bands: int) -> np.ndarray:
        """Process audio data and return frequency band magnitudes."""
        with self.lock:
            data = self.audio_data.copy()

        window = np.hanning(len(data))
        windowed = data * window

        fft_result = rfft(windowed)
        fft_magnitude = np.abs(fft_result) / len(data)

        freqs = rfftfreq(len(data), 1.0 / self.sample_rate)

        min_freq = max(self.settings.min_frequency, 20)
        max_freq = min(self.settings.max_frequency, self.sample_rate // 2)

        freq_bands = np.logspace(
            np.log10(min_freq),
            np.log10(max_freq),
            num_bands + 1
        )

        band_magnitudes = np.zeros(num_bands)

        for i in range(num_bands):
            low = freq_bands[i]
            high = freq_bands[i + 1]
            indices = np.where((freqs >= low) & (freqs < high))[0]
            if len(indices) > 0:
                band_magnitudes[i] = np.mean(fft_magnitude[indices])

        band_magnitudes *= self.settings.sensitivity * 70

        alpha = 1.0 - self.settings.smoothing
        if len(self.smoothed_fft) != num_bands:
            self.smoothed_fft = np.zeros(num_bands)

        self.smoothed_fft = alpha * band_magnitudes + (1 - alpha) * self.smoothed_fft
        self.smoothed_fft = np.clip(self.smoothed_fft, 0.0, 1.0)

        self.rms_level = float(np.sqrt(np.mean(data ** 2)))
        self.peak_level = float(np.max(np.abs(data)))

        return self.smoothed_fft.copy()

    def get_waveform(self) -> np.ndarray:
        """Return raw waveform data."""
        with self.lock:
            return self.audio_data.copy()

    @staticmethod
    def get_devices() -> List[dict]:
        """
        Get list of ALL available audio devices that can provide input.
        This includes:
          - Physical microphones
          - Loopback / monitor devices (system audio capture)
          - Virtual audio cables
          - WASAPI loopback devices (Windows)
        """
        devices = sd.query_devices()
        result = []
        seen_names = set()

        for i, dev in enumerate(devices):
            max_in = dev.get('max_input_channels', 0)
            max_out = dev.get('max_output_channels', 0)
            name = dev['name']
            hostapi_idx = dev.get('hostapi', 0)

            try:
                hostapi_info = sd.query_hostapis(hostapi_idx)
                hostapi_name = hostapi_info.get('name', '')
            except Exception:
                hostapi_name = ''

            is_input = max_in > 0
            is_loopback = False

            # Detect loopback / monitor devices
            name_lower = name.lower()
            loopback_keywords = [
                'loopback', 'stereo mix', 'what u hear', 'what you hear',
                'wave out', 'monitor', 'virtual', 'cable output',
                'voicemeeter', 'vb-audio', 'blackhole', 'soundflower',
                'pulse', 'pipewire'
            ]

            for keyword in loopback_keywords:
                if keyword in name_lower:
                    is_loopback = True
                    break

            # On Windows, WASAPI output devices can be used as loopback
            is_wasapi = 'wasapi' in hostapi_name.lower()
            is_wasapi_output = is_wasapi and max_out > 0 and max_in == 0

            if not is_input and not is_wasapi_output:
                continue

            # Build a unique key to avoid duplicates
            unique_key = f"{name}_{hostapi_name}"
            if unique_key in seen_names:
                continue
            seen_names.add(unique_key)

            # Determine device type label
            if is_wasapi_output:
                dev_type = "loopback"
                channels = max_out
            elif is_loopback:
                dev_type = "loopback"
                channels = max_in
            else:
                dev_type = "input"
                channels = max_in

            result.append({
                'index': i,
                'name': name,
                'channels': channels,
                'sample_rate': int(dev['default_samplerate']),
                'type': dev_type,
                'hostapi': hostapi_name,
                'is_loopback': is_loopback or is_wasapi_output,
            })

        # Sort: loopback devices first, then inputs
        result.sort(key=lambda d: (0 if d['is_loopback'] else 1, d['name']))

        return result

    @staticmethod
    def get_loopback_help() -> str:
        """Return platform-specific help for setting up system audio capture."""
        if sys.platform == 'win32':
            return (
                "WINDOWS — To capture system audio:\n"
                "Option 1: Enable 'Stereo Mix'\n"
                "  → Right-click speaker icon → Sounds → Recording tab\n"
                "  → Right-click empty area → Show Disabled Devices\n"
                "  → Enable 'Stereo Mix'\n\n"
                "Option 2: Install VB-CABLE (free)\n"
                "  → vb-audio.com/Cable → install\n"
                "  → Set 'CABLE Input' as default playback\n"
                "  → Select 'CABLE Output' in this visualizer\n\n"
                "Option 3: Use WASAPI loopback\n"
                "  → Select your output device (speakers/headphones)\n"
                "  → It will appear if WASAPI host is available"
            )
        elif sys.platform == 'darwin':
            return (
                "macOS — To capture system audio:\n"
                "Install BlackHole (free, open-source):\n"
                "  → github.com/ExistentialAudio/BlackHole\n"
                "  → Create Multi-Output Device in Audio MIDI Setup\n"
                "  → Select 'BlackHole' in this visualizer\n\n"
                "Alternative: install Soundflower"
            )
        else:
            return (
                "LINUX — To capture system audio:\n"
                "PulseAudio:\n"
                "  → The 'Monitor of ...' devices capture system audio\n"
                "  → Select a 'Monitor' source in this visualizer\n"
                "  → Or: pactl load-module module-loopback\n\n"
                "PipeWire:\n"
                "  → Monitor sources are available automatically\n"
                "  → Select one in this visualizer"
            )

    @property
    def is_running(self) -> bool:
        return self._running