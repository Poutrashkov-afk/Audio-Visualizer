"""
Settings and configuration for the audio visualizer.
All customizable parameters are defined here.
"""

import json
import os
from dataclasses import dataclass, field, asdict
from typing import Tuple, List


@dataclass
class ColorTheme:
    """Defines a color theme for the visualizer."""
    name: str
    primary: Tuple[int, int, int] = (0, 200, 255)
    secondary: Tuple[int, int, int] = (255, 0, 150)
    tertiary: Tuple[int, int, int] = (0, 255, 100)
    background: Tuple[int, int, int] = (10, 10, 20)
    text: Tuple[int, int, int] = (220, 220, 220)
    accent: Tuple[int, int, int] = (255, 255, 0)


# Predefined themes
THEMES = {
    "Neon": ColorTheme(
        name="Neon",
        primary=(0, 255, 255),
        secondary=(255, 0, 255),
        tertiary=(255, 255, 0),
        background=(5, 5, 15),
    ),
    "Ocean": ColorTheme(
        name="Ocean",
        primary=(0, 150, 255),
        secondary=(0, 80, 180),
        tertiary=(100, 200, 255),
        background=(5, 10, 30),
    ),
    "Fire": ColorTheme(
        name="Fire",
        primary=(255, 100, 0),
        secondary=(255, 50, 0),
        tertiary=(255, 200, 0),
        background=(20, 5, 5),
    ),
    "Forest": ColorTheme(
        name="Forest",
        primary=(0, 200, 80),
        secondary=(0, 150, 50),
        tertiary=(100, 255, 100),
        background=(5, 15, 10),
    ),
    "Synthwave": ColorTheme(
        name="Synthwave",
        primary=(255, 0, 200),
        secondary=(100, 0, 255),
        tertiary=(0, 200, 255),
        background=(15, 0, 30),
    ),
    "Monochrome": ColorTheme(
        name="Monochrome",
        primary=(200, 200, 200),
        secondary=(150, 150, 150),
        tertiary=(255, 255, 255),
        background=(10, 10, 10),
    ),
}


@dataclass
class VisualizerSettings:
    """All configurable settings for the visualizer."""

    # Window
    window_width: int = 1280
    window_height: int = 720
    fullscreen: bool = False
    fps_cap: int = 60

    # Audio
    sample_rate: int = 44100
    chunk_size: int = 2048
    device_index: int = -1  # -1 = default

    # Visualization
    visualization_mode: int = 0  # index into VISUALIZATION_MODES
    bar_count: int = 64
    smoothing: float = 0.3  # 0.0 to 1.0
    sensitivity: float = 1.5
    min_frequency: int = 20
    max_frequency: int = 16000

    # Effects
    mirror: bool = False
    glow: bool = True
    fade_trail: bool = True
    fade_speed: int = 50  # 1 to 100 — higher = faster fade (less burn)
    particles_enabled: bool = True
    particle_count: int = 100

    # Appearance
    theme_name: str = "Neon"
    bar_width_ratio: float = 0.8  # ratio of bar width to gap
    border_radius: int = 3
    show_fps: bool = True
    show_frequency_labels: bool = True

    def get_theme(self) -> ColorTheme:
        return THEMES.get(self.theme_name, THEMES["Neon"])

    def save(self, filepath: str = "visualizer_settings.json"):
        """Save settings to JSON file."""
        data = asdict(self)
        with open(filepath, 'w') as f:
            json.dump(data, f, indent=2)

    @classmethod
    def load(cls, filepath: str = "visualizer_settings.json") -> 'VisualizerSettings':
        """Load settings from JSON file."""
        if not os.path.exists(filepath):
            return cls()
        try:
            with open(filepath, 'r') as f:
                data = json.load(f)
            return cls(**{k: v for k, v in data.items()
                         if k in cls.__dataclass_fields__})
        except (json.JSONDecodeError, TypeError):
            return cls()


VISUALIZATION_MODES = [
    "Bars",
    "Bars Mirrored",
    "Circular",
    "Wave",
    "Spectrum Line",
    "Particles",
    "Waterfall",
]