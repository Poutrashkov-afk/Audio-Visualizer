#include "settings.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h> 
#include <math.h>

const char *VIS_MODE_NAMES[MAX_VIS_MODES] = {
    "Bars",
    "Bars Mirrored",
    "Circular",
    "Wave",
    "Spectrum Line",
    "Particles",
    "Waterfall"
};

ColorTheme THEMES[MAX_THEMES] = {
    {
        .name = "Neon",
        .primary   = {0, 255, 255},
        .secondary = {255, 0, 255},
        .tertiary  = {255, 255, 0},
        .background = {5, 5, 15},
        .text      = {220, 220, 220},
        .accent    = {255, 255, 0}
    },
    {
        .name = "Ocean",
        .primary   = {0, 150, 255},
        .secondary = {0, 80, 180},
        .tertiary  = {100, 200, 255},
        .background = {5, 10, 30},
        .text      = {220, 220, 220},
        .accent    = {0, 200, 255}
    },
    {
        .name = "Fire",
        .primary   = {255, 100, 0},
        .secondary = {255, 50, 0},
        .tertiary  = {255, 200, 0},
        .background = {20, 5, 5},
        .text      = {220, 220, 220},
        .accent    = {255, 150, 0}
    },
    {
        .name = "Forest",
        .primary   = {0, 200, 80},
        .secondary = {0, 150, 50},
        .tertiary  = {100, 255, 100},
        .background = {5, 15, 10},
        .text      = {220, 220, 220},
        .accent    = {0, 255, 80}
    },
    {
        .name = "Synthwave",
        .primary   = {255, 0, 200},
        .secondary = {100, 0, 255},
        .tertiary  = {0, 200, 255},
        .background = {15, 0, 30},
        .text      = {220, 220, 220},
        .accent    = {255, 0, 200}
    },
    {
        .name = "Monochrome",
        .primary   = {200, 200, 200},
        .secondary = {150, 150, 150},
        .tertiary  = {255, 255, 255},
        .background = {10, 10, 10},
        .text      = {220, 220, 220},
        .accent    = {255, 255, 255}
    }
};

int NUM_THEMES = 6;

void settings_init_defaults(Settings *s) {
    s->window_width = 1280;
    s->window_height = 720;
    s->fullscreen = false;
    s->fps_cap = 60;

    s->sample_rate = 44100;
    s->chunk_size = 2048;
    s->device_index = -1;

    s->visualization_mode = 0;
    s->bar_count = 64;
    s->smoothing = 0.3f;
    s->sensitivity = 1.5f;
    s->min_frequency = 20;
    s->max_frequency = 16000;

    s->glow = true;
    s->fade_trail = true;
    s->fade_speed = 50;
    s->particles_enabled = true;

    s->theme_index = 0;
    s->bar_width_ratio = 0.8f;
    s->border_radius = 3;
    s->show_fps = true;
}

bool settings_save(const Settings *s, const char *filepath) {
    FILE *f = fopen(filepath, "wb");
    if (!f) return false;

    fprintf(f, "window_width=%d\n", s->window_width);
    fprintf(f, "window_height=%d\n", s->window_height);
    fprintf(f, "fullscreen=%d\n", s->fullscreen);
    fprintf(f, "fps_cap=%d\n", s->fps_cap);
    fprintf(f, "sample_rate=%d\n", s->sample_rate);
    fprintf(f, "chunk_size=%d\n", s->chunk_size);
    fprintf(f, "device_index=%d\n", s->device_index);
    fprintf(f, "visualization_mode=%d\n", s->visualization_mode);
    fprintf(f, "bar_count=%d\n", s->bar_count);
    fprintf(f, "smoothing=%.4f\n", s->smoothing);
    fprintf(f, "sensitivity=%.4f\n", s->sensitivity);
    fprintf(f, "min_frequency=%d\n", s->min_frequency);
    fprintf(f, "max_frequency=%d\n", s->max_frequency);
    fprintf(f, "glow=%d\n", s->glow);
    fprintf(f, "fade_trail=%d\n", s->fade_trail);
    fprintf(f, "fade_speed=%d\n", s->fade_speed);
    fprintf(f, "particles_enabled=%d\n", s->particles_enabled);
    fprintf(f, "theme_index=%d\n", s->theme_index);
    fprintf(f, "bar_width_ratio=%.4f\n", s->bar_width_ratio);
    fprintf(f, "border_radius=%d\n", s->border_radius);
    fprintf(f, "show_fps=%d\n", s->show_fps);

    fclose(f);
    return true;
}

bool settings_load(Settings *s, const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) {
        settings_init_defaults(s);
        return false;
    }

    settings_init_defaults(s);

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char key[64];
        char val[128];
        if (sscanf(line, "%63[^=]=%127s", key, val) == 2) {
            if (strcmp(key, "window_width") == 0) s->window_width = atoi(val);
            else if (strcmp(key, "window_height") == 0) s->window_height = atoi(val);
            else if (strcmp(key, "fullscreen") == 0) s->fullscreen = atoi(val);
            else if (strcmp(key, "fps_cap") == 0) s->fps_cap = atoi(val);
            else if (strcmp(key, "sample_rate") == 0) s->sample_rate = atoi(val);
            else if (strcmp(key, "chunk_size") == 0) s->chunk_size = atoi(val);
            else if (strcmp(key, "device_index") == 0) s->device_index = atoi(val);
            else if (strcmp(key, "visualization_mode") == 0) s->visualization_mode = atoi(val);
            else if (strcmp(key, "bar_count") == 0) s->bar_count = atoi(val);
            else if (strcmp(key, "smoothing") == 0) s->smoothing = (float)atof(val);
            else if (strcmp(key, "sensitivity") == 0) s->sensitivity = (float)atof(val);
            else if (strcmp(key, "min_frequency") == 0) s->min_frequency = atoi(val);
            else if (strcmp(key, "max_frequency") == 0) s->max_frequency = atoi(val);
            else if (strcmp(key, "glow") == 0) s->glow = atoi(val);
            else if (strcmp(key, "fade_trail") == 0) s->fade_trail = atoi(val);
            else if (strcmp(key, "fade_speed") == 0) s->fade_speed = atoi(val);
            else if (strcmp(key, "particles_enabled") == 0) s->particles_enabled = atoi(val);
            else if (strcmp(key, "theme_index") == 0) s->theme_index = atoi(val);
            else if (strcmp(key, "bar_width_ratio") == 0) s->bar_width_ratio = (float)atof(val);
            else if (strcmp(key, "border_radius") == 0) s->border_radius = atoi(val);
            else if (strcmp(key, "show_fps") == 0) s->show_fps = atoi(val);
        }
    }

    fclose(f);
    return true;
}

ColorTheme *settings_get_theme(Settings *s) {
    if (s->theme_index < 0 || s->theme_index >= NUM_THEMES)
        s->theme_index = 0;
    return &THEMES[s->theme_index];
}

Color color_lerp(Color a, Color b, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    Color result;
    result.r = (uint8_t)(a.r + (b.r - a.r) * t);
    result.g = (uint8_t)(a.g + (b.g - a.g) * t);
    result.b = (uint8_t)(a.b + (b.b - a.b) * t);
    return result;
}

Color color_brightness(Color c, float brightness) {
    if (brightness < 0.0f) brightness = 0.0f;
    if (brightness > 1.0f) brightness = 1.0f;
    Color result;
    result.r = (uint8_t)(c.r * brightness);
    result.g = (uint8_t)(c.g * brightness);
    result.b = (uint8_t)(c.b * brightness);
    return result;
}