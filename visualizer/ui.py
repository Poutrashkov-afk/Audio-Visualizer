"""
UI overlay for settings and controls.
High-contrast, readable settings panel with opaque dropdowns, audio device selector,
and loopback/system audio help.
"""

import pygame
import sys
from typing import List, Tuple, Optional, Callable
from .settings import VisualizerSettings, THEMES, VISUALIZATION_MODES


# ─── Color palette ───────────────────────────────────────────────────────────
UI_BG            = (18, 18, 28)
UI_BG_ALPHA      = 245
UI_PANEL_BORDER  = (80, 130, 255)
UI_SECTION_BG    = (30, 30, 48)
UI_HOVER         = (45, 45, 70)
UI_ACTIVE        = (0, 140, 255)
UI_ACTIVE_DARK   = (0, 90, 180)
UI_TEXT          = (230, 230, 240)
UI_TEXT_DIM      = (160, 165, 180)
UI_TEXT_BRIGHT   = (255, 255, 255)
UI_LABEL         = (120, 180, 255)
UI_TRACK         = (50, 50, 72)
UI_TRACK_FILL    = (0, 160, 255)
UI_HANDLE        = (255, 255, 255)
UI_TOGGLE_ON     = (0, 180, 120)
UI_TOGGLE_OFF    = (70, 70, 90)
UI_BORDER        = (60, 65, 90)
UI_DROPDOWN_BG   = (22, 22, 38)
UI_DROPDOWN_HOVER = (50, 55, 85)
UI_DROPDOWN_SEL  = (0, 80, 160)
UI_SAVE_BTN      = (0, 140, 80)
UI_LOAD_BTN      = (140, 100, 0)
UI_LOOPBACK_TAG  = (255, 160, 0)
UI_INPUT_TAG     = (0, 200, 120)
UI_HELP_BG       = (25, 20, 40)
UI_HELP_BORDER   = (200, 150, 50)


class UIElement:
    """Base UI element."""

    def __init__(self, x: int, y: int, w: int, h: int):
        self.rect = pygame.Rect(x, y, w, h)
        self.visible = True
        self.hovered = False
        self.z_order = 0

    def handle_event(self, event: pygame.event.Event) -> bool:
        return False

    def update(self, mouse_pos: Tuple[int, int]):
        self.hovered = self.rect.collidepoint(mouse_pos)

    def draw(self, surface: pygame.Surface, font: pygame.font.Font):
        pass

    def draw_overlay(self, surface: pygame.Surface, font: pygame.font.Font):
        pass


class Slider(UIElement):
    """Slider UI element."""

    def __init__(self, x: int, y: int, w: int, label: str,
                 min_val: float, max_val: float, value: float,
                 on_change: Callable = None, is_int: bool = False):
        super().__init__(x, y, w, 40)
        self.label = label
        self.min_val = min_val
        self.max_val = max_val
        self.value = value
        self.on_change = on_change
        self.is_int = is_int
        self.dragging = False
        self.track_rect = pygame.Rect(x, y + 22, w, 12)

    def handle_event(self, event: pygame.event.Event) -> bool:
        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
            hit = self.track_rect.inflate(0, 16)
            if hit.collidepoint(event.pos):
                self.dragging = True
                self._update_value(event.pos[0])
                return True
        elif event.type == pygame.MOUSEBUTTONUP and event.button == 1:
            if self.dragging:
                self.dragging = False
                return True
        elif event.type == pygame.MOUSEMOTION and self.dragging:
            self._update_value(event.pos[0])
            return True
        return False

    def _update_value(self, mouse_x: int):
        ratio = (mouse_x - self.track_rect.x) / self.track_rect.width
        ratio = max(0.0, min(1.0, ratio))
        self.value = self.min_val + ratio * (self.max_val - self.min_val)
        if self.is_int:
            self.value = int(round(self.value))
        if self.on_change:
            self.on_change(self.value)

    def draw(self, surface: pygame.Surface, font: pygame.font.Font):
        if not self.visible:
            return

        display_val = str(int(self.value)) if self.is_int else f"{self.value:.2f}"
        label_surf = font.render(self.label, True, UI_TEXT)
        value_surf = font.render(display_val, True, UI_TEXT_BRIGHT)
        surface.blit(label_surf, (self.rect.x, self.rect.y + 2))
        surface.blit(value_surf, (self.rect.right - value_surf.get_width(), self.rect.y + 2))

        pygame.draw.rect(surface, UI_TRACK, self.track_rect, border_radius=6)

        ratio = (self.value - self.min_val) / max(0.001, self.max_val - self.min_val)
        fill_w = int(self.track_rect.width * ratio)
        fill_rect = pygame.Rect(self.track_rect.x, self.track_rect.y, fill_w, self.track_rect.height)
        pygame.draw.rect(surface, UI_TRACK_FILL, fill_rect, border_radius=6)

        handle_x = self.track_rect.x + fill_w
        handle_y = self.track_rect.centery
        radius = 8 if self.dragging else 7
        pygame.draw.circle(surface, UI_HANDLE, (handle_x, handle_y), radius)
        pygame.draw.circle(surface, UI_TRACK_FILL, (handle_x, handle_y), radius, 2)


