"""
Visual effects and particle systems.
"""

import numpy as np
import pygame
import random
import math
from typing import List, Tuple


class Particle:
    """A single particle for particle effects."""

    def __init__(self, x: float, y: float, color: Tuple[int, int, int],
                 velocity: Tuple[float, float] = None, lifetime: float = 2.0,
                 size: float = 3.0):
        self.x = x
        self.y = y
        self.color = color
        self.vx = velocity[0] if velocity else random.uniform(-2, 2)
        self.vy = velocity[1] if velocity else random.uniform(-5, -1)
        self.lifetime = lifetime
        self.max_lifetime = lifetime
        self.size = size
        self.alive = True

    def update(self, dt: float):
        """Update particle position and lifetime."""
        self.x += self.vx * dt * 60
        self.y += self.vy * dt * 60
        self.vy += 0.05 * dt * 60  # gravity
        self.lifetime -= dt

        if self.lifetime <= 0:
            self.alive = False

    def draw(self, surface: pygame.Surface):
        """Draw particle with alpha based on lifetime."""
        if not self.alive:
            return

        alpha = max(0, self.lifetime / self.max_lifetime)
        size = max(1, int(self.size * alpha))
        color = tuple(int(c * alpha) for c in self.color)

        pygame.draw.circle(surface, color, (int(self.x), int(self.y)), size)


class ParticleSystem:
    """Manages a collection of particles."""

    def __init__(self, max_particles: int = 200):
        self.particles: List[Particle] = []
        self.max_particles = max_particles

    def emit(self, x: float, y: float, color: Tuple[int, int, int],
             count: int = 1, spread: float = 2.0, speed: float = 3.0):
        """Emit particles from a position."""
        for _ in range(count):
            if len(self.particles) >= self.max_particles:
                break

            vx = random.uniform(-spread, spread)
            vy = random.uniform(-speed * 2, -speed * 0.5)
            lifetime = random.uniform(0.5, 2.0)
            size = random.uniform(1.5, 4.0)

            self.particles.append(
                Particle(x, y, color, (vx, vy), lifetime, size)
            )

    def update(self, dt: float):
        """Update all particles."""
        self.particles = [p for p in self.particles if p.alive]
        for p in self.particles:
            p.update(dt)

    def draw(self, surface: pygame.Surface):
        """Draw all particles."""
        for p in self.particles:
            p.draw(surface)

    def clear(self):
        self.particles.clear()


class GlowEffect:
    """Creates a glow effect on surfaces."""

    @staticmethod
    def draw_glow_circle(surface: pygame.Surface, color: Tuple[int, int, int],
                         pos: Tuple[int, int], radius: int, intensity: float = 0.5):
        """Draw a glowing circle."""
        glow_surface = pygame.Surface((radius * 4, radius * 4), pygame.SRCALPHA)

        for i in range(3, 0, -1):
            alpha = int(50 * intensity / i)
            glow_color = (*color, alpha)
            r = radius * (1 + i * 0.5)
            pygame.draw.circle(glow_surface, glow_color,
                               (radius * 2, radius * 2), int(r))

        pygame.draw.circle(glow_surface, (*color, int(200 * intensity)),
                           (radius * 2, radius * 2), radius)

        surface.blit(glow_surface,
                     (pos[0] - radius * 2, pos[1] - radius * 2),
                     special_flags=pygame.BLEND_ADD)

    @staticmethod
    def draw_glow_rect(surface: pygame.Surface, color: Tuple[int, int, int],
                       rect: pygame.Rect, intensity: float = 0.3):
        """Draw a glowing rectangle."""
        padding = 6
        glow_rect = rect.inflate(padding * 2, padding * 2)
        glow_surface = pygame.Surface(
            (glow_rect.width, glow_rect.height), pygame.SRCALPHA
        )

        for i in range(3, 0, -1):
            alpha = int(40 * intensity / i)
            glow_color = (*color, alpha)
            inflated = pygame.Rect(0, 0, glow_rect.width, glow_rect.height)
            inner = inflated.inflate(-i * 2, -i * 2)
            pygame.draw.rect(glow_surface, glow_color, inner,
                             border_radius=4)

        surface.blit(glow_surface, glow_rect.topleft,
                     special_flags=pygame.BLEND_ADD)


class WaterfallBuffer:
    """Scrolling spectrogram / waterfall display."""

    def __init__(self, width: int, height: int):
        self.width = width
        self.height = height
        self.surface = pygame.Surface((width, height))
        self.surface.fill((0, 0, 0))

    def update(self, band_data: np.ndarray, theme):
        """Add a new line of spectrum data to the waterfall."""
        # Scroll the surface up by 1 pixel
        temp = self.surface.copy()
        self.surface.fill((0, 0, 0))
        self.surface.blit(temp, (0, -1))

        # Draw new line at the bottom
        num_bands = len(band_data)
        band_width = self.width / num_bands

        for i, magnitude in enumerate(band_data):
            x = int(i * band_width)
            w = max(1, int(band_width))

            # Color based on magnitude
            r = int(min(255, theme.primary[0] * magnitude + theme.secondary[0] * (1 - magnitude)))
            g = int(min(255, theme.primary[1] * magnitude + theme.secondary[1] * (1 - magnitude)))
            b = int(min(255, theme.primary[2] * magnitude + theme.secondary[2] * (1 - magnitude)))
            brightness = min(1.0, magnitude * 2)

            color = (int(r * brightness), int(g * brightness), int(b * brightness))
            pygame.draw.rect(self.surface, color,
                             (x, self.height - 1, w, 1))

    def draw(self, target_surface: pygame.Surface, pos: Tuple[int, int] = (0, 0)):
        target_surface.blit(self.surface, pos)

    def resize(self, width: int, height: int):
        self.width = width
        self.height = height
        self.surface = pygame.Surface((width, height))
        self.surface.fill((0, 0, 0))