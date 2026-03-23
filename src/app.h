#ifndef APP_H
#define APP_H

#include <SDL.h>
#include <stdbool.h>
#include "settings.h"
#include "audio.h"
#include "fft.h"
#include "renderer.h"
#include "ui.h"

typedef struct {
    SDL_Window *window;
    SDL_Renderer *sdl_renderer;
    bool running;

    Settings settings;
    AudioEngine audio;
    FFTProcessor fft;
    Renderer renderer;
    UIPanel ui;

    float *audio_chunk;
    float *waveform;

    /* Timing */
    uint64_t perf_freq;
    uint64_t last_time;
    float fps;
    float frame_time;

    /* Help */
    bool show_help;
    float help_timer;
} App;

bool app_init(App *app);
void app_destroy(App *app);
void app_run(App *app);

#endif