class Button(UIElement):
    """Toggle / action button."""

    def __init__(self, x: int, y: int, w: int, h: int, label: str,
                 on_click: Callable = None, toggle: bool = False,
                 toggled: bool = False, color_on=None, color_off=None):
        super().__init__(x, y, w, h)
        self.label = label
        self.on_click = on_click
        self.toggle = toggle
        self.toggled = toggled
        self.color_on = color_on or UI_TOGGLE_ON
        self.color_off = color_off or UI_TOGGLE_OFF

    def handle_event(self, event: pygame.event.Event) -> bool:
        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
            if self.rect.collidepoint(event.pos):
                if self.toggle:
                    self.toggled = not self.toggled
                if self.on_click:
                    self.on_click(self.toggled if self.toggle else None)
                return True
        return False

    def draw(self, surface: pygame.Surface, font: pygame.font.Font):
        if not self.visible:
            return

        if self.toggle:
            bg = self.color_on if self.toggled else self.color_off
            border = (0, 220, 150) if self.toggled else UI_BORDER
            text_color = UI_TEXT_BRIGHT if self.toggled else UI_TEXT_DIM
        else:
            bg = UI_HOVER if self.hovered else UI_SECTION_BG
            border = UI_ACTIVE if self.hovered else UI_BORDER
            text_color = UI_TEXT_BRIGHT if self.hovered else UI_TEXT

        pygame.draw.rect(surface, bg, self.rect, border_radius=6)
        pygame.draw.rect(surface, border, self.rect, 2, border_radius=6)

        label_text = self.label
        if self.toggle:
            label_text = ("● " if self.toggled else "○ ") + self.label

        text = font.render(label_text, True, text_color)
        text_rect = text.get_rect(center=self.rect.center)
        surface.blit(text, text_rect)


