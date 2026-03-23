#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_THEME_NAME 32
#define MAX_DEVICE_NAME 256
#define MAX_DEVICES 128
#define MAX_VIS_MODES 7
#define MAX_THEMES 6
#define SETTINGS_FILE "visualizer_settings.cfg"

typedef struct {
    uint8_t r, g, b;
} Color;

typedef struct {
    char name[MAX_THEME_NAME];
    Color primary;
    Color secondary;
    Color tertiary;
    Color background;
    Color text;
    Color accent;
} ColorTheme;

typedef struct {
    int index;
    char name[MAX_DEVICE_NAME];
    int channels;
    int sample_rate;
    bool is_loopback;
    bool is_input;
} AudioDeviceInfo;

typedef struct {
    /* Window */
    int window_width;
    int window_height;
    bool fullscreen;
    int fps_cap;

    /* Audio */
    int sample_rate;
    int chunk_size;
    int device_index; /* -1 = default */

    /* Visualization */
    int visualization_mode;
    int bar_count;
    float smoothing;
    float sensitivity;
    int min_frequency;
    int max_frequency;

    /* Effects */
    bool glow;
    bool fade_trail;
    int fade_speed;
    bool particles_enabled;

    /* Appearance */
    int theme_index;
    float bar_width_ratio;
    int border_radius;
    bool show_fps;
} Settings;

/* Visualization mode names */
extern const char *VIS_MODE_NAMES[MAX_VIS_MODES];
extern ColorTheme THEMES[MAX_THEMES];
extern int NUM_THEMES;

/* Functions */
void settings_init_defaults(Settings *s);
bool settings_save(const Settings *s, const char *filepath);
bool settings_load(Settings *s, const char *filepath);
ColorTheme *settings_get_theme(Settings *s);

/* Color helpers */
Color color_lerp(Color a, Color b, float t);
Color color_brightness(Color c, float brightness);

#endif