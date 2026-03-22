"""
Rendering engine for different visualization modes.
Fixed: proper background clearing to prevent burn-in.
"""

import pygame
import numpy as np
import math
from typing import Tuple

from .settings import VisualizerSettings, ColorTheme, VISUALIZATION_MODES
from .effects import ParticleSystem, GlowEffect, WaterfallBuffer


class Renderer:
    """Main rendering class for all visualization modes."""

    def __init__(self, settings: VisualizerSettings):
        self.settings = settings
        self.particle_system = ParticleSystem(settings.particle_count)
        self.glow = GlowEffect()
        self.waterfall: WaterfallBuffer = None
        self.trail_surface: pygame.Surface = None
        self._prev_bands = None
        self._history: list = []
        self._max_history = 200

    def _ensure_surfaces(self, surface: pygame.Surface):
        """Create or resize surfaces as needed."""
        w, h = surface.get_size()

        if self.trail_surface is None or self.trail_surface.get_size() != (w, h):
            self.trail_surface = pygame.Surface((w, h))
            self.trail_surface.fill(self.settings.get_theme().background)

        if self.waterfall is None or (self.waterfall.width, self.waterfall.height) != (w, h):
            self.waterfall = WaterfallBuffer(w, h)

    def render(self, surface: pygame.Surface, band_data: np.ndarray,
               waveform: np.ndarray, mode: int, theme: ColorTheme):
        """Render the current visualization mode."""
        self._ensure_surfaces(surface)
        w, h = surface.get_size()

        # ── Background clearing ──
        # Always do a solid fill first, then optionally composite the trail
        bg = theme.background

        if self.settings.fade_trail:
            # Darken trail surface toward background color each frame
            fade_surface = pygame.Surface((w, h), pygame.SRCALPHA)
            # Higher fade_speed = faster fade = less burn-in
            fade_alpha = max(15, int(self.settings.fade_speed * 3.5))
            fade_alpha = min(255, fade_alpha)
            fade_surface.fill((*bg, fade_alpha))
            self.trail_surface.blit(fade_surface, (0, 0))
            # Copy trail onto main surface
            surface.blit(self.trail_surface, (0, 0))
        else:
            # No trail: hard clear every frame
            surface.fill(bg)
            self.trail_surface.fill(bg)

        # Store history for waterfall
        self._history.append(band_data.copy())
        if len(self._history) > self._max_history:
            self._history.pop(0)

        mode_name = VISUALIZATION_MODES[mode] if mode < len(VISUALIZATION_MODES) else "Bars"

        # Choose render target: draw to trail_surface so it persists for fade,
        # then blit trail onto main surface
        draw_target = self.trail_surface if self.settings.fade_trail else surface

        if mode_name == "Bars":
            self._draw_bars(draw_target, band_data, theme, w, h, mirrored=False)
        elif mode_name == "Bars Mirrored":
            self._draw_bars(draw_target, band_data, theme, w, h, mirrored=True)
        elif mode_name == "Circular":
            self._draw_circular(draw_target, band_data, theme, w, h)
        elif mode_name == "Wave":
            self._draw_wave(draw_target, waveform, band_data, theme, w, h)
        elif mode_name == "Spectrum Line":
            self._draw_spectrum_line(draw_target, band_data, theme, w, h)
        elif mode_name == "Particles":
            self._draw_particle_mode(surface, band_data, theme, w, h)
        elif mode_name == "Waterfall":
            self._draw_waterfall(surface, band_data, theme, w, h)

        # If we drew on trail_surface, composite it onto main surface
        if self.settings.fade_trail and mode_name not in ("Particles", "Waterfall"):
            surface.blit(self.trail_surface, (0, 0))

        # Draw particles overlay if enabled
        if self.settings.particles_enabled and mode_name != "Particles":
            self._emit_beat_particles(band_data, theme, w, h)

        dt = 1.0 / max(1, self.settings.fps_cap)
        self.particle_system.update(dt)
        self.particle_system.draw(surface)

        self._prev_bands = band_data.copy()

    def _lerp_color(self, c1: Tuple, c2: Tuple, t: float) -> Tuple[int, int, int]:
        """Linearly interpolate between two colors."""
        t = max(0.0, min(1.0, t))
        return (
            int(c1[0] + (c2[0] - c1[0]) * t),
            int(c1[1] + (c2[1] - c1[1]) * t),
            int(c1[2] + (c2[2] - c1[2]) * t),
        )

    def _get_bar_color(self, magnitude: float, index: int, total: int,
                       theme: ColorTheme) -> Tuple[int, int, int]:
        """Get color for a bar based on its position and magnitude."""
        position = index / max(1, total - 1)

        if position < 0.5:
            base_color = self._lerp_color(theme.primary, theme.secondary, position * 2)
        else:
            base_color = self._lerp_color(theme.secondary, theme.tertiary, (position - 0.5) * 2)

        # Brighten with magnitude
        brightness = 0.4 + 0.6 * magnitude
        return tuple(max(0, min(255, int(c * brightness))) for c in base_color)

    def _draw_bars(self, surface: pygame.Surface, data: np.ndarray,
                   theme: ColorTheme, w: int, h: int, mirrored: bool):
        """Draw bar visualization."""
        num_bars = len(data)
        total_width = w * 0.9
        bar_gap = total_width / num_bars
        bar_width = max(2, int(bar_gap * self.settings.bar_width_ratio))
        start_x = (w - total_width) / 2
        max_height = h * 0.85

        if mirrored:
            max_height = h * 0.42
            center_y = h // 2
        else:
            center_y = h

        for i, magnitude in enumerate(data):
            x = int(start_x + i * bar_gap)
            bar_height = int(magnitude * max_height)
            bar_height = max(2, bar_height)

            color = self._get_bar_color(magnitude, i, num_bars, theme)

            if mirrored:
                rect_up = pygame.Rect(x, center_y - bar_height, bar_width, bar_height)
                pygame.draw.rect(surface, color, rect_up,
                                 border_radius=self.settings.border_radius)
                rect_down = pygame.Rect(x, center_y, bar_width, bar_height)
                pygame.draw.rect(surface, color, rect_down,
                                 border_radius=self.settings.border_radius)

                if self.settings.glow and magnitude > 0.3:
                    self.glow.draw_glow_rect(surface, color, rect_up, magnitude * 0.5)
                    self.glow.draw_glow_rect(surface, color, rect_down, magnitude * 0.5)
            else:
                rect = pygame.Rect(x, center_y - bar_height, bar_width, bar_height)
                pygame.draw.rect(surface, color, rect,
                                 border_radius=self.settings.border_radius)

                if self.settings.glow and magnitude > 0.3:
                    self.glow.draw_glow_rect(surface, color, rect, magnitude * 0.5)

    def _draw_circular(self, surface: pygame.Surface, data: np.ndarray,
                       theme: ColorTheme, w: int, h: int):
        """Draw circular visualization."""
        cx, cy = w // 2, h // 2
        base_radius = min(w, h) * 0.15
        max_bar_length = min(w, h) * 0.3
        num_bars = len(data)

        for i, magnitude in enumerate(data):
            angle = (2 * math.pi * i / num_bars) - math.pi / 2
            bar_length = magnitude * max_bar_length + 5

            x1 = cx + math.cos(angle) * base_radius
            y1 = cy + math.sin(angle) * base_radius
            x2 = cx + math.cos(angle) * (base_radius + bar_length)
            y2 = cy + math.sin(angle) * (base_radius + bar_length)

            color = self._get_bar_color(magnitude, i, num_bars, theme)
            thickness = max(2, int(3 * (1 + magnitude)))

            pygame.draw.line(surface, color, (int(x1), int(y1)),
                             (int(x2), int(y2)), thickness)

            if self.settings.glow and magnitude > 0.5:
                self.glow.draw_glow_circle(surface, color,
                                           (int(x2), int(y2)), 4, magnitude * 0.4)

        pygame.draw.circle(surface, theme.primary, (cx, cy), int(base_radius), 2)

        avg_magnitude = float(np.mean(data))
        inner_radius = int(base_radius * 0.8 * (0.8 + 0.2 * avg_magnitude))
        inner_color = self._lerp_color(theme.background, theme.primary, avg_magnitude * 0.5)
        pygame.draw.circle(surface, inner_color, (cx, cy), inner_radius)

    def _draw_wave(self, surface: pygame.Surface, waveform: np.ndarray,
                   band_data: np.ndarray, theme: ColorTheme, w: int, h: int):
        """Draw waveform visualization."""
        center_y = h // 2
        amplitude = h * 0.35

        step = max(1, len(waveform) // w)
        samples = waveform[::step][:w]

        for layer in range(3):
            points = []
            color_t = layer / 3.0
            color = self._lerp_color(theme.primary, theme.tertiary, color_t)
            alpha_color = tuple(max(0, min(255, int(c * (1.0 - layer * 0.25)))) for c in color)

            for i, sample in enumerate(samples):
                x = i
                y = center_y + int(sample * amplitude * (1 + layer * 0.3))
                points.append((x, y))

            if len(points) > 2:
                pygame.draw.lines(surface, alpha_color, False, points,
                                  max(1, 3 - layer))

        avg_energy = float(np.mean(band_data))
        if self.settings.glow and avg_energy > 0.2:
            self.glow.draw_glow_circle(
                surface, theme.primary,
                (w // 2, center_y), int(20 * avg_energy), avg_energy * 0.3
            )

    def _draw_spectrum_line(self, surface: pygame.Surface, data: np.ndarray,
                            theme: ColorTheme, w: int, h: int):
        """Draw a smooth spectrum line with fill."""
        margin = w * 0.05
        usable_width = w - 2 * margin
        num_points = len(data)

        points_top = []
        points_bottom = []
        center_y = h * 0.6

        for i, magnitude in enumerate(data):
            x = int(margin + (i / max(1, num_points - 1)) * usable_width)
            y_offset = magnitude * h * 0.5
            points_top.append((x, int(center_y - y_offset)))
            points_bottom.append((x, int(center_y + y_offset * 0.3)))

        if len(points_top) > 2:
            fill_points = points_top + list(reversed(points_bottom))
            fill_surface = pygame.Surface((w, h), pygame.SRCALPHA)
            pygame.draw.polygon(fill_surface,
                                (*theme.primary, 40), fill_points)
            surface.blit(fill_surface, (0, 0))

            pygame.draw.lines(surface, theme.primary, False, points_top, 2)
            pygame.draw.lines(surface, theme.secondary, False, points_bottom, 1)

            for i, (x, y) in enumerate(points_top):
                if data[i] > 0.4:
                    color = self._get_bar_color(data[i], i, num_points, theme)
                    pygame.draw.circle(surface, color, (x, y), 3)
                    if self.settings.glow and data[i] > 0.6:
                        self.glow.draw_glow_circle(surface, color, (x, y), 5,
                                                   data[i] * 0.4)

    def _draw_particle_mode(self, surface: pygame.Surface, data: np.ndarray,
                            theme: ColorTheme, w: int, h: int):
        """Particle-based visualization."""
        num_bands = len(data)
        band_width = w / num_bands

        for i, magnitude in enumerate(data):
            if magnitude > 0.15:
                x = int((i + 0.5) * band_width)
                count = max(1, int(magnitude * 5))
                color = self._get_bar_color(magnitude, i, num_bands, theme)

                self.particle_system.emit(
                    x, h - 10, color,
                    count=count,
                    spread=band_width * 0.3,
                    speed=magnitude * 8
                )

    def _draw_waterfall(self, surface: pygame.Surface, data: np.ndarray,
                        theme: ColorTheme, w: int, h: int):
        """Draw waterfall/spectrogram visualization."""
        self.waterfall.update(data, theme)
        self.waterfall.draw(surface)

    def _emit_beat_particles(self, data: np.ndarray, theme: ColorTheme,
                             w: int, h: int):
        """Emit particles on beats (energy spikes)."""
        if self._prev_bands is None:
            return

        diff = data - self._prev_bands
        for i, d in enumerate(diff):
            if d > 0.3:
                x = int((i / len(data)) * w)
                color = self._get_bar_color(data[i], i, len(data), theme)
                self.particle_system.emit(x, h * 0.5, color, count=2, speed=4)