#ifndef UI_H
#define UI_H

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdbool.h>
#include "settings.h"
#include "audio.h"

typedef struct {
    bool visible;
    Settings *settings;
    AudioEngine *audio;

    TTF_Font *font;
    TTF_Font *font_small;
    TTF_Font *font_title;

    int panel_width;
    int scroll_y;
    int content_height;

    /* Interaction state */
    int active_slider;      /* -1 = none */
    int active_dropdown;    /* -1 = none */
    int dropdown_scroll;
    bool was_mouse_down;
    bool device_click_consumed;

    /* Device Category Filter */
    int device_category;    /* 0 = Microphones (Inputs), 1 = System Audio (Loopbacks) */

    /* Callback */
    void (*on_device_change)(int device_index, void *userdata);
    void *callback_userdata;
} UIPanel;

void ui_init(UIPanel *ui, Settings *settings, AudioEngine *audio);
void ui_destroy(UIPanel *ui);
void ui_toggle(UIPanel *ui);
bool ui_handle_event(UIPanel *ui, SDL_Event *event);
void ui_draw(UIPanel *ui, SDL_Renderer *renderer, int screen_w, int screen_h);

/* HUD (always visible) */
void ui_draw_hud(SDL_Renderer *renderer, TTF_Font *font, TTF_Font *font_small,
                 Settings *settings, float fps, float rms_level, bool audio_running,
                 int screen_w, int screen_h);

#endif