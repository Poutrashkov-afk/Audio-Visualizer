"""
Main application class that ties everything together.
"""

import pygame
import sys
import numpy as np

from .settings import VisualizerSettings, VISUALIZATION_MODES
from .audio_engine import AudioEngine
from .renderer import Renderer
from .ui import SettingsPanel


class AudioVisualizerApp:
    """Main application."""

    def __init__(self):
        pygame.init()
        pygame.font.init()

        self.settings = VisualizerSettings.load()
        self.clock = pygame.time.Clock()
        self.running = False

        self._setup_display()

        self.audio_engine = AudioEngine(self.settings)
        self.renderer = Renderer(self.settings)

        # Settings panel
        self.settings_panel = SettingsPanel(
            self.settings,
            on_device_change=self._on_audio_device_changed
        )

        # Populate audio devices and loopback help
        self._refresh_audio_devices()
        self.settings_panel.set_loopback_help(AudioEngine.get_loopback_help())

        self.font = pygame.font.SysFont("Segoe UI", 16)
        self.small_font = pygame.font.SysFont("Segoe UI", 12)
        self.title_font = pygame.font.SysFont("Segoe UI", 22, bold=True)

        self.show_help = True
        self.help_timer = 5.0

        self._status_msg = ""
        self._status_timer = 0.0

    def _refresh_audio_devices(self):
        devices = AudioEngine.get_devices()
        self.settings_panel.set_audio_devices(devices)

        loopback_count = sum(1 for d in devices if d.get('is_loopback'))
        input_count = len(devices) - loopback_count

        print(f"Found {len(devices)} audio device(s): "
              f"{input_count} input, {loopback_count} loopback/system")
        for d in devices:
            tag = "LOOPBACK" if d.get('is_loopback') else "INPUT"
            marker = " ◄ ACTIVE" if d['index'] == self.settings.device_index else ""
            api = f" ({d.get('hostapi', '')})" if d.get('hostapi') else ""
            print(f"  [{d['index']:2d}] [{tag:8s}] {d['name']}{api}{marker}")

    def _on_audio_device_changed(self, device_index: int):
        self.audio_engine.restart_with_device(device_index)
        self.renderer.trail_surface = None
        self.renderer.waterfall = None

        device_name = "System Default"
        if device_index != -1:
            for d in self.settings_panel._audio_devices:
                if d['index'] == device_index:
                    device_name = d['name']
                    break

        self._show_status(f"Switched to: {device_name}")

    def _show_status(self, msg: str, duration: float = 3.0):
        self._status_msg = msg
        self._status_timer = duration
        print(f"[Status] {msg}")

    def _setup_display(self):
        flags = pygame.RESIZABLE
        if self.settings.fullscreen:
            flags = pygame.FULLSCREEN | pygame.HWSURFACE | pygame.DOUBLEBUF

        if self.settings.fullscreen:
            info = pygame.display.Info()
            self.screen = pygame.display.set_mode(
                (info.current_w, info.current_h), flags
            )
        else:
            self.screen = pygame.display.set_mode(
                (self.settings.window_width, self.settings.window_height), flags
            )

        pygame.display.set_caption("Audio Visualizer")

    def _toggle_fullscreen(self):
        self.settings.fullscreen = not self.settings.fullscreen
        self._setup_display()
        self.renderer.trail_surface = None
        self.renderer.waterfall = None

    def _handle_events(self):
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                self.running = False
                return

            if self.settings_panel.handle_event(event):
                continue

            if event.type == pygame.KEYDOWN:
                self._handle_keydown(event)

            elif event.type == pygame.VIDEORESIZE:
                if not self.settings.fullscreen:
                    self.settings.window_width = event.w
                    self.settings.window_height = event.h
                    self.renderer.trail_surface = None
                    self.renderer.waterfall = None

    def _handle_keydown(self, event: pygame.event.Event):
        if event.key == pygame.K_ESCAPE:
            if self.settings_panel.visible:
                self.settings_panel.toggle()
            elif self.settings.fullscreen:
                self._toggle_fullscreen()
            else:
                self.running = False

        elif event.key == pygame.K_F11 or (event.key == pygame.K_f and not self.settings_panel.visible):
            self._toggle_fullscreen()

        elif event.key == pygame.K_TAB:
            self.settings_panel.toggle()

        elif event.key == pygame.K_SPACE or event.key == pygame.K_RIGHT:
            self.settings.visualization_mode = (
                (self.settings.visualization_mode + 1) % len(VISUALIZATION_MODES)
            )
            self._show_status(f"Mode: {VISUALIZATION_MODES[self.settings.visualization_mode]}", 2.0)

        elif event.key == pygame.K_LEFT:
            self.settings.visualization_mode = (
                (self.settings.visualization_mode - 1) % len(VISUALIZATION_MODES)
            )
            self._show_status(f"Mode: {VISUALIZATION_MODES[self.settings.visualization_mode]}", 2.0)

        elif event.key == pygame.K_UP:
            self.settings.sensitivity = min(5.0, self.settings.sensitivity + 0.1)

        elif event.key == pygame.K_DOWN:
            self.settings.sensitivity = max(0.1, self.settings.sensitivity - 0.1)

        elif event.key == pygame.K_g:
            self.settings.glow = not self.settings.glow

        elif event.key == pygame.K_p:
            self.settings.particles_enabled = not self.settings.particles_enabled

        elif event.key == pygame.K_h:
            self.show_help = not self.show_help
            self.help_timer = 999 if self.show_help else 0

        elif event.key == pygame.K_s and pygame.key.get_mods() & pygame.KMOD_CTRL:
            self.settings.save()
            self._show_status("Settings saved!")

    def _draw_hud(self):
        w, h = self.screen.get_size()

        # FPS
        if self.settings.show_fps:
            fps = self.clock.get_fps()
            fps_color = (100, 255, 100) if fps > 50 else (255, 255, 0) if fps > 30 else (255, 80, 80)
            fps_text = self.small_font.render(f"FPS: {fps:.0f}", True, fps_color)
            self.screen.blit(fps_text, (w - 80, 10))

        # Mode pill
        mode_name = VISUALIZATION_MODES[self.settings.visualization_mode]
        theme = self.settings.get_theme()
        mode_text = self.font.render(mode_name, True, theme.primary)
        pill_rect = mode_text.get_rect()
        pill_rect.bottomright = (w - 15, h - 12)
        pill_bg = pill_rect.inflate(16, 8)

        pill_surface = pygame.Surface(pill_bg.size)
        pill_surface.fill((0, 0, 0))
        pill_surface.set_alpha(160)
        self.screen.blit(pill_surface, pill_bg.topleft)
        pygame.draw.rect(self.screen, theme.primary, pill_bg, 1, border_radius=12)
        self.screen.blit(mode_text, pill_rect)

        # Audio level
        if self.audio_engine.is_running:
            level = self.audio_engine.rms_level
            bar_w = 100
            bar_h = 4
            bar_x = w - bar_w - 15
            bar_y = h - 50
            pygame.draw.rect(self.screen, (40, 40, 60),
                             (bar_x, bar_y, bar_w, bar_h), border_radius=2)
            fill_w = int(bar_w * min(1.0, level * 10))
            if fill_w > 0:
                color = (0, 200, 100) if level < 0.08 else (255, 200, 0) if level < 0.15 else (255, 50, 50)
                pygame.draw.rect(self.screen, color,
                                 (bar_x, bar_y, fill_w, bar_h), border_radius=2)

        # Status message
        if self._status_timer > 0:
            alpha = min(1.0, self._status_timer / 0.5)
            status_surf = self.font.render(self._status_msg, True, (200, 255, 200))
            status_bg = pygame.Surface((status_surf.get_width() + 20, status_surf.get_height() + 10))
            status_bg.fill((0, 0, 0))
            status_bg.set_alpha(int(150 * alpha))
            pos_x = (w - status_bg.get_width()) // 2
            pos_y = h - 80
            self.screen.blit(status_bg, (pos_x, pos_y))
            temp = status_surf.copy()
            temp.set_alpha(int(255 * alpha))
            self.screen.blit(temp, (pos_x + 10, pos_y + 5))

        # Help
        if self.show_help and self.help_timer > 0:
            self._draw_help()

    def _draw_help(self):
        help_lines = [
            ("CONTROLS", True),
            ("Tab          Settings panel", False),
            ("Space / ← →  Change mode", False),
            ("↑ / ↓        Sensitivity", False),
            ("F / F11      Fullscreen", False),
            ("G            Toggle glow", False),
            ("P            Toggle particles", False),
            ("H            Toggle this help", False),
            ("Ctrl+S       Save settings", False),
            ("Esc          Exit / Back", False),
        ]

        padding = 16
        line_height = 22
        box_w = 280
        box_h = len(help_lines) * line_height + padding * 2

        help_surface = pygame.Surface((box_w, box_h))
        help_surface.fill((8, 8, 20))
        help_surface.set_alpha(220)
        self.screen.blit(help_surface, (10, 10))

        pygame.draw.rect(self.screen, (60, 130, 255),
                         (10, 10, box_w, box_h), 1, border_radius=8)

        for i, (line, is_header) in enumerate(help_lines):
            color = (80, 180, 255) if is_header else (200, 200, 215)
            font = self.font if is_header else self.small_font
            text = font.render(line, True, color)
            self.screen.blit(text, (10 + padding, 10 + padding + i * line_height))

    def _draw_no_audio_message(self):
        w, h = self.screen.get_size()

        box_w, box_h = 500, 140
        box_x, box_y = (w - box_w) // 2, (h - box_h) // 2

        bg = pygame.Surface((box_w, box_h))
        bg.fill((15, 10, 25))
        self.screen.blit(bg, (box_x, box_y))
        pygame.draw.rect(self.screen, (255, 80, 80),
                         (box_x, box_y, box_w, box_h), 2, border_radius=10)

        msg = self.title_font.render("No audio input detected", True, (255, 100, 100))
        self.screen.blit(msg, (box_x + (box_w - msg.get_width()) // 2, box_y + 15))

        sub = self.font.render("Check your microphone or audio device", True, (180, 180, 200))
        self.screen.blit(sub, (box_x + (box_w - sub.get_width()) // 2, box_y + 50))

        hint = self.small_font.render("Press Tab → Settings → Audio Source to select a device", True, (140, 160, 180))
        self.screen.blit(hint, (box_x + (box_w - hint.get_width()) // 2, box_y + 80))

        hint2 = self.small_font.render("Use a loopback device to visualize system audio", True, (200, 160, 80))
        self.screen.blit(hint2, (box_x + (box_w - hint2.get_width()) // 2, box_y + 105))

    def run(self):
        print("=" * 55)
        print("  Audio Visualizer v1.0")
        print("  Supports: Microphone, Loopback, Virtual Cables")
        print("=" * 55)
        print()

        self.audio_engine.start()
        self.running = True

        print("\nRunning. Press Tab for settings, H for help.\n")

        while self.running:
            dt = self.clock.tick(self.settings.fps_cap) / 1000.0

            if self.help_timer > 0 and self.help_timer < 999:
                self.help_timer -= dt

            if self._status_timer > 0:
                self._status_timer -= dt

            self._handle_events()

            if self.audio_engine.is_running:
                band_data = self.audio_engine.get_frequency_bands(
                    self.settings.bar_count
                )
                waveform = self.audio_engine.get_waveform()
            else:
                band_data = np.zeros(self.settings.bar_count)
                waveform = np.zeros(self.settings.chunk_size)

            theme = self.settings.get_theme()

            self.renderer.render(
                self.screen, band_data, waveform,
                self.settings.visualization_mode, theme
            )

            if not self.audio_engine.is_running:
                self._draw_no_audio_message()

            self._draw_hud()
            self.settings_panel.draw(self.screen)
            pygame.display.flip()

        self.audio_engine.stop()
        self.settings.save()
        pygame.quit()
        sys.exit(0)