class Dropdown(UIElement):
    """Dropdown selector with fully opaque options and device type tags."""

    def __init__(self, x: int, y: int, w: int, label: str,
                 options: List[str], selected: int = 0,
                 on_change: Callable = None, max_visible: int = 0,
                 option_tags: List[str] = None):
        super().__init__(x, y, w, 32)
        self.label = label
        self.options = options
        self.selected = selected
        self.on_change = on_change
        self.expanded = False
        self.option_height = 30
        self.max_visible = max_visible
        self.scroll_offset = 0
        self.z_order = 10
        self.option_tags = option_tags or []  # e.g. ["loopback", "input", ...]

    def _get_visible_count(self) -> int:
        visible = self.max_visible if self.max_visible > 0 else len(self.options)
        return min(visible, len(self.options))

    def _get_dropdown_rect(self) -> pygame.Rect:
        visible = self._get_visible_count()
        total_h = visible * self.option_height
        return pygame.Rect(self.rect.x, self.rect.bottom, self.rect.width, total_h)

    def handle_event(self, event: pygame.event.Event) -> bool:
        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
            if self.rect.collidepoint(event.pos):
                self.expanded = not self.expanded
                self.scroll_offset = 0
                return True

            if self.expanded:
                dd_rect = self._get_dropdown_rect()
                if dd_rect.collidepoint(event.pos):
                    rel_y = event.pos[1] - dd_rect.y
                    index = self.scroll_offset + int(rel_y // self.option_height)
                    if 0 <= index < len(self.options):
                        self.selected = index
                        self.expanded = False
                        if self.on_change:
                            self.on_change(index)
                    return True
                self.expanded = False
                return False

        if event.type == pygame.MOUSEWHEEL and self.expanded:
            dd_rect = self._get_dropdown_rect()
            if dd_rect.collidepoint(pygame.mouse.get_pos()):
                self.scroll_offset -= event.y
                max_scroll = len(self.options) - self._get_visible_count()
                self.scroll_offset = max(0, min(self.scroll_offset, max(0, max_scroll)))
                return True

        return False

    def _get_tag_color(self, tag: str) -> Tuple[int, int, int]:
        if tag == "loopback":
            return UI_LOOPBACK_TAG
        elif tag == "input":
            return UI_INPUT_TAG
        return UI_TEXT_DIM

    def _get_tag_bg(self, tag: str) -> Tuple[int, int, int]:
        if tag == "loopback":
            return (60, 40, 10)
        elif tag == "input":
            return (10, 40, 25)
        return (40, 40, 50)

    def draw(self, surface: pygame.Surface, font: pygame.font.Font):
        if not self.visible:
            return

        # Label
        label_surf = font.render(self.label, True, UI_LABEL)
        surface.blit(label_surf, (self.rect.x, self.rect.y - 18))

        # Main box
        bg = UI_HOVER if (self.hovered or self.expanded) else UI_SECTION_BG
        pygame.draw.rect(surface, bg, self.rect, border_radius=5)
        border = UI_ACTIVE if self.expanded else UI_BORDER
        pygame.draw.rect(surface, border, self.rect, 2, border_radius=5)

        # Selected text
        sel_str = self.options[self.selected] if self.selected < len(self.options) else "—"
        max_text_w = self.rect.width - 30

        # Show tag for selected item
        if self.option_tags and self.selected < len(self.option_tags):
            tag = self.option_tags[self.selected]
            if tag:
                tag_surf = font.render(tag.upper(), True, self._get_tag_color(tag))
                tag_w = tag_surf.get_width() + 8
                max_text_w -= tag_w + 4

                tag_rect = pygame.Rect(
                    self.rect.right - 28 - tag_w, self.rect.y + 6,
                    tag_w + 4, 20
                )
                pygame.draw.rect(surface, self._get_tag_bg(tag), tag_rect, border_radius=3)
                surface.blit(tag_surf, (tag_rect.x + 2, tag_rect.y + 2))

        sel_surf = font.render(sel_str, True, UI_TEXT_BRIGHT)
        if sel_surf.get_width() > max_text_w:
            while sel_surf.get_width() > max_text_w and len(sel_str) > 3:
                sel_str = sel_str[:-4] + "..."
                sel_surf = font.render(sel_str, True, UI_TEXT_BRIGHT)
        surface.blit(sel_surf, (self.rect.x + 10, self.rect.y + 8))

        # Arrow
        arrow = "▼" if not self.expanded else "▲"
        arrow_surf = font.render(arrow, True, UI_TEXT_DIM)
        surface.blit(arrow_surf, (self.rect.right - 22, self.rect.y + 8))

    def draw_overlay(self, surface: pygame.Surface, font: pygame.font.Font):
        if not self.expanded or not self.visible:
            return

        visible_count = self._get_visible_count()
        dd_rect = self._get_dropdown_rect()
        mouse_pos = pygame.mouse.get_pos()

        # Solid opaque background
        pygame.draw.rect(surface, UI_DROPDOWN_BG, dd_rect)
        pygame.draw.rect(surface, UI_ACTIVE, dd_rect, 2)

        for vi in range(visible_count):
            idx = self.scroll_offset + vi
            if idx >= len(self.options):
                break

            opt_rect = pygame.Rect(
                dd_rect.x + 2, dd_rect.y + vi * self.option_height,
                dd_rect.width - 4, self.option_height
            )

            is_selected = idx == self.selected
            is_hovered = opt_rect.collidepoint(mouse_pos)

            if is_selected:
                opt_bg = UI_DROPDOWN_SEL
            elif is_hovered:
                opt_bg = UI_DROPDOWN_HOVER
            else:
                opt_bg = UI_DROPDOWN_BG

            pygame.draw.rect(surface, opt_bg, opt_rect)

            if vi > 0:
                pygame.draw.line(surface, (50, 50, 70),
                                 (opt_rect.x + 4, opt_rect.y),
                                 (opt_rect.right - 4, opt_rect.y), 1)

            # Tag badge
            tag_w_total = 0
            if self.option_tags and idx < len(self.option_tags):
                tag = self.option_tags[idx]
                if tag:
                    tag_text = tag.upper()
                    tag_surf = font.render(tag_text, True, self._get_tag_color(tag))
                    tag_w_total = tag_surf.get_width() + 12
                    tag_rect = pygame.Rect(
                        opt_rect.right - tag_w_total - 4, opt_rect.y + 5,
                        tag_w_total, 20
                    )
                    pygame.draw.rect(surface, self._get_tag_bg(tag), tag_rect, border_radius=3)
                    surface.blit(tag_surf, (tag_rect.x + 4, tag_rect.y + 2))

            # Option text
            text_color = UI_TEXT_BRIGHT if (is_selected or is_hovered) else UI_TEXT
            prefix = "► " if is_selected else "   "
            option_str = self.options[idx]

            max_w = opt_rect.width - 24 - tag_w_total
            opt_surf = font.render(prefix + option_str, True, text_color)
            if opt_surf.get_width() > max_w:
                while opt_surf.get_width() > max_w and len(option_str) > 5:
                    option_str = option_str[:-4] + "..."
                    opt_surf = font.render(prefix + option_str, True, text_color)

            surface.blit(opt_surf, (opt_rect.x + 8, opt_rect.y + 6))

        # Scroll indicators
        if self.scroll_offset > 0:
            pygame.draw.rect(surface, UI_DROPDOWN_BG,
                             (dd_rect.right - 65, dd_rect.y, 63, 18))
            arrow_up = font.render("▲ more", True, UI_TEXT_DIM)
            surface.blit(arrow_up, (dd_rect.right - 60, dd_rect.y + 2))

        max_scroll = len(self.options) - visible_count
        if self.scroll_offset < max_scroll:
            pygame.draw.rect(surface, UI_DROPDOWN_BG,
                             (dd_rect.right - 65, dd_rect.bottom - 18, 63, 18))
            arrow_dn = font.render("▼ more", True, UI_TEXT_DIM)
            surface.blit(arrow_dn, (dd_rect.right - 60, dd_rect.bottom - 16))


class HelpPopup:
    """Popup window showing help text for system audio setup."""

    def __init__(self):
        self.visible = False
        self.text = ""
        self.scroll_y = 0

    def show(self, text: str):
        self.text = text
        self.visible = True
        self.scroll_y = 0

    def hide(self):
        self.visible = False

    def toggle(self, text: str):
        if self.visible:
            self.hide()
        else:
            self.show(text)

    def handle_event(self, event: pygame.event.Event) -> bool:
        if not self.visible:
            return False

        if event.type == pygame.MOUSEBUTTONDOWN:
            # Close on click outside
            return True

        if event.type == pygame.KEYDOWN:
            if event.key in (pygame.K_ESCAPE, pygame.K_h):
                self.hide()
                return True

        if event.type == pygame.MOUSEWHEEL:
            self.scroll_y += event.y * 15
            self.scroll_y = min(0, self.scroll_y)
            return True

        return False

    def draw(self, surface: pygame.Surface, font: pygame.font.Font, title_font: pygame.font.Font):
        if not self.visible:
            return

        sw, sh = surface.get_size()
        popup_w = min(500, sw - 80)
        popup_h = min(400, sh - 80)
        px = (sw - popup_w) // 2
        py = (sh - popup_h) // 2

        # Darken background
        overlay = pygame.Surface((sw, sh))
        overlay.fill((0, 0, 0))
        overlay.set_alpha(150)
        surface.blit(overlay, (0, 0))

        # Popup background
        popup_rect = pygame.Rect(px, py, popup_w, popup_h)
        pygame.draw.rect(surface, UI_HELP_BG, popup_rect, border_radius=10)
        pygame.draw.rect(surface, UI_HELP_BORDER, popup_rect, 2, border_radius=10)

        # Title
        title = title_font.render("System Audio Setup Guide", True, UI_HELP_BORDER)
        surface.blit(title, (px + 20, py + 15))

        close_text = font.render("[Esc / Click to close]", True, UI_TEXT_DIM)
        surface.blit(close_text, (px + popup_w - close_text.get_width() - 15, py + 18))

        # Divider
        pygame.draw.line(surface, UI_HELP_BORDER,
                         (px + 15, py + 45), (px + popup_w - 15, py + 45), 1)

        # Content area with clipping
        content_rect = pygame.Rect(px + 15, py + 55, popup_w - 30, popup_h - 70)
        content_surface = pygame.Surface((content_rect.width, content_rect.height))
        content_surface.fill(UI_HELP_BG)

        lines = self.text.split('\n')
        y_offset = self.scroll_y
        line_height = 20

        for line in lines:
            if not line.strip():
                y_offset += line_height // 2
                continue

            # Detect headers (lines ending with ':')
            is_header = line.strip().endswith(':') and not line.startswith(' ')
            if is_header:
                color = UI_LOOPBACK_TAG
                rendered_font = title_font
            elif line.startswith('  →'):
                color = (150, 220, 255)
                rendered_font = font
            else:
                color = UI_TEXT
                rendered_font = font

            text_surf = rendered_font.render(line, True, color)

            if 0 <= y_offset < content_rect.height:
                content_surface.blit(text_surf, (5, y_offset))

            y_offset += line_height

        surface.blit(content_surface, content_rect.topleft)


class SettingsPanel:
    """Full settings panel with audio device selection and loopback help."""

    def __init__(self, settings: VisualizerSettings, on_device_change: Callable = None):
        self.settings = settings
        self.on_device_change = on_device_change
        self.visible = False
        self.font: Optional[pygame.font.Font] = None
        self.small_font: Optional[pygame.font.Font] = None
        self.title_font: Optional[pygame.font.Font] = None
        self.elements: List[UIElement] = []
        self._initialized = False
        self.panel_width = 370
        self.scroll_y = 0
        self._section_labels: List[Tuple[str, int, int]] = []
        self._audio_devices: List[dict] = []
        self._device_dropdown: Optional[Dropdown] = None
        self._help_popup = HelpPopup()
        self._loopback_help_text = ""

    def set_audio_devices(self, devices: List[dict]):
        self._audio_devices = devices
        self._initialized = False

    def set_loopback_help(self, text: str):
        self._loopback_help_text = text

    def _init_ui(self):
        if self._initialized:
            return

        self.font = pygame.font.SysFont("Segoe UI", 15)
        self.small_font = pygame.font.SysFont("Segoe UI", 13)
        self.title_font = pygame.font.SysFont("Segoe UI", 18, bold=True)
        self._rebuild_elements()
        self._initialized = True

    def _rebuild_elements(self):
        self.elements.clear()
        self._section_labels.clear()
        x = 20
        y = 70
        w = self.panel_width - 40
        slider_spacing = 52
        section_gap = 22

        # ═══════════════════════════════════════
        # AUDIO SOURCE
        # ═══════════════════════════════════════
        self._section_labels.append(("AUDIO SOURCE", x, y - 4))
        y += 16

        # Build device list with type tags
        device_names = ["System Default"]
        device_indices = [-1]
        device_tags = [""]
        current_device_sel = 0

        for dev in self._audio_devices:
            short_name = dev['name']
            if len(short_name) > 32:
                short_name = short_name[:29] + "..."

            api = dev.get('hostapi', '')
            if api:
                display = f"{short_name} [{api}]"
            else:
                display = short_name

            device_names.append(display)
            device_indices.append(dev['index'])
            device_tags.append(dev.get('type', 'input'))

            if dev['index'] == self.settings.device_index:
                current_device_sel = len(device_names) - 1

        def on_device_selected(idx):
            self.settings.device_index = device_indices[idx]
            if self.on_device_change:
                self.on_device_change(self.settings.device_index)

        max_vis = min(8, len(device_names))
        self._device_dropdown = Dropdown(
            x, y, w, "Input Device",
            device_names,
            current_device_sel,
            on_device_selected,
            max_visible=max_vis,
            option_tags=device_tags
        )
        self.elements.append(self._device_dropdown)
        y += 55

        # Help button for system audio
        help_btn = Button(
            x, y, w, 28, "? How to capture system audio",
            lambda _: self._help_popup.toggle(self._loopback_help_text),
        )
        self.elements.append(help_btn)
        y += 40 + section_gap

        # ═══════════════════════════════════════
        # VISUALIZATION
        # ═══════════════════════════════════════
        self._section_labels.append(("VISUALIZATION", x, y - 4))
        y += 16

        self.elements.append(Dropdown(
            x, y, w, "Mode",
            VISUALIZATION_MODES,
            self.settings.visualization_mode,
            lambda v: setattr(self.settings, 'visualization_mode', v)
        ))
        y += 60

        theme_names = list(THEMES.keys())
        current_theme_idx = theme_names.index(self.settings.theme_name) \
            if self.settings.theme_name in theme_names else 0

        self.elements.append(Dropdown(
            x, y, w, "Color Theme",
            theme_names,
            current_theme_idx,
            lambda v: setattr(self.settings, 'theme_name', theme_names[v])
        ))
        y += 60 + section_gap

        # ═══════════════════════════════════════
        # AUDIO PROCESSING
        # ═══════════════════════════════════════
        self._section_labels.append(("AUDIO PROCESSING", x, y - 4))
        y += 16

        self.elements.append(Slider(
            x, y, w, "Bar Count", 8, 256, self.settings.bar_count,
            lambda v: setattr(self.settings, 'bar_count', int(v)), is_int=True
        ))
        y += slider_spacing

        self.elements.append(Slider(
            x, y, w, "Smoothing", 0.0, 0.95, self.settings.smoothing,
            lambda v: setattr(self.settings, 'smoothing', v)
        ))
        y += slider_spacing

        self.elements.append(Slider(
            x, y, w, "Sensitivity", 0.1, 5.0, self.settings.sensitivity,
            lambda v: setattr(self.settings, 'sensitivity', v)
        ))
        y += slider_spacing

        self.elements.append(Slider(
            x, y, w, "Min Freq (Hz)", 20, 2000, self.settings.min_frequency,
            lambda v: setattr(self.settings, 'min_frequency', int(v)), is_int=True
        ))
        y += slider_spacing

        self.elements.append(Slider(
            x, y, w, "Max Freq (Hz)", 2000, 20000, self.settings.max_frequency,
            lambda v: setattr(self.settings, 'max_frequency', int(v)), is_int=True
        ))
        y += slider_spacing + section_gap

        # ═══════════════════════════════════════
        # EFFECTS
        # ═══════════════════════════════════════
        self._section_labels.append(("EFFECTS", x, y - 4))
        y += 16

        self.elements.append(Slider(
            x, y, w, "Fade Speed", 1, 100, self.settings.fade_speed,
            lambda v: setattr(self.settings, 'fade_speed', int(v)), is_int=True
        ))
        y += slider_spacing

        self.elements.append(Slider(
            x, y, w, "Bar Width", 0.2, 1.0, self.settings.bar_width_ratio,
            lambda v: setattr(self.settings, 'bar_width_ratio', v)
        ))
        y += slider_spacing + 8

        btn_w = (w - 10) // 2
        btn_h = 32

        self.elements.append(Button(
            x, y, btn_w, btn_h, "Glow",
            lambda v: setattr(self.settings, 'glow', v),
            toggle=True, toggled=self.settings.glow
        ))
        self.elements.append(Button(
            x + btn_w + 10, y, btn_w, btn_h, "Fade Trail",
            lambda v: setattr(self.settings, 'fade_trail', v),
            toggle=True, toggled=self.settings.fade_trail
        ))
        y += btn_h + 8

        self.elements.append(Button(
            x, y, btn_w, btn_h, "Particles",
            lambda v: setattr(self.settings, 'particles_enabled', v),
            toggle=True, toggled=self.settings.particles_enabled
        ))
        self.elements.append(Button(
            x + btn_w + 10, y, btn_w, btn_h, "Show FPS",
            lambda v: setattr(self.settings, 'show_fps', v),
            toggle=True, toggled=self.settings.show_fps
        ))
        y += btn_h + section_gap + 8

        # ═══════════════════════════════════════
        # PRESETS
        # ═══════════════════════════════════════
        self._section_labels.append(("PRESETS", x, y - 4))
        y += 16

        self.elements.append(Button(
            x, y, btn_w, 34, "Save",
            lambda _: self.settings.save(),
            color_on=UI_SAVE_BTN, color_off=UI_SAVE_BTN
        ))
        self.elements.append(Button(
            x + btn_w + 10, y, btn_w, 34, "Load",
            lambda _: self._load_and_rebuild(),
            color_on=UI_LOAD_BTN, color_off=UI_LOAD_BTN
        ))

        self._total_height = y + 60

    def _load_and_rebuild(self):
        loaded = VisualizerSettings.load()
        for field_name in self.settings.__dataclass_fields__:
            setattr(self.settings, field_name, getattr(loaded, field_name))
        self._initialized = False

    def toggle(self):
        self.visible = not self.visible
        if self.visible:
            self._initialized = False
        else:
            self._help_popup.hide()

    def _close_all_dropdowns_except(self, keep: Optional[Dropdown] = None):
        for el in self.elements:
            if isinstance(el, Dropdown) and el is not keep:
                el.expanded = False

    def handle_event(self, event: pygame.event.Event) -> bool:
        if not self.visible:
            return False

        self._init_ui()

        # Help popup takes priority
        if self._help_popup.visible:
            if self._help_popup.handle_event(event):
                return True

        if event.type == pygame.MOUSEWHEEL:
            for el in self.elements:
                if isinstance(el, Dropdown) and el.expanded:
                    dd_rect = el._get_dropdown_rect()
                    if dd_rect.collidepoint(pygame.mouse.get_pos()):
                        el.handle_event(event)
                        return True
            self.scroll_y += event.y * 25
            self.scroll_y = min(0, self.scroll_y)
            return True

        # Expanded dropdowns first
        for el in reversed(self.elements):
            if isinstance(el, Dropdown) and el.expanded:
                if el.handle_event(event):
                    return True

        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
            clicked_dropdown = None
            for el in self.elements:
                if isinstance(el, Dropdown) and el.rect.collidepoint(event.pos):
                    clicked_dropdown = el
                    break
            self._close_all_dropdowns_except(clicked_dropdown)

        for element in reversed(self.elements):
            if isinstance(element, Dropdown) and element.expanded:
                continue
            if element.handle_event(event):
                return True

        if event.type == pygame.MOUSEBUTTONDOWN:
            panel_rect = pygame.Rect(0, 0, self.panel_width,
                                     pygame.display.get_surface().get_height())
            if panel_rect.collidepoint(event.pos):
                return True

        return False

    def draw(self, surface: pygame.Surface):
        if not self.visible:
            return

        self._init_ui()
        w = self.panel_width
        h = surface.get_height()

        # Solid background
        panel_rect = pygame.Rect(0, 0, w, h)
        pygame.draw.rect(surface, UI_BG, panel_rect)

        # Right border
        pygame.draw.line(surface, UI_PANEL_BORDER, (w - 1, 0), (w - 1, h), 2)

        # Title bar
        title_bar = pygame.Rect(0, 0, w, 52)
        pygame.draw.rect(surface, (12, 12, 24), title_bar)
        pygame.draw.line(surface, UI_PANEL_BORDER, (0, 52), (w, 52), 1)

        title = self.title_font.render("Settings", True, UI_TEXT_BRIGHT)
        surface.blit(title, (20, 15))

        hint = self.small_font.render("[Tab] close", True, UI_TEXT_DIM)
        surface.blit(hint, (w - hint.get_width() - 15, 19))

        # Section headers
        for label_text, lx, ly in self._section_labels:
            line_y = ly + 6
            pygame.draw.line(surface, UI_BORDER, (lx, line_y),
                             (lx + w - 40, line_y), 1)
            section_surf = self.small_font.render(label_text, True, UI_LABEL)
            text_bg_rect = section_surf.get_rect(topleft=(lx, ly - 6))
            text_bg_rect.inflate_ip(10, 4)
            pygame.draw.rect(surface, UI_BG, text_bg_rect)
            surface.blit(section_surf, (lx, ly - 6))

        # Elements base layer
        mouse_pos = pygame.mouse.get_pos()
        for element in self.elements:
            element.update(mouse_pos)
            element.draw(surface, self.small_font)

        # Dropdown overlays (on top)
        for element in self.elements:
            if isinstance(element, Dropdown):
                element.draw_overlay(surface, self.small_font)

        # Help popup (on top of everything)
        self._help_popup.draw(surface, self.small_font, self.title_font)